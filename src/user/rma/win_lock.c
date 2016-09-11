/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"
#include "cspu_rma_sync.h"

/* Lock ghost processes for a given target.
 * It is called by both WIN_LOCK and WIN_LOCK_ALL (only lock-exist mode). */
int CSP_win_target_lock(int lock_type, int assert, int target_rank, CSP_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_win_target_t *target = NULL;
    int user_rank;
    int k;

    target = &(ug_win->targets[target_rank]);
    PMPI_Comm_rank(ug_win->user_comm, &user_rank);

    /* Lock every ghost on every window for each target.
     * Because a ghost may be used on any window of this process for runtime
     * load balancing. */
    for (k = 0; k < CSP_ENV.num_g; k++) {
        int target_g_rank_in_ug = 0;
        int g_lock_type = MPI_LOCK_SHARED;
        int g_assert = MPI_MODE_NOCHECK;

        /* Only the main ghost requires permission check, other ghosts should just get
         * shared & nocheck lock. Otherwise deadlock may happen in binding-free stage,
         * if MPI implementation always acquires remote lock in lock call even if no
         * operation issued on that target. */
        if (target->main_g_off == k) {
            g_lock_type = lock_type;
            g_assert = assert;
        }

        target_g_rank_in_ug = target->g_ranks_in_ug[k];

        CSP_DBG_PRINT(" lock(ghost(%d), ug_win 0x%x, lock=%s, assert=%s), instead of "
                      "target rank %d\n", target_g_rank_in_ug, target->ug_win,
                      CSP_get_lock_type_name(g_lock_type), CSP_get_assert_name(g_assert),
                      target_rank);
        mpi_errno = PMPI_Win_lock(g_lock_type, target_g_rank_in_ug, g_assert, target->ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    if (target_rank == user_rank) {
        /* Local lock processing.
         *
         * - Force lock.
         * We need grant local lock (self-target) before return. However, since
         * the actual locked processes are remote ghost processes, whose locks may be
         * delayed in most MPI implementation, we need a flush to force the lock to be
         * granted. This operation could be ignored if (1) received user hint
         * no_local_load_store, or (2) no concurrent epoch (MODE_NOCHECK).
         *
         * - Lock self.
         * This step is for memory consistency on local load/store operations.
         * But it can be skipped if received user hint no_local_load_store. */
        if (!ug_win->info_args.no_local_load_store
            && !(ug_win->targets[user_rank].remote_lock_assert & MPI_MODE_NOCHECK)) {
            mpi_errno = CSP_win_grant_local_lock(user_rank, ug_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
        if (!ug_win->info_args.no_local_load_store && !ug_win->is_self_locked /*already locked */) {
            mpi_errno = CSP_win_lock_self(ug_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_lock(int lock_type, int target_rank, int assert, MPI_Win win)
{
    CSP_win_t *ug_win;
    CSP_win_target_t *target;
    int mpi_errno = MPI_SUCCESS;
    int user_rank;

    CSP_DBG_PRINT_FCNAME();

    CSP_fetch_ug_win_from_cache(win, ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_lock(lock_type, target_rank, assert, win);
    }

    /* casper window starts */

    if (target_rank == MPI_PROC_NULL)
        goto fn_exit;

    CSP_assert((ug_win->info_args.epochs_used & CSP_EPOCH_LOCK));

    if (ug_win->epoch_stat == CSP_WIN_EPOCH_FENCE)
        ug_win->is_self_locked = 0;     /* because we cannot reset it in previous FENCE. */

    target = &(ug_win->targets[target_rank]);
    PMPI_Comm_rank(ug_win->user_comm, &user_rank);

#ifdef CSP_ENABLE_EPOCH_STAT_CHECK
    /* Check access epoch status.
     * We do not require closed FENCE epoch, because we don't know whether
     * the previous FENCE is closed or not.*/
    if (ug_win->epoch_stat == CSP_WIN_EPOCH_LOCK_ALL) {
        CSP_ERR_PRINT("Wrong synchronization call! "
                      "Previous LOCK_ALL epoch is still open in %s\n", __FUNCTION__);
        mpi_errno = -1;
        goto fn_fail;
    }

    /* Check per-target access epoch status. */
    if (ug_win->epoch_stat == CSP_WIN_EPOCH_PER_TARGET && target->epoch_stat != CSP_TARGET_NO_EPOCH) {
        CSP_ERR_PRINT("Wrong synchronization call! "
                      "Previous %s epoch on target %d is still open in %s\n",
                      target->epoch_stat == CSP_TARGET_EPOCH_LOCK ? "LOCK" : "PSCW",
                      target_rank, __FUNCTION__);
        mpi_errno = -1;
        goto fn_fail;
    }

    CSP_assert(user_rank != target_rank || ug_win->is_self_locked == 0);
#endif

    target->remote_lock_assert = assert;
    CSP_DBG_PRINT(" lock(%d), MPI_MODE_NOCHECK %d(assert %d)\n", target_rank,
                  (assert & MPI_MODE_NOCHECK) != 0, assert);

#ifdef CSP_ENABLE_SYNC_ALL_OPT
    CSP_DBG_PRINT(" lock_all(ug_win 0x%x), instead of target rank %d\n", target->ug_win,
                  target_rank);
    mpi_errno = PMPI_Win_lock_all(assert, target->ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (user_rank == target_rank)
        ug_win->is_self_locked = 1;
#else
    mpi_errno = CSP_win_target_lock(lock_type, assert, target_rank, ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
#endif

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    target->main_lock_stat = CSP_MAIN_LOCK_RESET;
    CSP_reset_target_opload(target_rank, ug_win);
#endif


    /* Indicate epoch status.
     * later operations issued to the target will be redirected to ug_wins.*/
    target->epoch_stat = CSP_TARGET_EPOCH_LOCK;
    ug_win->epoch_stat = CSP_WIN_EPOCH_PER_TARGET;
    ug_win->lock_counter++;

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

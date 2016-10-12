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
int CSPU_win_target_lock(int lock_type, int assert, int target_rank, CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    CSPU_win_target_t *target = NULL;
    int user_rank;
    int k;

    target = &(ug_win->targets[target_rank]);
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->user_comm, &user_rank));

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
                      CSPU_GET_LOCK_TYPE_NAME(g_lock_type), CSPU_GET_ASSERT_NAME(g_assert),
                      target_rank);
        CSP_CALLMPI(JUMP, PMPI_Win_lock(g_lock_type, target_g_rank_in_ug, g_assert,
                                        target->ug_win));
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
            mpi_errno = CSPU_win_grant_local_lock(user_rank, ug_win);
            CSP_CHKMPIFAIL_JUMP(mpi_errno);
        }
        if (!ug_win->info_args.no_local_load_store && !ug_win->is_self_locked /*already locked */) {
            mpi_errno = CSPU_win_lock_self(ug_win);
            CSP_CHKMPIFAIL_JUMP(mpi_errno);
        }
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_lock(int lock_type, int target_rank, int assert, MPI_Win win)
{
    CSPU_win_t *ug_win;
    CSPU_win_target_t *target;
    int mpi_errno = MPI_SUCCESS;
    int user_rank;

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();
    CSPU_ERRHAN_EXTOBJ_LOCAL_DCL();
    CSPU_WIN_ERRHAN_SET_EXTOBJ();

    CSPU_fetch_ug_win_from_cache(win, &ug_win);

    if (ug_win == NULL) {
        /* normal window */
        CSPU_ERRHAN_RESET_EXTOBJ();     /* reset before calling original MPI */
        return PMPI_Win_lock(lock_type, target_rank, assert, win);
    }

    CSPU_THREAD_ENTER_OBJ_CS(ug_win);

    if (target_rank == MPI_PROC_NULL)
        goto fn_exit;

    CSP_ASSERT((ug_win->info_args.epochs_used & CSP_EPOCH_LOCK));

    if (ug_win->epoch_stat == CSPU_WIN_EPOCH_FENCE)
        ug_win->is_self_locked = 0;     /* because we cannot reset it in previous FENCE. */

    CSPU_TARGET_CHECK_RANK(target_rank, ug_win);

    target = &(ug_win->targets[target_rank]);
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->user_comm, &user_rank));

#ifdef CSP_ENABLE_RMA_ERR_CHECK
    /* Check access epoch status.
     * We do not require closed FENCE epoch, because we don't know whether
     * the previous FENCE is closed or not.*/
    if (ug_win->epoch_stat == CSPU_WIN_EPOCH_LOCK_ALL) {
        CSP_msg_print(CSP_MSG_ERROR, "Wrong synchronization call! "
                      "Previous LOCK_ALL access epoch is still open in %s\n", __FUNCTION__);
        mpi_errno = MPI_ERR_RMA_SYNC;
        goto fn_fail;
    }

    /* Check per-target access epoch status. */
    if (ug_win->epoch_stat == CSPU_WIN_EPOCH_PER_TARGET &&
        target->epoch_stat != CSPU_TARGET_NO_EPOCH) {
        CSP_msg_print(CSP_MSG_ERROR, "Wrong synchronization call! "
                      "Previous %s access epoch on target %d is still open in %s\n",
                      CSPU_TARGET_GET_EPOCH_STAT_NAME(target, ug_win), target_rank, __FUNCTION__);
        mpi_errno = MPI_ERR_RMA_SYNC;
        goto fn_fail;
    }
#endif

    CSP_ASSERT(user_rank != target_rank || ug_win->is_self_locked == 0);

    target->remote_lock_assert = assert;
    CSP_DBG_PRINT(" lock(%d), MPI_MODE_NOCHECK %d(assert %d)\n", target_rank,
                  (assert & MPI_MODE_NOCHECK) != 0, assert);

#ifdef CSP_ENABLE_SYNC_ALL_OPT
    CSP_DBG_PRINT(" lock_all(ug_win 0x%x), instead of target rank %d\n", target->ug_win,
                  target_rank);
    CSP_CALLMPI(JUMP, PMPI_Win_lock_all(assert, target->ug_win));

    if (user_rank == target_rank)
        ug_win->is_self_locked = 1;
#else
    mpi_errno = CSPU_win_target_lock(lock_type, assert, target_rank, ug_win);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);
#endif

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    target->main_lock_stat = CSPU_MAIN_LOCK_RESET;
    CSPU_reset_target_opload(target_rank, ug_win);
#endif


    /* Indicate epoch status.
     * later operations issued to the target will be redirected to ug_wins.*/
    target->epoch_stat = CSPU_TARGET_EPOCH_LOCK;
    ug_win->epoch_stat = CSPU_WIN_EPOCH_PER_TARGET;
    ug_win->lock_counter++;

  fn_exit:
    CSPU_THREAD_EXIT_OBJ_CS(ug_win);
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before return */
    return mpi_errno;

  fn_fail:
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before error handling */
    CSPU_WIN_ERRHANLDING(win, &mpi_errno);
    goto fn_exit;
}

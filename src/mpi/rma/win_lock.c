/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "csp.h"

#ifdef CSP_ENABLE_LOCAL_LOCK_OPT
static inline int CSP_win_lock_self_impl(CSP_win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank;

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);

#ifdef CSP_ENABLE_SYNC_ALL_OPT
    /* lockall already locked window for local target */
#else
    CSP_DBG_PRINT("[%d]lock self(%d, local win 0x%x)\n", user_rank,
                  ug_win->my_rank_in_ug_comm, ug_win->my_ug_win);
    mpi_errno = PMPI_Win_lock(MPI_LOCK_SHARED, ug_win->my_rank_in_ug_comm,
                              MPI_MODE_NOCHECK, ug_win->my_ug_win);
    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;
#endif

    ug_win->is_self_locked = 1;
    return mpi_errno;
}
#endif

int MPI_Win_lock(int lock_type, int target_rank, int assert, MPI_Win win)
{
    CSP_win *ug_win;
    CSP_win_target *target;
    int mpi_errno = MPI_SUCCESS;
    int user_rank;
    int k;
    int is_local_lock_granted ATTRIBUTE((unused));

    CSP_DBG_PRINT_FCNAME();

    CSP_fetch_ug_win_from_cache(win, ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_lock(lock_type, target_rank, assert, win);
    }

    /* casper window starts */

    if (target_rank == MPI_PROC_NULL)
        goto fn_exit;

    CSP_assert((ug_win->info_args.epoch_type & CSP_EPOCH_LOCK) ||
               (ug_win->info_args.epoch_type & CSP_EPOCH_LOCK_ALL));

    target = &(ug_win->targets[target_rank]);

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
#endif

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    target->remote_lock_assert = assert;
    CSP_DBG_PRINT("[%d]lock(%d), MPI_MODE_NOCHECK %d(assert %d)\n", user_rank,
                  target_rank, (assert & MPI_MODE_NOCHECK) != 0, assert);

    /* Lock Ghost processes in corresponding ug-window of target process. */
#ifdef CSP_ENABLE_SYNC_ALL_OPT
    /* Optimization for MPI implementations that have optimized lock_all.
     * However, user should be noted that, if MPI implementation issues lock messages
     * for every target even if it does not have any operation, this optimization
     * could lose performance and even lose asynchronous! */

    CSP_DBG_PRINT("[%d]lock_all(ug_win 0x%x), instead of target rank %d\n",
                  user_rank, target->ug_win, target_rank);
    mpi_errno = PMPI_Win_lock_all(assert, target->ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
#else
    /* Lock every ghost on every window.
     * Note that a ghost may be used on any window of this process for runtime
     * load balancing whether it is binded to that segment or not. */
    for (k = 0; k < CSP_ENV.num_g; k++) {
        int target_g_rank_in_ug = target->g_ranks_in_ug[k];

        CSP_DBG_PRINT("[%d]lock(Ghost(%d), ug_wins 0x%x), instead of "
                      "target rank %d\n", user_rank, target_g_rank_in_ug,
                      target->ug_win, target_rank);

        mpi_errno = PMPI_Win_lock(lock_type, target_g_rank_in_ug, assert, target->ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
#endif

    ug_win->is_self_locked = 0;

    if (user_rank == target_rank) {
        is_local_lock_granted = 0;

        /* If target is itself, we need grant this lock before return.
         * However, the actual locked processes are the Ghosts whose locks may be delayed by
         * most MPI implementation, thus we need a flush to force the lock to be granted.
         *
         * For performance reason, this operation is ignored if meet at least one of following conditions:
         * 1. if user passed information that this process will not do local load/store on this window.
         * 2. if user passed information that there is no concurrent epochs.
         */
        if (!ug_win->info_args.no_local_load_store &&
            !(target->remote_lock_assert & MPI_MODE_NOCHECK)) {
            mpi_errno = CSP_win_grant_local_lock(user_rank, ug_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
            is_local_lock_granted = 1;
        }

#ifdef CSP_ENABLE_LOCAL_LOCK_OPT
        /* Lock local rank so that operations can be executed through local target.
         * 1. Need grant lock on ghost in advance due to permission check,
         * OR
         * 2. there is no concurrent epochs, hence it is safe to get local lock.*/
        if (is_local_lock_granted || (target->remote_lock_assert & MPI_MODE_NOCHECK)) {
            mpi_errno = CSP_win_lock_self_impl(ug_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
#endif
    }

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    int j;
    for (j = 0; j < target->num_segs; j++) {
        target->segs[j].main_lock_stat = CSP_MAIN_LOCK_RESET;
        CSP_reset_target_opload(target_rank, ug_win);
    }
#endif


    /* Indicate epoch status.
     * later operations issued to the target will be redirected to ug_wins.*/
    target->epoch_stat = CSP_TARGET_EPOCH_LOCK;
    ug_win->epoch_stat = CSP_WIN_EPOCH_PER_TARGET;
    ug_win->lock_counter++;

    /* TODO: All the operations which we have not wrapped up will be failed, because they
     * are issued to user window. We need wrap up all operations.
     */

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

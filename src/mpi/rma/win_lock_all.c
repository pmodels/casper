/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "csp.h"

#ifdef CSP_ENABLE_LOCAL_LOCK_OPT
static inline int CSP_Win_lock_self_impl(CSP_Win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;

#ifdef CSP_ENABLE_SYNC_ALL_OPT
    /* lockall already locked window for local target */
#else
    int user_rank;
    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
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

static int CSP_Win_mixed_lock_all_impl(int assert, CSP_Win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs;
    int i, k;
    int is_local_lock_granted ATTRIBUTE((unused));

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);

#ifdef CSP_ENABLE_SYNC_ALL_OPT

    /* Optimization for MPI implementations that have optimized lock_all.
     * However, user should be noted that, if MPI implementation issues lock messages
     * for every target even if it does not have any operation, this optimization
     * could lose performance and even lose asynchronous! */
    for (i = 0; i < ug_win->num_ug_wins; i++) {
        CSP_DBG_PRINT("[%d]lock_all(ug_win 0x%x)\n", user_rank, ug_win->ug_wins[i]);
        mpi_errno = PMPI_Win_lock_all(assert, ug_win->ug_wins[i]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
#else

    /* Lock every ghost on every window for each target.
     * Note that a ghost may be used on any window of this process for runtime
     * load balancing whether it is binded to that segment or not. */
    for (i = 0; i < user_nprocs; i++) {
        for (k = 0; k < CSP_ENV.num_g; k++) {
            int target_g_rank_in_ug = ug_win->targets[i].g_ranks_in_ug[k];

            CSP_DBG_PRINT("[%d]lock(Ghost(%d), ug_win 0x%x), instead of "
                          "target rank %d\n", user_rank, target_g_rank_in_ug,
                          ug_win->targets[i].ug_win, i);
            mpi_errno = PMPI_Win_lock(MPI_LOCK_SHARED, target_g_rank_in_ug, assert,
                                      ug_win->targets[i].ug_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
    }
#endif

    is_local_lock_granted = 0;
    if (!ug_win->info_args.no_local_load_store &&
        !(ug_win->targets[user_rank].remote_lock_assert & MPI_MODE_NOCHECK)) {
        /* We need grant the local lock (self-target) before return.
         * However, the actual locked processes are the Ghosts whose locks may be delayed by
         * most MPI implementation, thus we need a flush to force the lock to be granted on ghost 0
         * who is the one actually controls the locks.
         *
         * For performance reason, this operation is ignored if meet at least one of following conditions:
         * 1. if user passed information that this process will not do local load/store on this window.
         * 2. if user passed information that there is no concurrent epochs.
         */
        mpi_errno = CSP_Win_grant_local_lock(user_rank, ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        is_local_lock_granted = 1;
    }

    ug_win->is_self_locked = 0;
#ifdef CSP_ENABLE_LOCAL_LOCK_OPT
    /* Lock local rank so that operations can be executed through local target.
     * 1. Need grant lock on ghost in advance due to permission check,
     * OR
     * 2. there is no concurrent epochs, hence it is safe to get local lock.*/
    if (is_local_lock_granted || (ug_win->targets[user_rank].remote_lock_assert & MPI_MODE_NOCHECK)) {
        mpi_errno = CSP_Win_lock_self_impl(ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
#endif

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_lock_all(int assert, MPI_Win win)
{
    CSP_Win *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs;
    int i;

    CSP_DBG_PRINT_FCNAME();

    CSP_Fetch_ug_win_from_cache(win, ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_lock_all(assert, win);
    }

    /* casper window starts */

    CSP_Assert((ug_win->info_args.epoch_type & CSP_EPOCH_LOCK) ||
               (ug_win->info_args.epoch_type & CSP_EPOCH_LOCK_ALL));

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);

    for (i = 0; i < user_nprocs; i++) {
        ug_win->targets[i].remote_lock_assert = assert;
    }

    CSP_DBG_PRINT("[%d]lock_all, MPI_MODE_NOCHECK %d(assert %d)\n", user_rank,
                  (assert & MPI_MODE_NOCHECK) != 0, assert);

    if (!(ug_win->info_args.epoch_type & CSP_EPOCH_LOCK)) {

        /* In lock_all only epoch, lock all ghosts on the single window. */

#ifdef CSP_ENABLE_SYNC_ALL_OPT

        /* Optimization for MPI implementations that have optimized lock_all.
         * However, user should be noted that, if MPI implementation issues lock messages
         * for every target even if it does not have any operation, this optimization
         * could lose performance and even lose asynchronous! */
        CSP_DBG_PRINT("[%d]lock_all(ug_win 0x%x)\n", user_rank, ug_win->ug_wins[0]);
        mpi_errno = PMPI_Win_lock_all(assert, ug_win->ug_wins[0]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
#else
        mpi_errno = PMPI_Win_lock_all(assert, ug_win->ug_wins[0]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
#if 0   /* segmentation fault */
        for (i = 0; i < ug_win->num_g_ranks_in_ug; i++) {
            mpi_errno = PMPI_Win_lock(MPI_LOCK_SHARED, ug_win->g_ranks_in_ug[i],
                                      assert, ug_win->ug_wins[0]);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
#endif
#endif

        ug_win->is_self_locked = 0;
#ifdef CSP_ENABLE_LOCAL_LOCK_OPT
#if 0   /* workaround of lock_all */
        /* Do not need grant lock before lock local target, because only shared lock
         * in current epoch. */
        mpi_errno = CSP_Win_lock_self_impl(ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
#else
        ug_win->is_self_locked = 1;
#endif
#endif
    }
    else {

        /* In lock_all/lock mixed epoch, separate windows are bound with each target. */
        mpi_errno = CSP_Win_mixed_lock_all_impl(assert, ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    int j;
    for (i = 0; i < user_nprocs; i++) {
        for (j = 0; j < ug_win->targets[i].num_segs; j++) {
            ug_win->targets[i].segs[j].main_lock_stat = CSP_MAIN_LOCK_RESET;

            CSP_Reset_win_target_load_opt(i, ug_win);
        }
    }
#endif

    /* Indicate epoch status, later operations will be redirected to ug_wins
     * until lock/lockall counters decrease to 0 .*/
    ug_win->epoch_stat = CSP_WIN_EPOCH_LOCK;
    ug_win->lockall_counter++;

    /* TODO: All the operations which we have not wrapped up will be failed, because they
     * are issued to user window. We need wrap up all operations.
     */

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

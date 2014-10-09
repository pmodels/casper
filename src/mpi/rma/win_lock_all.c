#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"
static inline int MTCORE_Win_lock_self_impl(MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;

#ifdef MTCORE_ENABLE_SYNC_ALL_OPT
    /* lockall already locked window for local target */
#else
    int user_rank;
    PMPI_Comm_rank(uh_win->user_comm, &user_rank);
    MTCORE_DBG_PRINT("[%d]lock self(%d, local win 0x%x)\n", user_rank,
                     uh_win->my_rank_in_uh_comm, uh_win->my_uh_win);

    mpi_errno = PMPI_Win_lock(MPI_LOCK_SHARED, uh_win->my_rank_in_uh_comm,
                              MPI_MODE_NOCHECK, uh_win->my_uh_win);
    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;
#endif

    uh_win->is_self_locked = 1;
    return mpi_errno;
}

static int MTCORE_Win_mixed_lock_all_impl(int assert, MPI_Win win, MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs;
    int i, j, k;

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);
    PMPI_Comm_size(uh_win->user_comm, &user_nprocs);

#ifdef MTCORE_ENABLE_SYNC_ALL_OPT

    /* Optimization for MPI implementations that have optimized lock_all.
     * However, user should be noted that, if MPI implementation issues lock messages
     * for every target even if it does not have any operation, this optimization
     * could lose performance and even lose asynchronous! */
    for (i = 0; i < uh_win->num_uh_wins; i++) {
        MTCORE_DBG_PRINT("[%d]lock_all(uh_win 0x%x)\n", user_rank, uh_win->uh_wins[i]);
        mpi_errno = PMPI_Win_lock_all(assert, uh_win->uh_wins[i]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
#else

    /* Lock every helper on every window for each target.
     * Note that a helper may be used on any window of this process for runtime
     * load balancing whether it is binded to that segment or not. */
    for (i = 0; i < user_nprocs; i++) {
        j = 0;
        for (k = 0; k < MTCORE_ENV.num_h; k++) {
            int target_h_rank_in_uh = uh_win->targets[i].h_ranks_in_uh[k];

            MTCORE_DBG_PRINT("[%d]lock(Helper(%d), uh_win 0x%x), instead of "
                             "target rank %d\n", user_rank, target_h_rank_in_uh,
                             uh_win->targets[i].uh_win, i);
            mpi_errno = PMPI_Win_lock(MPI_LOCK_SHARED, target_h_rank_in_uh, assert,
                                      uh_win->targets[i].uh_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
    }
#endif

    int is_local_lock_granted = 0;
    if (!uh_win->info_args.no_local_load_store &&
        !(uh_win->targets[user_rank].remote_lock_assert & MPI_MODE_NOCHECK)) {
        /* We need grant the local lock (self-target) before return.
         * However, the actual locked processes are the Helpers whose locks may be delayed by
         * most MPI implementation, thus we need a flush to force the lock to be granted on helper 0
         * who is the one actually controls the locks.
         *
         * For performance reason, this operation is ignored if meet at least one of following conditions:
         * 1. if user passed information that this process will not do local load/store on this window.
         * 2. if user passed information that there is no concurrent epochs.
         */
        mpi_errno = MTCORE_Win_grant_local_lock(user_rank, MPI_LOCK_SHARED, 0, uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        is_local_lock_granted = 1;
    }

    uh_win->is_self_locked = 0;
#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
    /* Lock local rank so that operations can be executed through local target.
     * 1. Need grant lock on helper in advance due to permission check,
     * OR
     * 2. there is no concurrent epochs, hence it is safe to get local lock.*/
    if (is_local_lock_granted || (uh_win->targets[user_rank].remote_lock_assert & MPI_MODE_NOCHECK)) {
        mpi_errno = MTCORE_Win_lock_self_impl(uh_win);
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
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs;
    int i, j, k;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    if (uh_win == NULL) {
        /* normal window */
        return PMPI_Win_lock_all(assert, win);
    }

    /* mtcore window starts */

    MTCORE_Assert((uh_win->info_args.epoch_type & MTCORE_EPOCH_LOCK) ||
                  (uh_win->info_args.epoch_type & MTCORE_EPOCH_LOCK_ALL));

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);
    PMPI_Comm_size(uh_win->user_comm, &user_nprocs);

    for (i = 0; i < user_nprocs; i++) {
        uh_win->targets[i].remote_lock_assert = assert;
    }

    MTCORE_DBG_PRINT("[%d]lock_all, MPI_MODE_NOCHECK %d(assert %d)\n", user_rank,
                     (assert & MPI_MODE_NOCHECK) != 0, assert);

    if (!(uh_win->info_args.epoch_type & MTCORE_EPOCH_LOCK)) {

        /* In lock_all only epoch, lock all helpers on the single window. */

#ifdef MTCORE_ENABLE_SYNC_ALL_OPT

        /* Optimization for MPI implementations that have optimized lock_all.
         * However, user should be noted that, if MPI implementation issues lock messages
         * for every target even if it does not have any operation, this optimization
         * could lose performance and even lose asynchronous! */
        MTCORE_DBG_PRINT("[%d]lock_all(uh_win 0x%x)\n", user_rank, uh_win->uh_wins[0]);
        mpi_errno = PMPI_Win_lock_all(assert, uh_win->uh_wins[0]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
#else
        mpi_errno = PMPI_Win_lock_all(assert, uh_win->uh_wins[0]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
#if 0
        for (i = 0; i < uh_win->num_h_ranks_in_uh; i++) {
            mpi_errno = PMPI_Win_lock(MPI_LOCK_SHARED, uh_win->h_ranks_in_uh[i],
                                      assert, uh_win->uh_wins[0]);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
#endif
#endif

        uh_win->is_self_locked = 0;
#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
        /* Do not need grant lock before lock local target, because only shared lock
         * in current epoch. */
        mpi_errno = MTCORE_Win_lock_self_impl(uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
#endif
    }
    else {

        /* In lock_all/lock mixed epoch, separate windows are bound with each target. */
        mpi_errno = MTCORE_Win_mixed_lock_all_impl(assert, win, uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

#if defined(MTCORE_ENABLE_RUNTIME_LOAD_OPT)
    for (i = 0; i < user_nprocs; i++) {
        for (j = 0; j < uh_win->targets[i].num_segs; j++) {
            uh_win->targets[i].segs[j].main_lock_stat = MTCORE_MAIN_LOCK_RESET;

            MTCORE_Reset_win_target_load_opt(i, uh_win);
        }
    }
#endif

    /* Indicate epoch status, later operations will be redirected to uh_wins
     * until lock/lockall counters decrease to 0 .*/
    uh_win->epoch_stat = MTCORE_WIN_EPOCH_LOCK;
    uh_win->lockall_counter++;

    /* TODO: All the operations which we have not wrapped up will be failed, because they
     * are issued to user window. We need wrap up all operations.
     */

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

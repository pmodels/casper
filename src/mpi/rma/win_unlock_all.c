#include <stdio.h>
#include <stdlib.h>
#include "csp.h"

static inline int CSP_Win_unlock_self_impl(CSP_Win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;

#ifdef CSP_ENABLE_SYNC_ALL_OPT
    /* unlockall already released window for local target */
#else
    int user_rank;
    PMPI_Comm_rank(ug_win->user_comm, &user_rank);

    if (ug_win->is_self_locked) {
        /* We need also release the lock of local rank */

        CSP_DBG_PRINT("[%d]unlock self(%d, local win 0x%x)\n", user_rank,
                         ug_win->my_rank_in_ug_comm, ug_win->my_ug_win);
        mpi_errno = PMPI_Win_unlock(ug_win->my_rank_in_ug_comm, ug_win->my_ug_win);
        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;
    }
#endif

    ug_win->is_self_locked = 0;
    return mpi_errno;
}

static int CSP_Win_mixed_unlock_all_impl(MPI_Win win, CSP_Win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs;
    int i, j, k;

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);

#ifdef CSP_ENABLE_SYNC_ALL_OPT

    /* Optimization for MPI implementations that have optimized lock_all.
     * However, user should be noted that, if MPI implementation issues lock messages
     * for every target even if it does not have any operation, this optimization
     * could lose performance and even lose asynchronous! */
    for (i = 0; i < ug_win->num_ug_wins; i++) {
        CSP_DBG_PRINT("[%d]unlock_all(ug_win 0x%x)\n", user_rank, ug_win->ug_wins[i]);
        mpi_errno = PMPI_Win_unlock_all(ug_win->ug_wins[i]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
#else
    for (i = 0; i < user_nprocs; i++) {
        for (k = 0; k < CSP_ENV.num_g; k++) {
            int target_g_rank_in_ug = ug_win->targets[i].g_ranks_in_ug[k];

            CSP_DBG_PRINT("[%d]unlock(Ghost(%d), ug_win 0x%x), instead of "
                             "target rank %d\n", user_rank, target_g_rank_in_ug,
                             ug_win->targets[i].ug_win, i);
            mpi_errno = PMPI_Win_unlock(target_g_rank_in_ug, ug_win->targets[i].ug_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
    }
#endif

#ifdef CSP_ENABLE_LOCAL_LOCK_OPT
    mpi_errno = CSP_Win_unlock_self_impl(ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
#endif

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_unlock_all(MPI_Win win)
{
    CSP_Win *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs;
    int i, j, k;

    CSP_DBG_PRINT_FCNAME();

    CSP_Fetch_ug_win_from_cache(win, ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_unlock_all(win);
    }

    /* casper window starts */

    CSP_Assert((ug_win->info_args.epoch_type & CSP_EPOCH_LOCK) ||
                  (ug_win->info_args.epoch_type & CSP_EPOCH_LOCK_ALL));

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);

    for (i = 0; i < user_nprocs; i++) {
        ug_win->targets[i].remote_lock_assert = 0;
    }

    if (!(ug_win->info_args.epoch_type & CSP_EPOCH_LOCK)) {

        /* In lock_all only epoch, unlock all ghosts on the single window. */

#ifdef CSP_ENABLE_SYNC_ALL_OPT

        /* Optimization for MPI implementations that have optimized lock_all.
         * However, user should be noted that, if MPI implementation issues lock messages
         * for every target even if it does not have any operation, this optimization
         * could lose performance and even lose asynchronous! */
        CSP_DBG_PRINT("[%d]unlock_all(ug_win 0x%x)\n", user_rank, ug_win->ug_wins[0]);
        mpi_errno = PMPI_Win_unlock_all(ug_win->ug_wins[0]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
#else
        mpi_errno = PMPI_Win_unlock_all(ug_win->ug_wins[0]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
#if 0   /* segmentation fault */
        for (i = 0; i < ug_win->num_g_ranks_in_ug; i++) {
            mpi_errno = PMPI_Win_unlock(ug_win->g_ranks_in_ug[i], ug_win->ug_wins[0]);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
#endif
#endif

#ifdef CSP_ENABLE_LOCAL_LOCK_OPT
#if 0   /* segmentation fault */
        mpi_errno = CSP_Win_unlock_self_impl(ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
#else
        ug_win->is_self_locked = 0;
#endif
#endif
    }
    else {

        /* In lock_all/lock mixed epoch, separate windows are bound with each target. */
        mpi_errno = CSP_Win_mixed_unlock_all_impl(win, ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    for (i = 0; i < user_nprocs; i++) {
        for (j = 0; j < ug_win->targets[i].num_segs; j++) {
            ug_win->targets[i].segs[j].main_lock_stat = CSP_MAIN_LOCK_RESET;
        }
    }
#endif

    /* Decrease lock/lockall counter, change epoch status only when counter
     * become 0. */
    ug_win->lockall_counter--;
    if (ug_win->lockall_counter == 0 && ug_win->lock_counter == 0) {
        CSP_DBG_PRINT("all locks are cleared ! no epoch now\n");
        ug_win->epoch_stat = CSP_WIN_NO_EPOCH;
    }

    /* TODO: All the operations which we have not wrapped up will be failed, because they
     * are issued to user window. We need wrap up all operations.
     */

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

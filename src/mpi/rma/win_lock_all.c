#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

int MPI_Win_lock_all(int assert, MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs;
    int i, j, k;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);
    PMPI_Comm_size(uh_win->user_comm, &user_nprocs);

    for (i = 0; i < user_nprocs; i++) {
        uh_win->targets[i].remote_lock_assert = assert;
    }

    MTCORE_DBG_PRINT("[%d]lock_all, MPI_MODE_NOCHECK %d(assert %d)\n", user_rank,
                     (assert & MPI_MODE_NOCHECK) != 0, assert);

    /* Lock all Helpers in corresponding uh-window of each target process. */
#ifdef MTCORE_ENABLE_SYNC_ALL_OPT

    /* Optimization for MPI implementations that have optimized lock_all.
     * However, user should be noted that, if MPI implementation issues lock messages
     * for every target even if it does not have any operation, this optimization
     * could lose performance and even lose asynchronous! */
    for (i = 0; i < uh_win->num_uh_wins; i++) {
        MTCORE_DBG_PRINT("[%d]lock_all(uh_wins[%d])\n", user_rank, i);
        mpi_errno = PMPI_Win_lock_all(assert, uh_win->uh_wins[i]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
#else

    /* Lock all the helpers of each target user. We do not lock other user processes,
     * otherwise it cannot be guaranteed to be fully asynchronous. */
    for (i = 0; i < user_nprocs; i++) {
        for (j = 0; j < uh_win->targets[i].num_segs; j++) {
            for (k = 0; k < MTCORE_NUM_H; k++) {
                int target_h_rank_in_uh = uh_win->targets[i].h_ranks_in_uh[k];

                MTCORE_DBG_PRINT("[%d]lock(Helper(%d), uh_wins 0x%x), instead of "
                                 "target rank %d seg %d\n", user_rank, target_h_rank_in_uh,
                                 uh_win->targets[i].segs[j].uh_win, i, j);
                mpi_errno = PMPI_Win_lock(MPI_LOCK_SHARED, target_h_rank_in_uh, assert,
                                          uh_win->targets[i].segs[j].uh_win);
                if (mpi_errno != MPI_SUCCESS)
                    goto fn_fail;
            }
        }
    }
#endif

#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
    uh_win->is_self_locked = 0;
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
        for (j = 0; j < uh_win->targets[user_rank].num_segs; j++) {
            mpi_errno = MTCORE_Win_grant_local_lock(user_rank, j, MPI_LOCK_SHARED, 0, uh_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        is_local_lock_granted = 1;
    }

#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
    if (is_local_lock_granted || (uh_win->targets[user_rank].remote_lock_assert & MPI_MODE_NOCHECK)) {
        /* Lock local rank so that operations can be executed through local target.
         * 1. Need grant lock on helper in advance due to permission check,
         * OR
         * 2. there is no concurrent epochs, hence it is safe to get local lock.*/
        int local_uh_rank;
        PMPI_Comm_rank(uh_win->local_uh_comm, &local_uh_rank);
        MTCORE_DBG_PRINT("[%d]lock self(%d, local_uh_win)\n", user_rank, local_uh_rank);

        mpi_errno = PMPI_Win_lock(MPI_LOCK_SHARED, local_uh_rank, 0, uh_win->local_uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
        uh_win->is_self_locked = 1;
    }
#endif

#if (MTCORE_LOAD_OPT != MTCORE_LOAD_OPT_NON)
    for (i = 0; i < user_nprocs; i++) {
        for (j = 0; j < uh_win->targets[i].num_segs; j++) {
            uh_win->targets[i].segs[j].main_lock_stat = MTCORE_MAIN_LOCK_RESET;

            MTCORE_Reset_win_target_ordering(i, j, uh_win);
            MTCORE_Reset_win_target_load_opt(i, uh_win);
        }
    }
#endif

    /* TODO: All the operations which we have not wrapped up will be failed, because they
     * are issued to user window. We need wrap up all operations.
     */

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

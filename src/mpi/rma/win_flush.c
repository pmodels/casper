#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

int MPI_Win_flush(int target_rank, MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank;
    int j, k;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    if (uh_win == NULL) {
        /* normal window */
        return PMPI_Win_flush(target_rank, win);
    }

    /* mtcore window starts */

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);
#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
    if (user_rank == target_rank && uh_win->is_self_locked) {
        int local_uh_rank;
        PMPI_Comm_rank(uh_win->local_uh_comm, &local_uh_rank);

        /* If target is itself, only flush the target on shared window.
         * It does not matter which window we are using for local communication,
         * we just choose local shared window in our implementation.
         */
        MTCORE_DBG_PRINT("[%d]flush self(%d, local_uh_win)\n", user_rank, local_uh_rank);
        mpi_errno = PMPI_Win_flush(local_uh_rank, uh_win->local_uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
    else
#endif
    {
#ifdef MTCORE_ENABLE_SYNC_ALL_OPT

        for (j = 0; j < uh_win->targets[target_rank].num_uh_wins; j++) {
            /* Optimization for MPI implementations that have optimized lock_all.
             * However, user should be noted that, if MPI implementation issues lock messages
             * for every target even if it does not have any operation, this optimization
             * could lose performance and even lose asynchronous! */
            MTCORE_DBG_PRINT("[%d]flush_all(uh_win 0x%x), instead of target rank %d\n",
                             user_rank, uh_win->targets[target_rank].uh_wins[j], target_rank);
            mpi_errno = PMPI_Win_flush_all(uh_win->targets[target_rank].uh_wins[j]);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
#else

#if !defined(MTCORE_ENABLE_RUNTIME_LOAD_OPT)
        /* RMA operations are only issued to the main helper, so we only flush it. */
        for (j = 0; j < uh_win->targets[target_rank].num_segs; j++) {
            int main_h_off = uh_win->targets[target_rank].segs[j].main_h_off;
            int target_h_rank_in_uh = uh_win->targets[target_rank].h_ranks_in_uh[main_h_off];
            MTCORE_DBG_PRINT("[%d]flush(Helper(%d), uh_wins 0x%x), instead of "
                             "target rank %d seg %d\n", user_rank, target_h_rank_in_uh,
                             uh_win->targets[target_rank].segs[j].uh_win, target_rank, j);

            mpi_errno = PMPI_Win_flush(target_h_rank_in_uh,
                                       uh_win->targets[target_rank].segs[j].uh_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

#else
        /* RMA operations may be distributed to all helpers, so we should
         * flush all helpers on all windows.
         *
         * Note that some flushes could be eliminated before the main lock of a
         * segment granted (see above). However, we have to loop all the segments
         * in order to check each lock status, and we may flush the same helper
         * on the same window twice if the lock is granted on that segment.
         * i.e., flush (H0, win0) and (H1, win0) twice for seg0 and seg1.
         *
         * Consider flush does nothing if no operations on that target in most
         * MPI implementation, simpler code is better */
        for (j = 0; j < uh_win->targets[target_rank].num_uh_wins; j++) {
            for (k = 0; k < MTCORE_ENV.num_h; k++) {
                int target_h_rank_in_uh = uh_win->targets[target_rank].h_ranks_in_uh[k];
                MTCORE_DBG_PRINT("[%d]flush(Helper(%d), uh_wins 0x%x), instead of "
                                 "target rank %d\n", user_rank, target_h_rank_in_uh,
                                 uh_win->targets[target_rank].uh_wins[j], target_rank);

                mpi_errno = PMPI_Win_flush(target_h_rank_in_uh,
                                           uh_win->targets[target_rank].uh_wins[j]);
                if (mpi_errno != MPI_SUCCESS)
                    goto fn_fail;
            }
        }
#endif /*end of MTCORE_ENABLE_RUNTIME_LOAD_OPT */
#endif /*end of MTCORE_ENABLE_SYNC_ALL_OPT */
    }

#if defined(MTCORE_ENABLE_RUNTIME_LOAD_OPT)
    for (j = 0; j < uh_win->targets[target_rank].num_segs; j++) {
        /* Lock of main helper is granted, we can start load balancing from the next flush/unlock.
         * Note that only target which was issued operations to is guaranteed to be granted. */
        if (uh_win->targets[target_rank].segs[j].main_lock_stat == MTCORE_MAIN_LOCK_OP_ISSUED) {
            uh_win->targets[target_rank].segs[j].main_lock_stat = MTCORE_MAIN_LOCK_GRANTED;
            MTCORE_DBG_PRINT("[%d] main lock (rank %d, seg %d) granted\n", user_rank, target_rank,
                             j);
        }

        MTCORE_Reset_win_target_load_opt(target_rank, uh_win);
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

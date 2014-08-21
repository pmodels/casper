#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

int MPI_Win_flush(int target_rank, MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, target_local_rank = -1;
    int j;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

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
        target_local_rank = uh_win->local_user_ranks[target_rank];

#ifdef MTCORE_ENABLE_SYNC_ALL_OPT

        /* Optimization for MPI implementations that have optimized lock_all.
         * However, user should be noted that, if MPI implementation issues lock messages
         * for every target even if it does not have any operation, this optimization
         * could lose performance and even lose asynchronous! */

        MTCORE_DBG_PRINT("[%d]flush_all(uh_wins[%d]), instead of target rank %d\n",
                         user_rank, target_local_rank, target_rank);
        mpi_errno = PMPI_Win_flush_all(uh_win->uh_wins[target_local_rank]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
#else
        /* RMA operations are only issued to the main helper, so we only flush it. */
        int target_h_rank_in_uh = uh_win->h_ranks_in_uh[target_rank * MTCORE_NUM_H];
        MTCORE_DBG_PRINT("[%d]flush(Helper(%d), uh_wins[%d]), instead of target rank %d\n",
                         user_rank, target_h_rank_in_uh, target_local_rank, target_rank);

        mpi_errno = PMPI_Win_flush(target_h_rank_in_uh, uh_win->uh_wins[target_local_rank]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

#if (MTCORE_LOAD_OPT != MTCORE_LOAD_OPT_NON)
        if (uh_win->is_main_lock_granted[target_rank] == MTCORE_MAIN_LOCK_GRANTED) {
            /* RMA operations may be distributed to all helpers, so we should also
             * flush other helpers. */
            for (j = 1; j < MTCORE_NUM_H; j++) {
                target_h_rank_in_uh = uh_win->h_ranks_in_uh[target_rank * MTCORE_NUM_H + j];

                MTCORE_DBG_PRINT("[%d]flush(Helper(%d), uh_wins[%d]), instead of target rank %d\n",
                                 user_rank, target_h_rank_in_uh, target_local_rank, target_rank);

                mpi_errno = PMPI_Win_flush(target_h_rank_in_uh, uh_win->uh_wins[target_local_rank]);
                if (mpi_errno != MPI_SUCCESS)
                    goto fn_fail;
            }
        }
#endif /*end of MTCORE_LOAD_OPT */

#endif /*end of MTCORE_ENABLE_SYNC_ALL_OPT */
    }

#if (MTCORE_LOAD_OPT != MTCORE_LOAD_OPT_NON)
    /* Lock of main helper is granted, we can start load balancing from the next flush/unlock.
     * Note that only target which was issued operations to is guaranteed to be granted. */
    if (uh_win->is_main_lock_granted[target_rank] == MTCORE_MAIN_LOCK_OP_ISSUED) {
        uh_win->is_main_lock_granted[target_rank] = MTCORE_MAIN_LOCK_GRANTED;
        MTCORE_DBG_PRINT("[%d] main lock (%d) granted\n", user_rank, target_rank);
    }
#endif

#if (MTCORE_LOAD_OPT != MTCORE_LOAD_OPT_NON)
    MTCORE_Reset_win_target_ordering(target_rank, uh_win);
    MTCORE_Reset_win_target_bytes_counting(target_rank, uh_win);
#endif

    /* TODO: All the operations which we have not wrapped up will be failed, because they
     * are issued to user window. We need wrap up all operations.
     */

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

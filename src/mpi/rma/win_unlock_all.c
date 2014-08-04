#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

int MPI_Win_unlock_all(MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs;
    int i, j;

    MTCORE_DBG_PRINT_FCNAME();

    mpi_errno = get_uh_win(win, &uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (uh_win > 0) {
        PMPI_Comm_rank(uh_win->user_comm, &user_rank);
        PMPI_Comm_size(uh_win->user_comm, &user_nprocs);

        /* Unlock all Helpers in corresponding uh-window of each target process. */
#ifdef MTCORE_ENABLE_SYNC_ALL_OPT

        /* Optimization for MPI implementations that have optimized lock_all.
         * However, user should be noted that, if MPI implementation issues lock messages
         * for every target even if it does not have any operation, this optimization
         * could lose performance and even lose asynchronous! */
        for (i = 0; i < uh_win->max_local_user_nprocs; i++) {
            MTCORE_DBG_PRINT("[%d]unlock_all(uh_wins[%d])\n", user_rank, i);
            mpi_errno = PMPI_Win_unlock_all(uh_win->uh_wins[i]);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
#else
        for (i = 0; i < user_nprocs; i++) {
            int target_local_rank = uh_win->local_user_ranks[i];

            for (j = 0; j < MTCORE_NUM_H; j++) {
                int target_h_rank_in_uh = uh_win->h_ranks_in_uh[i * MTCORE_NUM_H + j];

                MTCORE_DBG_PRINT("[%d]unlock(Helper(%d), uh_wins[%d]), instead of target rank %d\n",
                                 user_rank, target_h_rank_in_uh, target_local_rank, i);
                mpi_errno =
                    PMPI_Win_unlock(target_h_rank_in_uh, uh_win->uh_wins[target_local_rank]);
                if (mpi_errno != MPI_SUCCESS)
                    goto fn_fail;
            }
        }
#endif

#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
        if (uh_win->is_self_locked) {
            /* We need also release the lock of local rank */
            int local_uh_rank;
            PMPI_Comm_rank(uh_win->local_uh_comm, &local_uh_rank);

            MTCORE_DBG_PRINT("[%d]unlock self(%d, local_uh_win)\n", user_rank, local_uh_rank);
            mpi_errno = PMPI_Win_unlock(local_uh_rank, uh_win->local_uh_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;

            uh_win->is_self_locked = 0;
        }
#endif
    }
    /* TODO: All the operations which we have not wrapped up will be failed, because they
     * are issued to user window. We need wrap up all operations.
     */
    else {
        mpi_errno = PMPI_Win_unlock_all(win);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

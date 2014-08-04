#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

int MPI_Win_unlock(int target_rank, MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, target_local_rank = -1;
    int j;

    MTCORE_DBG_PRINT_FCNAME();

    mpi_errno = get_uh_win(win, &uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (uh_win > 0) {
        target_local_rank = uh_win->local_user_ranks[target_rank];
        PMPI_Comm_rank(uh_win->user_comm, &user_rank);

        /* Unlock helper process in corresponding uh-window of target process. */
        for (j = 0; j < MTCORE_NUM_H; j++) {
            int target_h_rank_in_uh = uh_win->h_ranks_in_uh[target_rank * MTCORE_NUM_H + j];

            MTCORE_DBG_PRINT("[%d]unlock(Helper(%d), uh_wins[%d]), instead of target rank %d\n",
                             user_rank, target_h_rank_in_uh, target_local_rank, target_rank);

            mpi_errno = PMPI_Win_unlock(target_h_rank_in_uh, uh_win->uh_wins[target_local_rank]);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
        /* If target is itself, we need also release the lock of local rank  */
        if (user_rank == target_rank && uh_win->is_self_locked) {
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
        mpi_errno = PMPI_Win_unlock(target_rank, win);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

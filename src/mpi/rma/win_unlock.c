#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

int MPI_Win_unlock(int target_rank, MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, rank_in_local_uh = 0;
    int target_node_id = -1;
    int target_local_rank = -1;

    MTCORE_DBG_PRINT_FCNAME();

    mpi_errno = get_uh_win(win, &uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (uh_win > 0) {
        target_local_rank = uh_win->local_user_ranks[target_rank];
        PMPI_Comm_rank(uh_win->user_comm, &user_rank);

        mpi_errno = MTCORE_Get_node_ids(uh_win->user_group, 1, &target_rank, &target_node_id);
        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;

        /* Unlock helper process in corresponding uh-window of target process. */
        MTCORE_DBG_PRINT("[%d]unlock(Helper(%d), uh_wins[%d]), instead of %d, node_id %d\n",
                         user_rank, uh_win->h_ranks_in_uh[target_node_id], target_local_rank,
                         target_rank, target_node_id);

        mpi_errno = PMPI_Win_unlock(uh_win->h_ranks_in_uh[target_node_id],
                                    uh_win->uh_wins[target_local_rank]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

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

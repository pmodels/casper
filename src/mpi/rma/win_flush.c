#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

int MPI_Win_flush(int target_rank, MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, local_uh_rank = 0;
    int is_shared = 0;
    int target_node_id = -1;
    int target_local_rank = -1;

    MTCORE_DBG_PRINT_FCNAME();

    mpi_errno = get_uh_win(win, &uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (uh_win > 0) {
        PMPI_Comm_rank(uh_win->user_comm, &user_rank);
#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
        if (user_rank == target_rank && uh_win->is_self_locked) {
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

            mpi_errno = MTCORE_Get_node_ids(uh_win->user_group, 1, &target_rank, &target_node_id);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;

            MTCORE_DBG_PRINT("[%d]flush(Helper(%d), uh_wins[%d]), instead of %d, node_id %d\n",
                             user_rank, uh_win->h_ranks_in_uh[target_node_id], target_local_rank,
                             target_rank, target_node_id);

            /* We flush target Helper processes in uh_window. Because for non-shared
             * targets, all translated operations are issued to target Helpers via uh_window.
             */
            mpi_errno = PMPI_Win_flush(uh_win->h_ranks_in_uh[target_node_id],
                                       uh_win->uh_wins[target_local_rank]);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
    }
    /* TODO: All the operations which we have not wrapped up will be failed, because they
     * are issued to user window. We need wrap up all operations.
     */
    else {
        mpi_errno = PMPI_Win_flush(target_rank, win);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

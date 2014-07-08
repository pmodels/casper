#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

int MPI_Win_flush(int rank, MPI_Win win)
{
    MPIASP_Win *ua_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, local_ua_rank = 0;
    int is_shared = 0;
    int target_node_id = -1;

    MPIASP_DBG_PRINT_FCNAME();

    mpi_errno = get_ua_win(win, &ua_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (ua_win > 0) {
        PMPI_Comm_rank(ua_win->user_comm, &user_rank);
        if (user_rank == rank) {
            PMPI_Comm_rank(ua_win->local_ua_comm, &local_ua_rank);

            /* If target is itself, only flush the target on shared window.
             * It does not matter which window we are using for local communication,
             * we just choose local shared window in our implementation.
             */
            MPIASP_DBG_PRINT("[%d]flush self(%d, local_ua_win)\n", user_rank, local_ua_rank);
            mpi_errno = PMPI_Win_flush(local_ua_rank, ua_win->local_ua_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
        else {
            mpi_errno = MPIASP_Get_node_ids(ua_win->user_group, 1, &rank, &target_node_id);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;

            MPIASP_DBG_PRINT("[%d]flush(Helper(%d), ua_wins[%d]), instead of %d, node_id %d\n",
                             user_rank, ua_win->asp_ranks_in_ua[target_node_id], rank,
                             rank, target_node_id);

            /* We flush target Helper processes in ua_window. Because for non-shared
             * targets, all translated operations are issued to target Helpers via ua_window.
             */
            mpi_errno = PMPI_Win_flush(ua_win->asp_ranks_in_ua[target_node_id],
                                       ua_win->ua_wins[rank]);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
    }
    /* TODO: All the operations which we have not wrapped up will be failed, because they
     * are issued to user window. We need wrap up all operations.
     */
    else {
        mpi_errno = PMPI_Win_flush(rank, win);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

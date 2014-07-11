#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

int MPI_Win_lock(int lock_type, int rank, int assert, MPI_Win win)
{
    MPIASP_Win *ua_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, rank_in_local_ua = 0;
    int is_shared = 0;
    int target_node_id = -1;

    MPIASP_DBG_PRINT_FCNAME();

    mpi_errno = get_ua_win(win, &ua_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (ua_win > 0) {
        PMPI_Comm_rank(ua_win->user_comm, &user_rank);
        mpi_errno = MPIASP_Get_node_ids(ua_win->user_group, 1, &rank, &target_node_id);
        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;

        /* Lock Helper processes in corresponding ua-window of target process. */
        MPIASP_DBG_PRINT("[%d]lock(Helper(%d), ua_wins[%d]), instead of %d, node_id %d\n",
                         user_rank, ua_win->asp_ranks_in_ua[target_node_id], rank, rank,
                         target_node_id);

        mpi_errno = PMPI_Win_lock(lock_type, ua_win->asp_ranks_in_ua[target_node_id],
                                  assert, ua_win->ua_wins[rank]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* If target is itself, we need grant this lock before return.
         * However, the actual locked processes are the Helpers whose locks may be delayed by
         * most MPI implementation, thus we need a flush to force the lock to be granted.
         * We also lock the target itself so that operations can be executed through self target.
         */
        if (user_rank == rank) {
            mpi_errno = MPIASP_Win_grant_local_lock(lock_type, assert, ua_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;

            /* Lock local rank so that operations can be executed through local target */
            int local_ua_rank;
            PMPI_Comm_rank(ua_win->local_ua_comm, &local_ua_rank);
            MPIASP_DBG_PRINT("[%d]lock self(%d, local_ua_win)\n", user_rank, local_ua_rank);

            mpi_errno = PMPI_Win_lock(lock_type, local_ua_rank, assert, ua_win->local_ua_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
    }
    /* TODO: All the operations which we have not wrapped up will be failed, because they
     * are issued to user window. We need wrap up all operations.
     */
    else {
        mpi_errno = PMPI_Win_lock(lock_type, rank, assert, win);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

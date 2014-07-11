#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

int MPI_Win_unlock_all(MPI_Win win)
{
    MPIASP_Win *ua_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs, local_ua_nprocs;
    int i;
    int *target_node_ids = NULL;
    int *target_ranks = NULL;

    MPIASP_DBG_PRINT_FCNAME();

    mpi_errno = get_ua_win(win, &ua_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (ua_win > 0) {
        PMPI_Comm_rank(ua_win->user_comm, &user_rank);
        PMPI_Comm_size(ua_win->local_ua_comm, &local_ua_nprocs);
        PMPI_Comm_size(ua_win->user_comm, &user_nprocs);

        target_ranks = calloc(user_nprocs, sizeof(int));
        target_node_ids = calloc(user_nprocs, sizeof(int));
        for (i = 0; i < user_nprocs; i++) {
            target_ranks[i] = i;
        }

        mpi_errno = MPIASP_Get_node_ids(ua_win->user_group, user_nprocs, target_ranks,
                                        target_node_ids);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* Unlock all Helpers in corresponding ua-window of each target process. */
        for (i = 0; i < user_nprocs; i++) {
            MPIASP_DBG_PRINT("[%d]unlock(Helper(%d), ua_wins[%d]), instead of %d, node_id %d\n",
                             user_rank, ua_win->asp_ranks_in_ua[target_node_ids[i]], i, i,
                             target_node_ids[i]);
            mpi_errno =
                PMPI_Win_unlock(ua_win->asp_ranks_in_ua[target_node_ids[i]], ua_win->ua_wins[i]);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        if (ua_win->is_self_locked) {
            /* We need also release the lock of local rank */
            int local_ua_rank;
            PMPI_Comm_rank(ua_win->local_ua_comm, &local_ua_rank);
            mpi_errno = PMPI_Win_unlock(local_ua_rank, ua_win->local_ua_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;

            ua_win->is_self_locked = 0;
        }
    }
    /* TODO: All the operations which we have not wrapped up will be failed, because they
     * are issued to user window. We need wrap up all operations.
     */
    else {
        mpi_errno = PMPI_Win_unlock_all(win);
    }

  fn_exit:
    if (target_ranks)
        free(target_ranks);
    if (target_node_ids)
        free(target_node_ids);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

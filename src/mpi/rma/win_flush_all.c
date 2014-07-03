#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

int MPI_Win_flush_all(MPI_Win win)
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

        /* Flush shared window for local communication if there is at least one
         * local user process.
         *
         * We only use a single window for all the local processes because operations
         * are directly issued to the real targets so that we do not need addition
         * windows for permission check.
         */
        if (local_ua_nprocs > MPIASP_NUM_ASP_IN_LOCAL) {
            MPIASP_DBG_PRINT("[%d]flush_all(local_ua_win)\n", user_rank);

            mpi_errno = PMPI_Win_flush_all(ua_win->local_ua_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        mpi_errno =
            MPIASP_Get_node_ids(ua_win->user_group, user_nprocs, target_ranks, target_node_ids);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* Flush all Helpers in corresponding ua-window of each target process.
         *
         * We cannot flush other user processes, otherwise it cannot be guaranteed
         * to be fully asynchronous.
         */
        for (i = 0; i < user_nprocs; i++) {
            MPIASP_DBG_PRINT("[%d]flush(Helper(%d), ua_wins[%d]), instead of %d, node_id %d\n",
                             user_rank, ua_win->asp_ranks_in_ua[target_node_ids[i]], i, i,
                             target_node_ids[i]);
            mpi_errno =
                PMPI_Win_flush(ua_win->asp_ranks_in_ua[target_node_ids[i]], ua_win->ua_wins[i]);
        }
    }
    /* TODO: All the operations which we have not wrapped up will be failed, because they
     * are issued to user window. We need wrap up all operations.
     */
    else {
        mpi_errno = PMPI_Win_flush_all(win);
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

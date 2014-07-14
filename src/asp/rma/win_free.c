#include <stdio.h>
#include <stdlib.h>
#include "asp.h"

#undef FUNCNAME
#define FUNCNAME ASP_Win_free

int ASP_Win_free(int user_local_root, int user_nprocs, int user_local_nprocs, int user_tag)
{
    int mpi_errno = MPI_SUCCESS;
    int dst;
    int ua_nprocs, ua_rank;
    ASP_Win *win;
    unsigned long asp_win_handle;
    MPI_Status stat;
    int i;

    /* Receive the handle of ASP win */
    mpi_errno = PMPI_Recv(&asp_win_handle, 1, MPI_UNSIGNED_LONG,
                          user_local_root, user_tag, MPIASP_COMM_LOCAL, &stat);
    if (mpi_errno != 0)
        goto fn_fail;

    mpi_errno = get_asp_win(asp_win_handle, &win);
    if (mpi_errno != 0)
        goto fn_fail;

    /* Release ASP resources if there is a corresponding ASP-window */
    if (win > 0) {

        /* Free ua_win before local_ua_win, because all the incoming operations
         * should be done before free shared buffers.
         *
         * We do not need additional barrier in Manticore for waiting all
         * operations complete, because Win_free already internally add a barrier
         * for waiting operations on that window complete.
         */
        if (win->ua_wins) {
            ASP_DBG_PRINT(" free ua windows\n");
            for (i = 0; i < win->max_local_user_nprocs; i++) {
                if (win->ua_wins[i]) {
                    mpi_errno = PMPI_Win_free(&win->ua_wins[i]);
                    if (mpi_errno != MPI_SUCCESS)
                        goto fn_fail;
                }
            }
        }

        if (win->local_ua_win) {
            ASP_DBG_PRINT(" free shared window\n");
            mpi_errno = PMPI_Win_free(&win->local_ua_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        if (win->local_ua_comm && win->local_ua_comm != MPIASP_COMM_LOCAL) {
            ASP_DBG_PRINT(" free shared communicator\n");
            mpi_errno = PMPI_Comm_free(&win->local_ua_comm);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        if (win->ua_comm && win->ua_comm != MPI_COMM_WORLD) {
            ASP_DBG_PRINT(" free ua communicator\n");
            mpi_errno = PMPI_Comm_free(&win->ua_comm);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        if (win->user_base_addrs_in_local)
            free(win->user_base_addrs_in_local);
        if (win->ua_wins)
            free(win->ua_wins);

        free(win);

        ASP_DBG_PRINT(" Freed ASP window\n");
    }
    else {
        ASP_DBG_PRINT(" no corresponding ASP window, tag 0x%x\n", user_tag);
    }

  fn_exit:

    return mpi_errno;

  fn_fail:

    goto fn_exit;
}

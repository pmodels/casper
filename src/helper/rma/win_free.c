#include <stdio.h>
#include <stdlib.h>
#include "mtcore_helper.h"

#undef FUNCNAME
#define FUNCNAME MTCORE_H_win_free

int MTCORE_H_win_free(int user_local_root, int user_nprocs, int user_local_nprocs)
{
    int mpi_errno = MPI_SUCCESS;
    int dst;
    int uh_nprocs, uh_rank;
    MTCORE_H_win *win;
    unsigned long mtcore_h_win_handle = 0UL;
    MPI_Status stat;
    int i;

    /* Receive the handle of helper win. */
    mpi_errno = PMPI_Recv(&mtcore_h_win_handle, 1, MPI_UNSIGNED_LONG,
                          user_local_root, 0, MTCORE_COMM_LOCAL, &stat);
    if (mpi_errno != 0)
        goto fn_fail;
    MTCORE_H_DBG_PRINT(" Received window handler 0x%lx\n", mtcore_h_win_handle);

    mpi_errno = mtcore_get_h_win(mtcore_h_win_handle, &win);
    if (mpi_errno != 0)
        goto fn_fail;

    /* Release MTCORE resources if there is a corresponding MTCORE-window */
    if (win > 0) {

        /* Free uh_win before local_uh_win, because all the incoming operations
         * should be done before free shared buffers.
         *
         * We do not need additional barrier in Manticore for waiting all
         * operations complete, because Win_free already internally add a barrier
         * for waiting operations on that window complete.
         */
        if (win->num_uh_wins > 0 && win->uh_wins) {
            MTCORE_H_DBG_PRINT(" free uh windows\n");
            for (i = 0; i < win->num_uh_wins; i++) {
                if (win->uh_wins[i]) {
                    mpi_errno = PMPI_Win_free(&win->uh_wins[i]);
                    if (mpi_errno != MPI_SUCCESS)
                        goto fn_fail;
                }
            }
        }
        if (win->num_pscw_uh_wins > 0 && win->pscw_wins) {
            MTCORE_DBG_PRINT("\t free pscw windows\n");
            for (i = 0; i < win->num_pscw_uh_wins; i++) {
                if (win->pscw_wins) {
                    mpi_errno = PMPI_Win_free(&win->pscw_wins[i]);
                    if (mpi_errno != MPI_SUCCESS)
                        goto fn_fail;
                }
            }
        }
        if (win->fence_win) {
            MTCORE_H_DBG_PRINT(" free fence window\n");
            mpi_errno = PMPI_Win_free(&win->fence_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        if (win->local_uh_win) {
            MTCORE_H_DBG_PRINT(" free shared window\n");
            mpi_errno = PMPI_Win_free(&win->local_uh_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        if (win->ur_h_comm && win->ur_h_comm != MPI_COMM_NULL) {
            MTCORE_H_DBG_PRINT(" free user root + helpers communicator\n");
            mpi_errno = PMPI_Comm_free(&win->ur_h_comm);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        if (win->local_uh_comm && win->local_uh_comm != MTCORE_COMM_LOCAL) {
            MTCORE_H_DBG_PRINT(" free shared communicator\n");
            mpi_errno = PMPI_Comm_free(&win->local_uh_comm);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        if (win->uh_comm && win->uh_comm != MPI_COMM_WORLD) {
            MTCORE_H_DBG_PRINT(" free uh communicator\n");
            mpi_errno = PMPI_Comm_free(&win->uh_comm);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        MTCORE_H_DBG_PRINT(" release key 0x%lx from hash table\n", mtcore_h_win_handle);
        mpi_errno = mtcore_remove_h_win(mtcore_h_win_handle);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        if (win->user_base_addrs_in_local)
            free(win->user_base_addrs_in_local);
        if (win->uh_wins)
            free(win->uh_wins);
        if (win->pscw_wins)
            free(win->pscw_wins);

        free(win);

        MTCORE_H_DBG_PRINT(" Freed MTCORE window\n");
    }
    else {
        MTCORE_H_DBG_PRINT(" no corresponding MTCORE window\n");
    }

  fn_exit:

    return mpi_errno;

  fn_fail:
    fprintf(stderr, "error happened in %s, abort\n", __FUNCTION__);
    PMPI_Abort(MPI_COMM_WORLD, 0);
}

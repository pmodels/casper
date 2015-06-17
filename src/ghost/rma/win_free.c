/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspg.h"

#undef FUNCNAME
#define FUNCNAME CSPG_win_free

int CSPG_win_free(int user_local_root)
{
    int mpi_errno = MPI_SUCCESS;
    CSPG_win *win;
    unsigned long csp_g_win_handle = 0UL;
    MPI_Status stat;
    int i;

    /* Receive the handle of ghost win. */
    mpi_errno = PMPI_Recv(&csp_g_win_handle, 1, MPI_UNSIGNED_LONG,
                          user_local_root, 0, CSP_COMM_LOCAL, &stat);
    if (mpi_errno != 0)
        goto fn_fail;
    CSPG_DBG_PRINT(" Received window handler 0x%lx\n", csp_g_win_handle);

    win = (CSPG_win *) csp_g_win_handle;
    if (!win) {
        CSPG_ERR_PRINT(" Wrong window handler 0x%lx, not exist\n", csp_g_win_handle);
        mpi_errno = -1;
        goto fn_fail;
    }
    if (win->csp_g_win_handle != csp_g_win_handle) {
        CSPG_ERR_PRINT(" Wrong window handler 0x%lx, not match\n", csp_g_win_handle);
        mpi_errno = -1;
        goto fn_fail;
    }

    /* Release CASPER resources if there is a corresponding CASPER-window */
    if (win > 0) {

        /* Free ug_win before local_ug_win, because all the incoming operations
         * should be done before free shared buffers.
         *
         * We do not need additional barrier in CASPER for waiting all
         * operations complete, because Win_free already internally add a barrier
         * for waiting operations on that window complete.
         */
        if (win->num_ug_wins > 0 && win->ug_wins) {
            CSPG_DBG_PRINT(" free ug windows\n");
            for (i = 0; i < win->num_ug_wins; i++) {
                if (win->ug_wins[i]) {
                    mpi_errno = PMPI_Win_free(&win->ug_wins[i]);
                    if (mpi_errno != MPI_SUCCESS)
                        goto fn_fail;
                }
            }
        }

        if (win->active_win) {
            CSPG_DBG_PRINT(" free active window\n");
            mpi_errno = PMPI_Win_free(&win->active_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        if (win->local_ug_win) {
            CSPG_DBG_PRINT(" free shared window\n");
            mpi_errno = PMPI_Win_free(&win->local_ug_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        if (win->ur_g_comm && win->ur_g_comm != MPI_COMM_NULL) {
            CSPG_DBG_PRINT(" free user root + ghosts communicator\n");
            mpi_errno = PMPI_Comm_free(&win->ur_g_comm);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        if (win->local_ug_comm && win->local_ug_comm != CSP_COMM_LOCAL) {
            CSPG_DBG_PRINT(" free shared communicator\n");
            mpi_errno = PMPI_Comm_free(&win->local_ug_comm);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        if (win->ug_comm && win->ug_comm != MPI_COMM_WORLD) {
            CSPG_DBG_PRINT(" free ug communicator\n");
            mpi_errno = PMPI_Comm_free(&win->ug_comm);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        if (win->user_base_addrs_in_local)
            free(win->user_base_addrs_in_local);
        if (win->ug_wins)
            free(win->ug_wins);

        free(win);

        CSPG_DBG_PRINT(" Freed CASPER window\n");
    }
    else {
        CSPG_DBG_PRINT(" no corresponding CASPER window\n");
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    fprintf(stderr, "error happened in %s, abort\n", __FUNCTION__);
    PMPI_Abort(MPI_COMM_WORLD, 0);
    goto fn_exit;
}

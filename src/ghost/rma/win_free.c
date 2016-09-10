/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspg.h"

/* Common internal implementation of win_free handlers.*/
static int win_free_impl(CSP_cmd_fnc_winfree_pkt_t * winfree_pkt)
{
    int mpi_errno = MPI_SUCCESS;
    CSPG_win_t *win = NULL;
    unsigned long csp_g_win_handle = 0UL;
    int i;

    /* Receive the handle of ghost win from local user root. */
    mpi_errno = PMPI_Recv(&csp_g_win_handle, 1, MPI_UNSIGNED_LONG,
                          winfree_pkt->user_local_root, CSP_CMD_PARAM_TAG,
                          CSP_PROC.local_comm, MPI_STATUS_IGNORE);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    CSPG_DBG_PRINT(" Received window handler 0x%lx\n", csp_g_win_handle);

    win = (CSPG_win_t *) csp_g_win_handle;
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
    if (win != NULL) {

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

        if (win->global_win) {
            CSPG_DBG_PRINT(" free global window\n");
            mpi_errno = PMPI_Win_free(&win->global_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        if (win->local_ug_win) {
            CSPG_DBG_PRINT(" free shared window\n");
            mpi_errno = PMPI_Win_free(&win->local_ug_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        if (win->local_ug_comm && win->local_ug_comm != CSP_PROC.local_comm) {
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
    goto fn_exit;
}

int CSPG_win_free_root_handler(CSP_cmd_pkt_t * pkt, int user_local_rank CSP_ATTRIBUTE((unused)))
{
    int mpi_errno = MPI_SUCCESS;
    CSP_cmd_fnc_winfree_pkt_t *winfree_pkt = &pkt->u.fnc_winfree;

    /* broadcast to other local ghosts */
    mpi_errno = CSPG_cmd_bcast(pkt);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    mpi_errno = win_free_impl(winfree_pkt);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    /* Release local lock after a locked command finished. */
    mpi_errno = CSPG_cmd_release_lock();
    return mpi_errno;

  fn_fail:
    CSPG_ERR_PRINT("error happened in %s, abort\n", __FUNCTION__);
    PMPI_Abort(MPI_COMM_WORLD, 0);
    goto fn_exit;
}

int CSPG_win_free_handler(CSP_cmd_pkt_t * pkt)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_cmd_fnc_winfree_pkt_t *winfree_pkt = &pkt->u.fnc_winfree;

    mpi_errno = win_free_impl(winfree_pkt);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    return mpi_errno;

  fn_fail:
    CSPG_ERR_PRINT("error happened in %s, abort\n", __FUNCTION__);
    PMPI_Abort(MPI_COMM_WORLD, 0);
    goto fn_exit;
}

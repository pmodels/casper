/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspg.h"

/* Common internal implementation of win_free handlers.*/
static int win_free_impl(CSP_cwp_fnc_winfree_pkt_t * winfree_pkt)
{
    int mpi_errno = MPI_SUCCESS;
    CSPG_win_t *win = NULL;
    unsigned long csp_g_win_handle = 0UL;
    int i;

    /* Receive the handle of ghost win from local user root. */
    CSP_CALLMPI(JUMP, PMPI_Recv(&csp_g_win_handle, 1, MPI_UNSIGNED_LONG,
                                winfree_pkt->user_local_root, CSP_CWP_PARAM_TAG,
                                CSP_PROC.local_comm, MPI_STATUS_IGNORE));
    CSPG_DBG_PRINT(" Received window handler 0x%lx\n", csp_g_win_handle);

    win = (CSPG_win_t *) csp_g_win_handle;
    if (!win) {
        CSP_msg_print(CSP_MSG_ERROR, " Wrong window handler 0x%lx, not exist\n", csp_g_win_handle);
        mpi_errno = CSP_get_error_code(CSP_ERR_INTERN);
        goto fn_fail;
    }
    if (win->csp_g_win_handle != csp_g_win_handle) {
        CSP_msg_print(CSP_MSG_ERROR, " Wrong window handler 0x%lx, not match\n", csp_g_win_handle);
        mpi_errno = CSP_get_error_code(CSP_ERR_INTERN);
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
                    CSP_CALLMPI(JUMP, PMPI_Win_free(&win->ug_wins[i]));
                }
            }
        }

        if (win->global_win) {
            CSPG_DBG_PRINT(" free global window\n");
            CSP_CALLMPI(JUMP, PMPI_Win_free(&win->global_win));
        }

        if (win->local_ug_win) {
            CSPG_DBG_PRINT(" free shared window\n");
            CSP_CALLMPI(JUMP, PMPI_Win_free(&win->local_ug_win));
        }

        if (win->local_ug_comm && win->local_ug_comm != CSP_PROC.local_comm) {
            CSPG_DBG_PRINT(" free shared communicator\n");
            CSP_CALLMPI(JUMP, PMPI_Comm_free(&win->local_ug_comm));
        }

        if (win->ug_comm && win->ug_comm != MPI_COMM_WORLD) {
            CSPG_DBG_PRINT(" free ug communicator\n");
            CSP_CALLMPI(JUMP, PMPI_Comm_free(&win->ug_comm));
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

int CSPG_win_free_cwp_root_handler(CSP_cwp_pkt_t * pkt, int user_local_rank CSP_ATTRIBUTE((unused)))
{
    int mpi_errno = MPI_SUCCESS;
    CSP_cwp_fnc_winfree_pkt_t *winfree_pkt = &pkt->u.fnc_winfree;

    /* broadcast to other local ghosts */
    mpi_errno = CSPG_cwp_bcast(pkt);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    mpi_errno = win_free_impl(winfree_pkt);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    /* Release local lock after a locked command finished. */
    mpi_errno = CSPG_mlock_release();
    return mpi_errno;

  fn_fail:
    /* Error is handled in CSPG_main. */
    goto fn_exit;
}

int CSPG_win_free_cwp_handler(CSP_cwp_pkt_t * pkt)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_cwp_fnc_winfree_pkt_t *winfree_pkt = &pkt->u.fnc_winfree;

    mpi_errno = win_free_impl(winfree_pkt);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    return mpi_errno;

  fn_fail:
    /* Error is handled in CSPG_main. */
    goto fn_exit;
}

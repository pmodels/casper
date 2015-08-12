/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspg.h"

CSPG_cmd_handler_t cmd_handlers[CSP_CMD_MAX] = { NULL };

static void init_cmd_handlers(void)
{
    cmd_handlers[CSP_CMD_WIN_ALLOCATE] = CSPG_win_allocate;
    cmd_handlers[CSP_CMD_WIN_FREE] = CSPG_win_free;
    cmd_handlers[CSP_CMD_FINALIZE] = CSPG_finalize;
}

int CSPG_init(void)
{
    int mpi_errno = MPI_SUCCESS;
    int err_class = 0, errstr_len = 0;
    char err_string[MPI_MAX_ERROR_STRING];
    CSP_cmd_pkt_t pkt;
    int exit_flag = 0;

    CSPG_DBG_PRINT(" main start\n");
    init_cmd_handlers();

    /* Disable MPI automatic error messages. */
    mpi_errno = PMPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    while (1) {
        mpi_errno = CSPG_cmd_recv(&pkt);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* skip unknown command */
        if (pkt.cmd <= CSP_CMD_NULL || pkt.cmd >= CSP_CMD_MAX || !cmd_handlers[pkt.cmd]) {
            CSPG_DBG_PRINT(" Received unknown CMD %d\n", (int) (pkt.cmd));
            continue;
        }

        mpi_errno = cmd_handlers[pkt.cmd] (&pkt, &exit_flag);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* exit when finalize finished */
        if (exit_flag)
            goto fn_exit;
    }

    CSPG_DBG_PRINT(" main done\n");

  fn_exit:
    return mpi_errno;

  fn_fail:
    PMPI_Error_class(mpi_errno, &err_class);
    PMPI_Error_string(mpi_errno, err_string, &errstr_len);
    CSPG_ERR_PRINT("MPI reports error code %d, error class %d\n%s",
                   mpi_errno, err_class, err_string);
    goto fn_exit;
}

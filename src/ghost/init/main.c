/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspg.h"

/* Main entry of ghost process after initialization. */
int CSPG_main(void)
{
    int mpi_errno = MPI_SUCCESS;
    int err_class = 0, errstr_len = 0;
    char err_string[MPI_MAX_ERROR_STRING];

    CSPG_DBG_PRINT(" main start\n");

    /* Keep polling progress until finalize done */
    mpi_errno = CSPG_cwp_do_progress();
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    CSPG_DBG_PRINT(" main done\n");

  fn_exit:
    return mpi_errno;

  fn_fail:
    PMPI_Error_class(mpi_errno, &err_class);
    PMPI_Error_string(mpi_errno, err_string, &errstr_len);
    CSP_msg_print(CSP_MSG_ERROR, "MPI reports error code %d, error class %d\n%s",
                  mpi_errno, err_class, err_string);
    CSP_ERR_ABORT();
    goto fn_exit;
}

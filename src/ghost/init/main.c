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

    CSPG_DBG_PRINT(" main start\n");

    /* Keep polling progress until finalize done */
    mpi_errno = CSPG_cwp_do_progress();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    CSPG_DBG_PRINT(" main done\n");

  fn_exit:
    return mpi_errno;

  fn_fail:
    CSP_ERROR_ABORT(mpi_errno);
    goto fn_exit;
}

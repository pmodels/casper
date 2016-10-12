/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Errhandler_free(MPI_Errhandler * errhandler)
{
    int mpi_errno = MPI_SUCCESS;

    /* Remove cached [error handler -> callback function] record, only free if found. */
    CSPU_errhan_remove_fnc(*errhandler);

    CSP_CALLMPI(JUMP, PMPI_Errhandler_free(errhandler));

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

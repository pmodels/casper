/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Errhandler_set(MPI_Comm comm, MPI_Errhandler errhandler)
{
    int mpi_errno = MPI_SUCCESS;

    mpi_errno = PMPI_Errhandler_set(comm, errhandler);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Set error handler for both MPI_COMM_WORLD and COMM_USER_WORLD if
     * input comm is MPI_COMM_WORLD (See description in comm_set_errhandler). */
    if (comm == MPI_COMM_WORLD) {
        mpi_errno = PMPI_Errhandler_set(CSP_COMM_USER_WORLD, errhandler);
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

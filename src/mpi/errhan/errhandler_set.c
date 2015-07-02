/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "csp.h"

int MPI_Errhandler_set(MPI_Comm comm, MPI_Errhandler errhandler)
{
    int mpi_errno = MPI_SUCCESS;

    mpi_errno = PMPI_Errhandler_set(comm, errhandler);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Also set error handler for the real MPI_COMM_WORLD.
     * Because MPI standard says:
     * MPI calls that are not related to any objects are considered to be
     * attached to the communicator MPI_COMM_WORLD. */
    if (comm == CSP_COMM_USER_WORLD) {
        mpi_errno = PMPI_Errhandler_set(MPI_COMM_WORLD, errhandler);
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

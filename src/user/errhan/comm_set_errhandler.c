/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Comm_set_errhandler(MPI_Comm comm, MPI_Errhandler errhandler)
{
    int mpi_errno = MPI_SUCCESS;

    CSP_CALLMPI(JUMP, PMPI_Comm_set_errhandler(comm, errhandler));

    /* Set error handler for both MPI_COMM_WORLD and COMM_USER_WORLD if
     * input comm is MPI_COMM_WORLD.
     * - Setting to MPI_COMM_WORLD allows global error handler to MPI calls
     *   that are not related to any objects.
     * - Setting to COMM_USER_WORLD allows specified error handler to MPI
     *   calls related COMM_WORLD (translated to COMM_USER_WORLD) and its children.
     * - No need to check any other input comm, because they are always created
     *   from COMM_USER_WORLD. */
    if (comm == MPI_COMM_WORLD) {
        mpi_errno = PMPI_Comm_set_errhandler(CSP_COMM_USER_WORLD, errhandler);
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

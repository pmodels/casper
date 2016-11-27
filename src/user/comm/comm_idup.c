/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Comm_idup(MPI_Comm comm, MPI_Comm * newcomm, MPI_Request * request)
{
    int mpi_errno = MPI_SUCCESS;

    if (comm == MPI_COMM_WORLD)
        comm = CSP_COMM_USER_WORLD;

    CSP_CALLMPI(JUMP, PMPI_Comm_idup(comm, newcomm, request));

    /* Inherit and cache the error handler wrapper for valid new communicator. */
    if ((*newcomm) != MPI_COMM_NULL) {
        mpi_errno = CSPU_comm_errhan_inherit(comm, *newcomm);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

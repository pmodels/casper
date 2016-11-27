/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Cart_create(MPI_Comm comm_old, int ndims, const int dims[],
                    const int periods[], int reorder, MPI_Comm * comm_cart)
{
    int mpi_errno = MPI_SUCCESS;

    if (comm_old == MPI_COMM_WORLD)
        comm_old = CSP_COMM_USER_WORLD;

    CSP_CALLMPI(JUMP, PMPI_Cart_create(comm_old, ndims, dims, periods, reorder, comm_cart));

    /* Inherit and cache the error handler wrapper for valid new communicator.
     * Note that MPI_COMM_NULL is returned on excluded process. */
    if ((*comm_cart) != MPI_COMM_NULL) {
        mpi_errno = CSPU_comm_errhan_inherit(comm_old, *comm_cart);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

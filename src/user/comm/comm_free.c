/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Comm_free(MPI_Comm * comm)
{
    int mpi_errno = MPI_SUCCESS;

    /* User should not free COMM_WORLD, but we just let MPI to handle such error.
     * Even user makes such mistake, we do not free COMM_USER_WORLD. */

    /* Only free the error handler record for valid user communicators.
     * The COMM_WORLD is always destroyed at finalize.*/
    if ((*comm) != MPI_COMM_WORLD && (*comm) != MPI_COMM_NULL) {
        CSP_ASSERT((*comm) != CSP_COMM_USER_WORLD);     /* user should never see COMM_USER_WORLD. */
        CSPU_comm_errhan_reset(*comm);
    }

    CSP_CALLMPI(JUMP, PMPI_Comm_free(comm));

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

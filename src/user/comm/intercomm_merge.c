/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Intercomm_merge(MPI_Comm intercomm, int high, MPI_Comm * newintracomm)
{
    int mpi_errno = MPI_SUCCESS;

    /* Skip internal processing when disabled */
    if (CSP_IS_DISABLED)
        return PMPI_Intercomm_merge(intercomm, high, newintracomm);

    if (intercomm == MPI_COMM_WORLD)
        intercomm = CSP_COMM_USER_WORLD;

    CSP_CALLMPI(JUMP, PMPI_Intercomm_merge(intercomm, high, newintracomm));

    /* Inherit and cache the error handler wrapper for valid new communicator.
     * Note that MPI_COMM_NULL is returned on excluded process. */
    if ((*newintracomm) != MPI_COMM_NULL) {
        mpi_errno = CSPU_comm_errhan_inherit(intercomm, *newintracomm);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

    /* Create background ug_comm.
     * FIXME: wrap up error handler. */
    if (CSP_IS_MODE_ENABLED(PT2PT) && (*newintracomm) != MPI_COMM_NULL) {
        mpi_errno = CSPU_ugcomm_create(intercomm, MPI_INFO_NULL, *newintracomm);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Intercomm_create(MPI_Comm local_comm, int local_leader,
                         MPI_Comm peer_comm, int remote_leader, int tag, MPI_Comm * newintercomm)
{
    int mpi_errno = MPI_SUCCESS;

    if (local_comm == MPI_COMM_WORLD)
        local_comm = CSP_COMM_USER_WORLD;

    if (peer_comm == MPI_COMM_WORLD)
        peer_comm = CSP_COMM_USER_WORLD;

    CSP_CALLMPI(JUMP, PMPI_Intercomm_create(local_comm, local_leader, peer_comm,
                                            remote_leader, tag, newintercomm));

    /* Inherit and cache the error handler wrapper for valid new communicator.
     * Note that MPI_COMM_NULL is returned on excluded process. */
    if ((*newintercomm) != MPI_COMM_NULL) {
        mpi_errno = CSPU_comm_errhan_inherit(local_comm, *newintercomm);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

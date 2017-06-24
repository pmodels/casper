/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Comm_get_attr(MPI_Comm comm, int comm_keyval, void *attribute_val, int *flag)
{
    int mpi_errno = MPI_SUCCESS;

    /* Skip internal processing when disabled */
    if (CSP_IS_DISABLED)
        return PMPI_Comm_get_attr(comm, comm_keyval, attribute_val, flag);

    if (comm == MPI_COMM_WORLD)
        comm = CSP_COMM_USER_WORLD;

    CSP_CALLMPI(JUMP, PMPI_Comm_get_attr(comm, comm_keyval, attribute_val, flag));

    /* If PT2PT is supported, we expose overwritten tag_ub. */
    if (CSP_IS_MODE_ENABLED(PT2PT)) {
        if (comm_keyval == MPI_TAG_UB) {
            *(int *) attribute_val = CSPU_offload_ch.user_tag_ub;
        }
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

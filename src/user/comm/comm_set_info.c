/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Comm_set_info(MPI_Comm comm, MPI_Info info)
{
    int mpi_errno = MPI_SUCCESS;

    /* Skip internal processing when disabled */
    if (CSP_IS_DISABLED)
        return PMPI_Comm_set_info(comm, info);

    if (comm == MPI_COMM_WORLD)
        comm = CSP_COMM_USER_WORLD;

    CSP_CALLMPI(JUMP, PMPI_Comm_set_info(comm, info));

    if (CSP_IS_MODE_ENABLED(PT2PT)) {
        CSPU_comm_t *ug_comm = NULL;

        CSPU_fetch_ug_comm_from_cache(comm, &ug_comm);
        if (ug_comm) {
            /* TODO: We do not change current comm's behavior for simplicity.
             * Only affect the child communicators . */
            mpi_errno = CSPU_ugcomm_set_info(&ug_comm->ref_info_args, info);
            CSP_CHKMPIFAIL_JUMP(mpi_errno);
        }
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

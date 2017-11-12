/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Win_allocate_shared(MPI_Aint size, int disp_unit, MPI_Info info, MPI_Comm comm,
                            void *baseptr, MPI_Win * win)
{
    int mpi_errno = MPI_SUCCESS;

    /* Skip internal processing when disabled */
    if (CSP_IS_DISABLED)
        return PMPI_Win_allocate_shared(size, disp_unit, info, comm, baseptr, win);

    if (comm == MPI_COMM_WORLD)
        comm = CSP_COMM_USER_WORLD;

    if (CSP_IS_MODE_ENABLED(PT2PT)) {
        CSPU_comm_t *ug_comm = NULL;

        /* Fetch ug_comm in user comm. */
        mpi_errno = CSPU_fetch_ug_comm_from_cache(comm, &ug_comm);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);

        /* Create shmbuf window if the communicator supports shmbuf (created ug_comm). */
        if (ug_comm && ug_comm->type >= CSP_COMM_SHMBUF)
            return CSPU_shmbuf_regist(ug_comm, size, disp_unit, info, comm, baseptr, win);
    }

    CSP_CALLMPI(NOSTMT, PMPI_Win_allocate_shared(size, disp_unit, info, comm, baseptr, win));

    if (CSP_IS_MODE_ENABLED(RMA)) {
        CSP_msg_print(CSP_MSG_WARN, "called MPI_Win_allocate_shared, "
                      "no asynchronous progress on win 0x%x\n", *win);
    }
  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

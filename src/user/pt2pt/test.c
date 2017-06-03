/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Test(MPI_Request * request, int *flag, MPI_Status * status)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_offload_cell_t *cell = NULL;

    /*FIXME: complete error handler wrapping. */

    CSPU_offload_poll_progress();

    CSPU_offload_req_hash_get(*request, &cell);

    /* Complete offload request. */
    if (cell && cell->type == CSP_OFFLOAD_CELL_SHM && CSPU_offload_check_complete(cell)) {
        CSP_CALLMPI(JUMP, PMPI_Grequest_complete(*request));
        CSP_DBG_PRINT("test: completed offload cell=%p, req=0x%x\n", cell, *request);
    }

    /* The callback functions are triggered after completion :
     * query_fn get the corresponding cell instance and generates correct status.
     * free_fn cleans up the cell instance. */
    CSP_CALLMPI(JUMP, PMPI_Test(request, flag, status));

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

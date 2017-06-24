/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Wait(MPI_Request * request, MPI_Status * status)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_offload_cell_t *cell = NULL;
    int flag = 0;

    /* Skip internal processing when disabled */
    if (CSP_IS_DISABLED || CSP_IS_MODE_DISABLED(PT2PT)) {
        return PMPI_Wait(request, status);
    }

    /*FIXME: complete error handler wrapping. */

    CSPU_offload_req_hash_get(*request, &cell);

    /* Original request or already completed. */
    if (!cell)
        return PMPI_Wait(request, status);

    do {
        if (cell->type == CSP_OFFLOAD_CELL_SHM && CSPU_offload_check_complete(cell)) {
            /* Complete offload request. */
            CSP_CALLMPI(JUMP, PMPI_Grequest_complete(*request));
            CSP_DBG_PRINT("Wait: completed offload cell=%p, req=0x%x\n", cell, *request);

            CSP_CALLMPI(JUMP, PMPI_Wait(request, status));
            break;
        }

        /* Poll progress if not completed yet. */
        CSPU_offload_poll_progress();

        /* Reload if still locally pending. */
        if (cell->type == CSP_OFFLOAD_CELL_PENDING)
            CSPU_offload_req_hash_get(*request, &cell);
        CSP_ASSERT(cell);

        /* The callback functions are triggered after completion :
         * query_fn get the corresponding cell instance and generates correct status.
         * free_fn cleans up the cell instance. */
        CSP_CALLMPI(JUMP, PMPI_Test(request, &flag, status));
    } while (!flag);

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

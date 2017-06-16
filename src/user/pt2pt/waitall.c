/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */


#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Waitall(int count, MPI_Request array_of_requests[], MPI_Status array_of_statuses[])
{
    int mpi_errno = MPI_SUCCESS;
    CSP_offload_cell_t **cells = NULL;
    int i, ncompleted = 0, *offload_cmpl_flags = NULL;
    int some_count = 0, *some_indices = NULL;
    MPI_Status *some_statuses = NULL;

    /*FIXME: complete error handler wrapping. */

    cells = CSP_calloc(count, sizeof(CSP_offload_cell_t *));
    offload_cmpl_flags = CSP_calloc(count, sizeof(int));
    some_indices = CSP_calloc(count, sizeof(int));
    if (array_of_statuses != MPI_STATUS_IGNORE)
        some_statuses = CSP_calloc(count, sizeof(MPI_Status));
    else
        some_statuses = MPI_STATUS_IGNORE;

    for (i = 0; i < count; i++)
        CSPU_offload_req_hash_get(array_of_requests[i], &cells[i]);

    do {
        for (i = 0; i < count; i++) {
            /* Skip any original or completed offload request. */
            if (offload_cmpl_flags[i] || cells[i] == NULL)
                continue;

            /* Complete offload request. */
            if (cells[i]->type == CSP_OFFLOAD_CELL_SHM && CSPU_offload_check_complete(cells[i])) {
                CSP_CALLMPI(JUMP, PMPI_Grequest_complete(array_of_requests[i]));
                CSP_DBG_PRINT("Waitall: completed offload cells[%d]=%p, reqs[%d]=0x%x\n", i,
                              cells[i], i, array_of_requests[i]);
                offload_cmpl_flags[i] = 1;
            }

            /* Polls offload progress if pending cell exists.
             * Because the progress always tries to transfer as many pending
             * cells as it can, we do not expect empty polling. */
            if (cells[i]->type == CSP_OFFLOAD_CELL_PENDING) {
                CSPU_offload_poll_progress();

                /* Reload pending cell. */
                CSPU_offload_req_hash_get(array_of_requests[i], &cells[i]);
                CSP_ASSERT(cells[i]);
            }
        }

        /* The callback functions are triggered after completion :
         * query_fn get the corresponding cell instance and generates correct status.
         * free_fn cleans up the cell instance.
         * Note that PMPI_Testall may release requests only after all requests
         * are completed. Instead, PMPI_Testsome can release completed request at
         * every poll. It guarantees every completed request becomes inactive,
         * thus is ignored at next poll.*/
        CSP_CALLMPI(JUMP,
                    PMPI_Testsome(count, array_of_requests, &some_count, some_indices,
                                  some_statuses));

        /* Should already left by checking ncompleted. */
        CSP_DBG_ASSERT(some_count != MPI_UNDEFINED);

        if (some_count > 0) {
            if (array_of_statuses != MPI_STATUS_IGNORE) {
                for (i = 0; i < some_count; i++)
                    memcpy(&array_of_statuses[some_indices[i]], &some_statuses[i],
                           sizeof(MPI_Status));
            }
            ncompleted += some_count;
        }
    } while (ncompleted < count);

  fn_exit:
    if (some_indices)
        free(some_indices);
    if (some_statuses)
        free(some_statuses);
    if (cells)
        free(cells);
    if (offload_cmpl_flags)
        free(offload_cmpl_flags);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

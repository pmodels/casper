/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */


#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

static inline int waitall_pending_impl(CSP_offload_cell_t ** cells, int count,
                                       MPI_Request array_of_requests[],
                                       MPI_Status array_of_statuses[])
{
    int mpi_errno = MPI_SUCCESS;
    int i, ncompleted = 0, ngcompleted = 0, *skip_flags = NULL;
    int some_count = 0, *some_indices = NULL;
    MPI_Status *some_statuses = NULL;

    skip_flags = CSP_calloc(count, sizeof(int));
    some_indices = CSP_calloc(count, sizeof(int));
    if (array_of_statuses != MPI_STATUSES_IGNORE)
        some_statuses = CSP_calloc(count, sizeof(MPI_Status));
    else
        some_statuses = MPI_STATUSES_IGNORE;

    do {
        for (i = 0; i < count; i++) {
            /* Skip any original or completed offload request. */
            if (skip_flags[i])
                continue;

            if (cells[i] == NULL) {
                skip_flags[i] = 1;
                ngcompleted++;  /* increment for original request in order to break from loop. */
            }
            /* Complete offload request. */
            else if (cells[i]->type == CSP_OFFLOAD_CELL_SHM &&
                     CSPU_offload_check_complete(cells[i])) {
                CSP_CALLMPI(JUMP, PMPI_Grequest_complete(array_of_requests[i]));
                CSP_DBG_PRINT("Waitall: completed offload cells[%d]=%p, reqs[%d]=0x%x\n", i,
                              cells[i], i, array_of_requests[i]);
                skip_flags[i] = 1;
                ngcompleted++;
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

        if (ncompleted != ngcompleted) {
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
        }
    } while (ncompleted < count);


  fn_exit:
    if (some_indices)
        free(some_indices);
    if (some_statuses && some_statuses != MPI_STATUSES_IGNORE)
        free(some_statuses);
    if (skip_flags)
        free(skip_flags);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Waitall(int count, MPI_Request array_of_requests[], MPI_Status array_of_statuses[])
{
    int mpi_errno = MPI_SUCCESS;
    CSP_offload_cell_t **cells = NULL;
    int i, ngcompleted = 0, *skip_flags = NULL;

    /* Skip internal processing when disabled */
    if (CSP_IS_DISABLED || CSP_IS_MODE_DISABLED(PT2PT)) {
        return PMPI_Waitall(count, array_of_requests, array_of_statuses);
    }

    /* Error directly handled by COMM_WORLD error handler. */
    /* TODO: do we need thread CS here ? */

    cells = CSP_calloc(count, sizeof(CSP_offload_cell_t *));
    skip_flags = CSP_calloc(count, sizeof(int));

    for (i = 0; i < count; i++)
        CSPU_offload_req_hash_get(array_of_requests[i], &cells[i]);

    if (CSPU_offload_ch.pending_q.noutstanding > 0) {
        /* Slow path */
        mpi_errno = waitall_pending_impl(cells, count, array_of_requests, array_of_statuses);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
        goto fn_exit;
    }

    /* Fast path if no pending cells. */
    do {
        for (i = 0; i < count; i++) {
            /* Skip any original or completed offload request. */
            if (skip_flags[i])
                continue;

            if (cells[i] == NULL) {
                skip_flags[i] = 1;
                ngcompleted++;  /* increment for original request in order to break from loop. */
            }
            /* Complete offload request. */
            else if (cells[i]->type == CSP_OFFLOAD_CELL_SHM &&
                     CSPU_offload_check_complete(cells[i])) {
                CSP_CALLMPI(JUMP, PMPI_Grequest_complete(array_of_requests[i]));
                CSP_DBG_PRINT("Waitall: completed offload cells[%d]=%p, reqs[%d]=0x%x\n", i,
                              cells[i], i, array_of_requests[i]);
                skip_flags[i] = 1;
                ngcompleted++;
            }
        }
    } while (ngcompleted < count);

    CSP_CALLMPI(JUMP, PMPI_Waitall(count, array_of_requests, array_of_statuses));

  fn_exit:
    if (cells)
        free(cells);
    if (skip_flags)
        free(skip_flags);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

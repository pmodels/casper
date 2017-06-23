/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

CSP_offload_channel_t CSPU_offload_ch;
int pending_cell_ncreated = 0;  /* DEBUG only */

static inline void offload_freestk_init(void)
{
    CSPU_offload_ch.freestk.count = 0;
    CSPU_offload_ch.freestk.top = NULL;
}

static inline void offload_pending_q_init(void)
{
    CSPU_offload_ch.pending_q.head = NULL;
    CSPU_offload_ch.pending_q.nissued = 0;
    CSPU_offload_ch.pending_q.noutstanding = 0;
}

static inline void offload_shm_recvq_init(void)
{
    CSP_OFFLOAD_SET_RL_NULL(CSPU_offload_ch.shm_recvq.q_ptr->head);
    CSP_OFFLOAD_SET_RL_NULL(CSPU_offload_ch.shm_recvq.q_ptr->tail);
    CSP_OFFLOAD_SET_RL_NULL(CSPU_offload_ch.shm_recvq.q_ptr->my_head);

    CSPU_offload_ch.shm_recvq.nissued = 0;
    CSPU_offload_ch.shm_recvq.noutstanding = 0;
}

/* NOTE : this is triggered only after explicitly completed the request.
 * See standard about MPI_Grequest_complete. */
static int CSPU_offload_req_query_fn(void *extra_state, MPI_Status * status)
{
    CSP_offload_cell_t *assign_cell = (CSP_offload_cell_t *) extra_state;
    CSP_offload_cell_t *cell = NULL;
    MPI_Request req = assign_cell->pkt.req;

    /* If the assigned cell is a pending one, we get the latest cell from hash. */
    if (assign_cell->type == CSP_OFFLOAD_CELL_PENDING) {
        CSPU_offload_req_hash_get(req, &cell);
        CSP_ASSERT(cell && cell->type == CSP_OFFLOAD_CELL_SHM && CSPU_offload_check_complete(cell));
    }
    else {
        cell = assign_cell;
        CSP_DBG_ASSERT(CSPU_offload_check_complete(cell));
    }

    /* Always send just one int */
    MPI_Status_set_elements(status, MPI_INT, 1);
    /* Can never cancel so always true */
    MPI_Status_set_cancelled(status, 0);

    /* Copy status from cell.
     * Note that the status of a send message is not updated by MPI. */
    status->MPI_SOURCE = cell->pkt.stat.MPI_SOURCE;
    status->MPI_TAG = cell->pkt.stat.MPI_TAG;
    status->MPI_ERROR = cell->pkt.stat.MPI_ERROR;

    CSP_DBG_PRINT("OFFLOAD req_query: req=0x%x, cell=%p, assign_cell=%p, "
                  "stat.src=%d, tag=%d, err=%d\n", req, cell,
                  assign_cell, status->MPI_SOURCE, status->MPI_TAG, status->MPI_ERROR);

    return status->MPI_ERROR;
}

/* NOTE: free_fn is invoked after the call to query_fn for the same request.
 * MPI will free the grequest object after this call. */
static int CSPU_offload_req_free_fn(void *extra_state)
{
    CSP_offload_cell_t *assign_cell = (CSP_offload_cell_t *) extra_state;
    CSP_offload_cell_t *cell = NULL;
    MPI_Request req = assign_cell->pkt.req;

    /* Remove shared cell from request -> cell hash. */
    CSPU_offload_req_hash_remove(req, &cell);
    CSP_DBG_ASSERT(cell->type == CSP_OFFLOAD_CELL_SHM);

    /* The cell is actually already completed a while, but it is reused only after
     * put back to freestk. So it is OK to decrement counter here.*/
    CSPU_offload_ch.shm_recvq.noutstanding--;

    /* A local pending cell may be assigned at request creation. Free it too. */
    if (assign_cell->type == CSP_OFFLOAD_CELL_PENDING) {
        CSP_DBG_ASSERT(cell != assign_cell);
        CSP_offload_release_pending_cell(&assign_cell);
    }

    CSP_offload_freestk_reset_cell(cell);
    CSP_offload_freestk_push(cell);

    CSP_DBG_PRINT("OFFLOAD req_free: free assign_cell %p cell %p\n", assign_cell, cell);

    return MPI_SUCCESS;
}

static int CSPU_offload_req_cancel_fn(void *extra_state, int complete)
{
    /* This generalized request does not support cancelling.
     * Abort if not already done.  If done then treat as if cancel failed. */
    if (!complete) {
        CSP_msg_print(CSP_MSG_ERROR, "Cannot cancel offloaded communication, abort !\n");
        CSP_ASSERT(0);
    }
    return MPI_SUCCESS;
}

int CSPU_offload_create_req(CSP_offload_cell_t * cell, MPI_Request * req_ptr)
{
    /* Because the request may be first generated for a local pending cell,
     * we use external hash to maintain request -> latest cell mapping. The
     * input cell is used to pass the generated request handle. */
    return PMPI_Grequest_start(CSPU_offload_req_query_fn, CSPU_offload_req_free_fn,
                               CSPU_offload_req_cancel_fn, (void *) cell, req_ptr);
}

/* Destroy offload channel.
 * This must be called after sent cwp finalize to ghost.  */
int CSPU_offload_destroy(void)
{
    int mpi_errno = MPI_SUCCESS;

    /* Check whether any outstanding or pending call exists.
     * User should ensure correct MPI program. */
    CSP_ASSERT(CSP_offload_pending_q_empty());
    CSP_ASSERT(pending_cell_ncreated == 0);
    CSP_ASSERT(CSP_offload_recvq_producer_empty(CSPU_offload_ch.shm_recvq.q_ptr) &&
               CSPU_offload_ch.shm_recvq.noutstanding == 0);

    if (CSPU_offload_ch.shm_win && CSPU_offload_ch.shm_win != MPI_WIN_NULL) {
        CSP_DBG_PRINT("OFFLOAD: free CSPU_offload_ch.shm_win 0x%x\n", CSPU_offload_ch.shm_win);
        CSP_CALLMPI(JUMP, PMPI_Win_free(&CSPU_offload_ch.shm_win));

        CSPU_offload_ch.shm_win = MPI_WIN_NULL;
        CSPU_offload_ch.shm_base = 0;
        CSPU_offload_ch.shm_recvq.q_ptr = NULL;
    }

    CSP_msg_print(CSP_MSG_INFO, "OFFLOAD: shm_recvq.nissued=%d, pending_q.nissued=%d\n",
                  CSPU_offload_ch.shm_recvq.nissued, CSPU_offload_ch.pending_q.nissued);


  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int CSPU_offload_init(void)
{
    int mpi_errno = MPI_SUCCESS;
    void *baseptr = NULL;
    MPI_Aint shm_region_size = 0, addr = 0;
    MPI_Aint align_cell_size = 0, align_shmq_size = 0;
    CSP_offload_cell_t *cell = NULL;
    int i;

    /* Make sure the shared structures are aligned by cache line. */
    align_cell_size = CSP_ALIGN(sizeof(CSP_offload_cell_t), CSP_OFFLOAD_CACHE_LINE_LEN);
    align_shmq_size = CSP_ALIGN(sizeof(CSP_offload_shmqueue_t), CSP_OFFLOAD_CACHE_LINE_LEN);

    /* Create shared memory region for pt2pt/collectives offload */
    /* [shm_recvq + 64 cells] per user process. Allocate on user process's
     * memory to ensure fast access. */
    shm_region_size = align_shmq_size + CSP_ENV.offload_shmq_ncells * align_cell_size;
    CSP_CALLMPI(JUMP, PMPI_Win_allocate_shared(shm_region_size, sizeof(char),
                                               MPI_INFO_NULL, CSP_PROC.local_comm,
                                               &baseptr, &CSPU_offload_ch.shm_win));
    CSPU_offload_ch.shm_base = (MPI_Aint) baseptr;
    CSPU_offload_ch.shm_recvq.q_ptr = (CSP_offload_shmqueue_t *) CSPU_offload_ch.shm_base;

    /* Not sure if win_allocate_shared gives an aligned start address. */
    if (!CSP_ALIGNED(CSPU_offload_ch.shm_recvq.q_ptr, CSP_OFFLOAD_CACHE_LINE_LEN))
        CSP_msg_print(CSP_MSG_WARN, "The shm_recvq %p is not aligned by %d !\n",
                      CSPU_offload_ch.shm_recvq.q_ptr, CSP_OFFLOAD_CACHE_LINE_LEN);

    /* Initialize local shm_recvq. */
    offload_shm_recvq_init();

    /* Ensure no ghost accesses shm_recvq before my initialization. */
    CSP_CALLMPI(JUMP, PMPI_Barrier(CSP_PROC.local_comm));

    CSP_DBG_PRINT("OFFLOAD: allocated shm_recvq.q_ptr %p\n", CSPU_offload_ch.shm_recvq.q_ptr);

    /* Initialize local containers */
    offload_freestk_init();
    offload_pending_q_init();

    /* Push all free cells into local stack */
    addr = CSPU_offload_ch.shm_base + align_shmq_size;
    for (i = 0; i < CSP_ENV.offload_shmq_ncells; i++) {
        cell = (CSP_offload_cell_t *) addr;
        cell->type = CSP_OFFLOAD_CELL_SHM;
        CSP_offload_freestk_reset_cell(cell);
        CSP_offload_freestk_push(cell);

        CSP_DBG_PRINT("OFFLOAD: pushed free cell %p\n", cell);
        addr += align_cell_size;
    }

    mpi_errno = CSPU_offload_bind_ghost(&CSPU_offload_ch.bound_g_lrank);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);
    CSP_DBG_PRINT("OFFLOAD: bound ghost %d\n", CSPU_offload_ch.bound_g_lrank);

  fn_exit:
    return mpi_errno;
  fn_fail:
    /* Free global objects in main function. */
    goto fn_exit;
}

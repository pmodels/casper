/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSPU_offload_ch_H_
#define CSPU_offload_ch_H_

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "csp.h"
#include "csp_util.h"
#include "csp_offload.h"

typedef struct CSPU_offload_req_hash {
    CSP_offload_cell_t *record;
} CSPU_offload_req_hash_t;

/* User offload structure for pt2pt and collectives. */
typedef struct CSP_offload_channel {
    MPI_Aint shm_base;
    MPI_Win shm_win;

    int bound_g_lrank;
    int *bound_g_lranks_local;  /* The bound ghost's local rank of every user
                                 * in local user comm. */

    CSP_offload_tag_trans_t tag_trans;

    /* Shared recvq, enqueued by local user and dequeued by a ghost. */
    struct {
        CSP_offload_shmqueue_t *q_ptr;
        int nissued, noutstanding;      /* DEBUG only */
    } shm_recvq;

    /* Local stack holds free shared cells.
     * Each element is preallocated from shared memory region, thus
     * avoid copy when move from/to shm_recvq. */
    struct {
        CSP_offload_cell_t *top;
        int count;              /* DEBUG only */
    } freestk;

    /* Local queue holds pending cells when no free shared cell available.
     * Each element allocates additional local memory.
     * TODO: how to avoid copy when move pending cell to recvq ? */
    struct {
        CSP_offload_cell_t *head;
        int nissued, noutstanding;      /* DEBUG only */
    } pending_q;

    CSPU_offload_req_hash_t req_hash;
} CSP_offload_channel_t;

/* Every user process has only one channel.
 * Different ghost process may poll it (currently only the bound ghost process polls).*/
extern CSP_offload_channel_t CSPU_offload_ch;


extern int CSPU_offload_init(void);
extern int CSPU_offload_destroy(void);

/* ======================================================================
 * Request hash routines for request and cell mapping on local process
 * TODO: These routines are not thread safe. Need fix for multithreaded program.
 * ====================================================================== */

static inline void CSPU_offload_req_hash_replace(CSP_offload_cell_t * new,
                                                 CSP_offload_cell_t ** old_ptr)
{
    CSP_offload_cell_t *old = NULL;

    /* First try to find if the key already exists */
    HASH_FIND(hh, (CSPU_offload_ch.req_hash.record), &new->pkt.req, sizeof(MPI_Request), old);
    if (old != NULL) {
        /* Remove old record */
        HASH_DEL((CSPU_offload_ch.req_hash.record), old);
    }
    /* Insert new record to hash */
    new->key = new->pkt.req;
    HASH_ADD(hh, (CSPU_offload_ch.req_hash.record), key, sizeof(MPI_Request), new);

    (*old_ptr) = old;
}

static inline void CSPU_offload_req_hash_get(MPI_Request req, CSP_offload_cell_t ** record_ptr)
{
    CSP_offload_cell_t *record = NULL;

    HASH_FIND(hh, (CSPU_offload_ch.req_hash.record), &req, sizeof(MPI_Request), record);
    (*record_ptr) = record;
}

static inline void CSPU_offload_req_hash_remove(MPI_Request req, CSP_offload_cell_t ** record_ptr)
{
    CSP_offload_cell_t *record = NULL;

    HASH_FIND(hh, (CSPU_offload_ch.req_hash.record), &req, sizeof(MPI_Request), record);
    if (record != NULL)
        HASH_DEL((CSPU_offload_ch.req_hash.record), record);

    (*record_ptr) = record;
}

/* ======================================================================
 * Stack routines for free cells on local process
 * TODO: These routines are not thread safe. Need fix for multithreaded program.
 * ====================================================================== */

static inline void CSP_offload_freestk_reset_cell(CSP_offload_cell_t * cell)
{
    memset(cell, 0, sizeof(CSP_offload_cell_t));
    CSP_offload_cell_reset_abs(cell);
    cell->type = CSP_OFFLOAD_CELL_SHM;
}

static inline int CSP_offload_freestk_empty(void)
{
    return (CSPU_offload_ch.freestk.top == NULL);
}

static inline void CSP_offload_freestk_push(CSP_offload_cell_t * cell_ptr)
{
    /* add at head */
    DL_PREPEND2(CSPU_offload_ch.freestk.top, cell_ptr, CSP_OFFLOAD_ABS_PT_DECL(prev),
                CSP_OFFLOAD_ABS_PT_DECL(next));
    CSPU_offload_ch.freestk.count++;
}

static inline void CSP_offload_freestk_pop(CSP_offload_cell_t ** cell_ptr)
{
    if (CSPU_offload_ch.freestk.top == NULL) {
        CSP_DBG_ASSERT(CSPU_offload_ch.freestk.count == 0);
        return;
    }

    /* delete head */
    (*cell_ptr) = CSPU_offload_ch.freestk.top;
    DL_DELETE2(CSPU_offload_ch.freestk.top, *cell_ptr, CSP_OFFLOAD_ABS_PT_DECL(prev),
               CSP_OFFLOAD_ABS_PT_DECL(next));
    CSPU_offload_ch.freestk.count--;
}


/* ======================================================================
 * Pending queue routines for pending offload calls on local process.
 * Store local calls when no free cell.
 * TODO: These routines are not thread safe. Need fix for multithreaded program.
 * TODO: Can we create a free cell pool shared by all users on a node ? This
 * helps uneven issuing.
 * ====================================================================== */

static inline int CSP_offload_pending_q_empty(void)
{
    return (CSPU_offload_ch.pending_q.head == NULL);
}

static inline void CSP_offload_pending_q_enqueue(CSP_offload_cell_t * cell_ptr)
{
    /* tail is internally stored at head->prev. */
    DL_APPEND2(CSPU_offload_ch.pending_q.head, cell_ptr, CSP_OFFLOAD_ABS_PT_DECL(prev),
               CSP_OFFLOAD_ABS_PT_DECL(next));

    CSPU_offload_ch.pending_q.nissued++;
    CSPU_offload_ch.pending_q.noutstanding++;
}

static inline void CSP_offload_pending_q_dequeue(CSP_offload_cell_t ** cell_ptr)
{
    if (CSPU_offload_ch.pending_q.head == NULL) {
        CSP_DBG_ASSERT(CSPU_offload_ch.pending_q.noutstanding == 0);
        return;
    }

    /* delete head */
    (*cell_ptr) = CSPU_offload_ch.pending_q.head;
    DL_DELETE2(CSPU_offload_ch.pending_q.head, *cell_ptr, CSP_OFFLOAD_ABS_PT_DECL(prev),
               CSP_OFFLOAD_ABS_PT_DECL(next));
    CSPU_offload_ch.pending_q.noutstanding--;
}

/* TODO: allocate pending cell pool. */
extern int pending_cell_ncreated;       /* DEBUG only */
static inline CSP_offload_cell_t *CSP_offload_create_pending_cell(void)
{
    CSP_offload_cell_t *cell_ptr = NULL;
    cell_ptr = CSP_calloc(1, sizeof(CSP_offload_cell_t));
    cell_ptr->type = CSP_OFFLOAD_CELL_PENDING;
    pending_cell_ncreated++;

    return cell_ptr;
}

static inline void CSP_offload_release_pending_cell(CSP_offload_cell_t ** cell_ptr)
{
    free(*cell_ptr);
    pending_cell_ncreated--;
}

/* ======================================================================
 * Offload issuing routines.
 * ====================================================================== */
extern int CSPU_offload_create_req(CSP_offload_cell_t * cell, MPI_Request * req_ptr);

static inline int CSPU_offload_check_complete(CSP_offload_cell_t * cell)
{
    return OPA_load_int(&cell->pkt.complet_flag);
}

static inline int CSPU_offload_new_cell(CSP_offload_cell_t ** cell_ptr)
{
    CSP_offload_cell_t *cell = NULL, *old_record = NULL;
    int mpi_errno = MPI_SUCCESS;

    /* Try to get free cell */
    CSP_offload_freestk_pop(&cell);

    /* Create a temporary pending cell if no free cell available */
    if (cell == NULL) {
        cell = CSP_offload_create_pending_cell();
        CSP_ASSERT(cell != NULL);
    }

    mpi_errno = CSPU_offload_create_req(cell, &cell->pkt.req);
    CSP_CHKMPIFAIL_RETURN(mpi_errno);

    CSPU_offload_req_hash_replace(cell, &old_record);
    CSP_ASSERT(old_record == NULL);     /* Previous cell record is not correctly cleaned. */

    *cell_ptr = cell;

    CSP_DBG_PRINT("OFFLOAD: new cell %p, %s, req=0x%x\n", cell,
                  (cell->type == CSP_OFFLOAD_CELL_SHM ? "shm" : "pending"), cell->pkt.req);
    return mpi_errno;
}

static inline void CSPU_offload_issue(CSP_offload_cell_t * cell)
{
    if (cell->type == CSP_OFFLOAD_CELL_SHM) {
        /* Enqueue to shared recvq, then the bound ghost process will handle it. */

        CSP_offload_cell_reset_rl(cell);

        CSP_offload_recvq_enqueue(CSPU_offload_ch.shm_base, CSPU_offload_ch.shm_recvq.q_ptr, cell);
        CSPU_offload_ch.shm_recvq.noutstanding++;
        CSPU_offload_ch.shm_recvq.nissued++;

        CSP_DBG_PRINT("OFFLOAD issue: enqueue shm_recvq cell %p, req=0x%x, count %d/%d\n",
                      cell, cell->pkt.req, CSPU_offload_ch.shm_recvq.noutstanding,
                      CSPU_offload_ch.shm_recvq.nissued);
    }
    else {
        /* Enqueue to local pending queue. Later progress polling will move it to
         * recvq once free cell is available. */

        CSP_offload_pending_q_enqueue(cell);

        CSP_DBG_PRINT("OFFLOAD issue: enqueue pending_q cell %p, req=0x%x, count %d/%d\n",
                      cell, cell->pkt.req, CSPU_offload_ch.pending_q.noutstanding,
                      CSPU_offload_ch.pending_q.nissued);
    }
}

static inline void CSPU_offload_poll_progress(void)
{
    /* Try to clean up pending cells as many as we can */
    while (!CSP_offload_pending_q_empty()) {
        CSP_offload_cell_t *pending_c = NULL;
        CSP_offload_cell_t *free_c = NULL;
        CSP_offload_cell_t *old_record = NULL;

        /* All shared cells are used. */
        if (CSP_offload_freestk_empty())
            break;

        /* Try to get free cell first */
        CSP_offload_freestk_pop(&free_c);
        if (free_c) {
            CSP_offload_cell_reset_rl(free_c);

            /* Get local pending cell and copy */
            CSP_offload_pending_q_dequeue(&pending_c);
            CSP_ASSERT(pending_c != NULL);

            memcpy(&free_c->pkt, &pending_c->pkt, sizeof(CSP_offload_pkt_t));

            /* Replace request hash record */
            CSPU_offload_req_hash_replace(free_c, &old_record);
            CSP_DBG_ASSERT(old_record == pending_c);

            /* Enqueue to shared recvq */
            CSP_offload_recvq_enqueue(CSPU_offload_ch.shm_base,
                                      CSPU_offload_ch.shm_recvq.q_ptr, free_c);
            CSPU_offload_ch.shm_recvq.noutstanding++;
            CSPU_offload_ch.shm_recvq.nissued++;

            CSP_DBG_PRINT("OFFLOAD progress: pending_c %p -> free_c %p, req=0x%x, "
                          "pending count %d/%d, shm_recvq count %d/%d\n",
                          pending_c, free_c, free_c->pkt.req,
                          CSPU_offload_ch.pending_q.noutstanding, CSPU_offload_ch.pending_q.nissued,
                          CSPU_offload_ch.shm_recvq.noutstanding,
                          CSPU_offload_ch.shm_recvq.nissued);

            /* Do not release pending cell at replace because we need to get request
             * handler at grequest callback. Instead we release it at free. */
        }
    }
}

static inline int CSPU_offload_bind_ghost(int *ghost_local_rank)
{
    int mpi_errno = MPI_SUCCESS;
    int local_rank, local_size, g_lrank = 0;
    int np_per_ghost = 0;

    CSP_CALLMPI(RETURN, PMPI_Comm_rank(CSP_PROC.local_comm, &local_rank));
    CSP_CALLMPI(RETURN, PMPI_Comm_size(CSP_PROC.local_comm, &local_size));

    np_per_ghost = (local_size - CSP_ENV.num_g) / CSP_ENV.num_g;
    if (np_per_ghost < 1)
        np_per_ghost = 1;       /* in case user sets more ghosts than user processes */

    g_lrank = (local_rank - CSP_ENV.num_g) / np_per_ghost;
    if (g_lrank >= CSP_ENV.num_g)
        g_lrank = CSP_ENV.num_g - 1;

    (*ghost_local_rank) = g_lrank;

    return mpi_errno;
}

static inline int CSPU_offload_get_ghost(void)
{
    return CSPU_offload_ch.bound_g_lrank;
}

static inline void CSPU_offload_init_pkt(CSP_offload_pkt_t * pkt, CSPU_comm_t * ug_comm,
                                         CSP_offload_pkt_type_t type)
{
    pkt->type = type;
    OPA_store_int(&pkt->complet_flag, 0);
    memset(&pkt->stat, 0, sizeof(MPI_Status));

    pkt->ug_comm_handle = (MPI_Aint) ug_comm;
    /* request is set at new_cell. */
}
#endif /* CSPU_offload_ch_H_ */

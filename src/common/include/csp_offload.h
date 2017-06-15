/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSP_OFFLOAD_H_INCLUDED
#define CSP_OFFLOAD_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <casperconf.h>
#include "csp_util.h"
#include "csp_msg.h"
#include "csp_error.h"
#if defined(CSP_ENABLE_THREAD_SAFE)
#include "csp_thread.h"
#endif

#include "opa_primitives.h"

/* ======================================================================
 * Communication offload definition.
 * This mechanism is used only for pt2pt and collective calls.
 * Also see csp_datatype.h.
 * ====================================================================== */

/* Default number of pre-allocated cells on each user process.
 * If all cells are enqueued into the shared recvq (offloaded), the user
 * process has to wait until one outstanding offloaded call finish.
 * Also see offload_shmq_ncells in CSP_env_param_t struct.  */
#define CSP_DEFAULT_OFFLOAD_SHMQ_NCELLS 64
#define CSP_OFFLOAD_TAG_FACTOR 100
#define CSP_OFFLOAD_SHMQ_MEMSZ(ncells) (ncells * sizeof(CSP_offload_cell_t))

typedef enum {
    CSP_OFFLOAD_ISEND = 0,
    CSP_OFFLOAD_IRECV,
    CSP_OFFLOAD_MAX
} CSP_offload_pkt_type_t;

typedef struct CSP_offload_isend_pkt {
    int rank;                   /* The user rank in ug_comm. Used to update tag on
                                 * ghost call to workaround the mismatching on
                                 * shared ghost issue...*/
    int peer_rank;              /* The peer rank, used to fill status. */
    int g_peer_rank;            /* The peer's ghost rank in ug_comm. */
    int tag;
    MPI_Aint g_bufaddr;         /* The absolute address of user buffer on ghost process */
    int count;
    MPI_Datatype g_datatype;    /* The handle on ghost process */
    MPI_Comm g_ugcomm;          /* The handle of ug_comm on ghost process */
} CSP_offload_isend_pkt_t;
typedef CSP_offload_isend_pkt_t CSP_offload_irecv_pkt_t;

typedef struct CSP_offload_pkt {
    CSP_offload_pkt_type_t type;
    union {
        CSP_offload_isend_pkt_t isend;
        CSP_offload_irecv_pkt_t irecv;
    };
    OPA_int_t complet_flag;     /* 0|1. Ghost sets to 1 after issued call is locally
                                 * completed.*/
    MPI_Status stat;            /* Set by ghost and read by user process.
                                 * User accesses it only when complet_flag is 1,
                                 * so do not need atomic access to it.*/

    /* FIXME: put local objects also in the shared packet to simply avoid additional
     * allocation/free. We should optimize them by using local pool rather than
     * putting in the shared region.*/
    MPI_Request req;            /* Only accessed by user process. Complete the request
                                 * when complet_flag = 1.*/
    MPI_Request g_req;          /* Only accessed by ghost process. Once the request is
                                 * completed, set complet_flag = 1.*/
} CSP_offload_pkt_t;

/* Relative offset of a cell's start address */
typedef OPA_ptr_t CSP_offload_cell_rl_ptr_t;

typedef enum {
    /* TODO: bad naming... */
    CSP_OFFLOAD_CELL_SHM = 0,
    CSP_OFFLOAD_CELL_PENDING = 1
} CSP_offload_cell_type_t;

typedef struct CSP_offload_cell {
    CSP_offload_cell_type_t type;
    CSP_offload_pkt_t pkt;

    /* Hash structure for request->cell mapping on user process. */
    UT_hash_handle hh;
    MPI_Request key;

    /* Note that the same cell can be stored in either local stack
     * or shared queue. Each routine should use the corresponding
     * pointers, and always reset before use. */
    union {
        /* Pointers used in locally managed stack; */
        struct {
            struct CSP_offload_cell *next, *prev;
        } abs;
        /* Pointers used in shared queue; */
        struct {
            CSP_offload_cell_rl_ptr_t next, prev;       /* Relative offset */
        } rl;
    } pt;
} CSP_offload_cell_t;

#define CSP_OFFLOAD_ABS_PT_DECL(pointer) pt.abs.pointer
#define CSP_OFFLOAD_CELL_ABS_PT(cell_ptr) ((cell_ptr)->pt.abs)
#define CSP_OFFLOAD_CELL_RL_PT(cell_ptr) ((cell_ptr)->pt.rl)
#define CSP_OFFLOAD_RL_NULL (0x0)
#define CSP_OFFLOAD_IS_RL_NULL(rl) (OPA_load_ptr(&(rl)) == CSP_OFFLOAD_RL_NULL)
#define CSP_OFFLOAD_SET_RL_NULL(rl) OPA_store_ptr(&(rl), CSP_OFFLOAD_RL_NULL)
#define CSP_OFFLOAD_RL_EQUAL(rl1, rl2) (OPA_load_ptr(&(rl1)) == OPA_load_ptr(&(rl2)))

typedef struct {
    CSP_offload_cell_rl_ptr_t head;     /* Need atomic access when queue was empty:
                                         * producer updates it at enqueue, and
                                         * consumer checks+loads it at empty.  */
    CSP_offload_cell_rl_ptr_t tail;     /* Need atomic access: producer updates it
                                         * at enqueue, and consumer may check+reset
                                         * it at dequeue.*/
    CSP_offload_cell_rl_ptr_t my_head;  /* local head used only by consumer.
                                         * Synced with head at empty, and updated
                                         * at dequeue.*/
} CSP_offload_shmqueue_t;

/* ======================================================================
 * Queue routines for cells offloaded from user process to ghost process.
 *
 * NOTE:
 * - These routines must be thread-safe for Single-Producer-Single-Consumer.
 * - The next and prev pointers must be translated to relative offset for
 *   shared queue. Because each of user or ghost process may have different
 *   base address of the shared memory region.
 * - Copied code from MPICH nemesis shm recvQ portion.
 * ====================================================================== */

static inline CSP_offload_cell_t *CSP_offload_cell_rl2abs(MPI_Aint base,
                                                          CSP_offload_cell_rl_ptr_t r)
{
    return (CSP_offload_cell_t *) ((char *) OPA_load_ptr(&r) + base);
}

static inline CSP_offload_cell_rl_ptr_t CSP_offload_cell_abs2rl(MPI_Aint base,
                                                                CSP_offload_cell_t * a)
{
    CSP_offload_cell_rl_ptr_t rl;
    OPA_store_ptr(&rl, (char *) a - base);
    return rl;
}

/* Because we use move the same cell instance between shm_recvq which uses
 * relative address, and local freestk which uses absolute address, we need
 * reset the cell instance pointers every time when moves to the other container.*/
static inline void CSP_offload_cell_reset_rl(CSP_offload_cell_t * cell)
{
    CSP_OFFLOAD_SET_RL_NULL(CSP_OFFLOAD_CELL_RL_PT(cell).next);
    CSP_OFFLOAD_SET_RL_NULL(CSP_OFFLOAD_CELL_RL_PT(cell).prev);
}

static inline void CSP_offload_cell_reset_abs(CSP_offload_cell_t * cell)
{
    CSP_OFFLOAD_CELL_ABS_PT(cell).next = NULL;
    CSP_OFFLOAD_CELL_ABS_PT(cell).prev = NULL;
}

/* Swaps with the new value and returns the old value */
static inline CSP_offload_cell_rl_ptr_t CSP_offload_cell_rl_swap(CSP_offload_cell_rl_ptr_t * ptr,
                                                                 CSP_offload_cell_rl_ptr_t val)
{
    CSP_offload_cell_rl_ptr_t ret;
    OPA_store_ptr(&ret, OPA_swap_ptr(ptr, OPA_load_ptr(&val)));
    return ret;
}

/* Compare-and-swap with CSP_OFFLOAD_RL_NULL */
static inline CSP_offload_cell_rl_ptr_t CSP_offload_cell_rl_cas_null(CSP_offload_cell_rl_ptr_t *
                                                                     ptr,
                                                                     CSP_offload_cell_rl_ptr_t oldv)
{
    CSP_offload_cell_rl_ptr_t ret;
    OPA_store_ptr(&ret, OPA_cas_ptr(ptr, OPA_load_ptr(&oldv), CSP_OFFLOAD_RL_NULL));
    return ret;
}

/* Empty queried only by producer. */
static inline int CSP_offload_recvq_producer_empty(CSP_offload_shmqueue_t * q)
{
    return CSP_OFFLOAD_IS_RL_NULL(q->tail);
}

/* Empty queried only by consumer. */
static inline int CSP_offload_recvq_consumer_empty(MPI_Aint base, CSP_offload_shmqueue_t * q)
{
    /* outside of this routine my_head and head should never both
     * contain a non-null value */
    CSP_DBG_ASSERT(CSP_OFFLOAD_IS_RL_NULL(q->my_head) || CSP_OFFLOAD_IS_RL_NULL(q->head));

    if (CSP_OFFLOAD_IS_RL_NULL(q->my_head)) {
        /* the order of comparison between my_head and head does not
         * matter, no read barrier needed here */
        if (CSP_OFFLOAD_IS_RL_NULL(q->head)) {
            /* both null, nothing in queue */
            return 1;
        }
        else {
            /* shadow head null and head has value, move the value to
             * our private shadow head and zero the real head */
            q->my_head = q->head;
            /* no barrier needed, my_head is entirely private to consumer */
            CSP_OFFLOAD_SET_RL_NULL(q->head);
        }
    }

    return 0;
}

static inline void CSP_offload_recvq_enqueue(MPI_Aint base, CSP_offload_shmqueue_t * q,
                                             CSP_offload_cell_t * cell)
{
    CSP_offload_cell_rl_ptr_t rl_old_tail;
    CSP_offload_cell_rl_ptr_t rl_cell = CSP_offload_cell_abs2rl(base, cell);

    /* Orders payload and e->next=NULL w.r.t. the SWAP, updating head, and
     * updating prev->next.  We assert e->next==NULL above, but it may have been
     * done by us in the preceding _dequeue operation.
     *
     * The SWAP itself does not need to be ordered w.r.t. the payload because
     * the consumer does not directly inspect the tail.  But the subsequent
     * update to the head or e->next field does need to be ordered w.r.t. the
     * payload or the consumer may read incorrect data. */
    OPA_write_barrier();

    /* enqueue at tail */
    rl_old_tail = CSP_offload_cell_rl_swap(&(q->tail), rl_cell);
    if (CSP_OFFLOAD_IS_RL_NULL(rl_old_tail)) {
        /* queue was empty, element is the new head too */

        /* no write barrier needed, we believe atomic SWAP with a control
         * dependence (if) will enforce ordering between the SWAP and the head
         * assignment */
        q->head = rl_cell;
    }
    else {
        /* queue was not empty, swing old tail's next field to point to
         * our element */

        CSP_DBG_ASSERT(CSP_OFFLOAD_IS_RL_NULL
                       (CSP_OFFLOAD_CELL_RL_PT(CSP_offload_cell_rl2abs(base, rl_old_tail)).next));

        /* no write barrier needed, we believe atomic SWAP with a control
         * dependence (if/else) will enforce ordering between the SWAP and the
         * prev->next assignment */
        CSP_OFFLOAD_CELL_RL_PT(CSP_offload_cell_rl2abs(base, rl_old_tail)).next = rl_cell;
    }
}

static inline void CSP_offload_recvq_dequeue(MPI_Aint base, CSP_offload_shmqueue_t * q,
                                             CSP_offload_cell_t ** cell_ptr)
{
    CSP_offload_cell_rl_ptr_t rl_old_head;
    CSP_offload_cell_t *old_head = NULL;

    /* _empty always called first, it moves head-->my_head */
    CSP_DBG_ASSERT(!CSP_OFFLOAD_IS_RL_NULL(q->my_head));
    CSP_DBG_ASSERT(CSP_OFFLOAD_IS_RL_NULL(q->head));

    rl_old_head = q->my_head;
    old_head = CSP_offload_cell_rl2abs(base, rl_old_head);

    /* no barrier needed, my_head is private to consumer, plus
     * head/my_head and _e->next are ordered by a data dependency */
    if (CSP_OFFLOAD_IS_RL_NULL(CSP_OFFLOAD_CELL_RL_PT(old_head).next)) {
        /* we've reached the end (tail) of the queue */
        CSP_offload_cell_rl_ptr_t rl_old_tail;

        CSP_OFFLOAD_SET_RL_NULL(q->my_head);

        /* no barrier needed, the caller doesn't need any ordering w.r.t.
         * my_head or the tail */
        rl_old_tail = CSP_offload_cell_rl_cas_null(&(q->tail), rl_old_head);

        if (!CSP_OFFLOAD_RL_EQUAL(rl_old_head, rl_old_tail)) {
            /* FIXME is a barrier needed here because of the control-only dependency? */
            while (CSP_OFFLOAD_IS_RL_NULL(CSP_OFFLOAD_CELL_RL_PT(old_head).next)) {
                /* tail has been changed by producer after old_header.next == NULL
                 * condition check. Here we wait the producer to add new cell into
                 * tail.*/
            }
        }
    }

    /* old_head.next may be changed only when the above inner branch happens,
     * but no read barrier needed between loads from the same location */
    q->my_head = CSP_OFFLOAD_CELL_RL_PT(old_head).next;

    CSP_OFFLOAD_SET_RL_NULL(CSP_OFFLOAD_CELL_RL_PT(old_head).next);

    /* Conservative read barrier here to ensure loads from head are ordered
     * w.r.t. payload reads by the caller.  The McKenney "whymb" document's
     * Figure 11 indicates that we don't need a barrier, but we are currently
     * unconvinced of this.  Further work, ideally using more formal methods,
     * should justify removing this.  (note that this barrier won't cost us
     * anything on many platforms, esp. x86) */
    OPA_read_barrier();

    *cell_ptr = old_head;
}


#endif /* CSP_OFFLOAD_H_INCLUDED */

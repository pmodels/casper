/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSPG_offload_ch_H_
#define CSPG_offload_ch_H_

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "cspg.h"
#include "csp_util.h"
#include "csp_offload.h"

typedef int (*CSPG_offload_handler_t) (CSP_offload_pkt_t * cell);
typedef void (*CSPG_offload_cmpl_handler_t) (CSP_offload_pkt_t * pkt, MPI_Status g_stat);

/* Ghost offload channel connecting to single each user */
typedef struct CSPG_offload_channel {
    MPI_Aint shm_base;

    /* shm recvq, enqueued by local user and dequeued by a ghost */
    CSP_offload_shmqueue_t *shm_recvq_ptr;
    int shm_recved_cnt;         /* DEBUG only */
} CSPG_offload_channel_t;

typedef struct CSPG_offload_server {
    MPI_Win shm_win;

    CSP_offload_tag_trans_t tag_trans;

    /* FIXME: creates channel for all local users.
     * This is unnecessary unless we do dynamic ghost binding.*/
    CSPG_offload_channel_t *channels;

    /* Range of bound local users */
    struct {
        int lrank_sta;
        int lrank_end;
    } urange;

    /* local issued queue, holding issued but incompleted cells. */
    struct {
        CSP_offload_cell_t *head;
        int nissued, noutstanding;      /* DEBUG only */
    } issued_list;

    /* Offload packet handlers on ghost.
     * The handler is called when polled a offload cell from a user channel. */
    CSPG_offload_handler_t pkt_handlers[CSP_OFFLOAD_MAX];

    /* Completion handlers on ghost.
     * The handler is called when an issued packet is completed on ghost.*/
    CSPG_offload_cmpl_handler_t cmpl_handlers[CSP_OFFLOAD_MAX];
} CSPG_offload_server_t;

extern CSPG_offload_server_t CSPG_offload_server;

/* ======================================================================
 * Double-linked list routines for holding issued but incomplete cells on
 * ghost process. Each cell is the same instance as dequeued from shm_recvq.
 * TODO: These routines are not thread safe. Need fix for multithreaded program.
 * ====================================================================== */

/* Should always reset the cell before moving from the shared queue. */
static inline void CSPG_offload_issued_list_reset_cellpt(CSP_offload_cell_t * cell)
{
    CSP_offload_cell_reset_abs(cell);
}

static inline int CSPG_offload_issued_list_empty(void)
{
    return (CSPG_offload_server.issued_list.head == NULL);
}

static inline void CSPG_offload_issued_list_remove(CSP_offload_cell_t ** cell_ptr)
{
    CSP_ASSERT(CSPG_offload_server.issued_list.head != NULL);

    DL_DELETE2(CSPG_offload_server.issued_list.head, *cell_ptr, CSP_OFFLOAD_ABS_PT_DECL(prev),
               CSP_OFFLOAD_ABS_PT_DECL(next));
    CSPG_offload_server.issued_list.noutstanding--;
}

static inline void CSPG_offload_issued_list_append(CSP_offload_cell_t * cell_ptr)
{
    /* tail is internally stored at head->prev. */
    DL_APPEND2(CSPG_offload_server.issued_list.head, cell_ptr, CSP_OFFLOAD_ABS_PT_DECL(prev),
               CSP_OFFLOAD_ABS_PT_DECL(next));
    CSPG_offload_server.issued_list.nissued++;
    CSPG_offload_server.issued_list.noutstanding++;
}

/* ======================================================================
 * Other offload related routines.
 * ====================================================================== */
extern int CSPG_offload_init(void);
extern int CSPG_offload_destroy(void);
extern int CSPG_offload_poll_progress(void);

extern int CSPG_isend_offload_handler(CSP_offload_pkt_t * pkt);
extern void CSPG_isend_cmpl_handler(CSP_offload_pkt_t * pkt, MPI_Status g_stat);

extern int CSPG_irecv_offload_handler(CSP_offload_pkt_t * pkt);
extern void CSPG_irecv_cmpl_handler(CSP_offload_pkt_t * pkt, MPI_Status g_stat);

#define CSPG_TRANS_TAG(tag, off) (tag + (off << CSPG_offload_server.tag_trans.user_tag_nbits))
#define CSPG_TRANS_TAG_UTAG(tag) (tag & CSPG_offload_server.tag_trans.user_tag_mask)
#define CSPG_TRANS_TAG_OFF(tag) ((tag & CSPG_offload_server.tag_trans.trans_tag_mask) >> \
                                    CSPG_offload_server.tag_trans.user_tag_nbits)

/* Becaues of rank reorder at ug_comm creation (see user's ugcomm_gather_ranks),
 * user ug_rank = ghost ug_rank + offset. */
#define CSPG_UGCOMM_RANK2OFF(g_ugrank, ugrank) (ugrank - g_ugrank - 1)
#define CSPG_UGCOMM_OFF2RANK(g_ugrank, offset) (g_ugrank + offset + 1)
#endif /* CSPG_offload_ch_H_ */

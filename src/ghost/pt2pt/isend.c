/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cspg.h"

void CSPG_isend_cmpl_handler(CSP_offload_pkt_t * pkt, MPI_Status g_stat)
{
    /* Do not set status for send message. */

    /* Set completion */
    OPA_store_int(&pkt->complet_flag, 1);

    CSPG_DBG_PRINT("OFFLOAD, isend cmpl: pkt=%p\n", pkt);
}


int CSPG_isend_offload_handler(CSP_offload_pkt_t * pkt)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_offload_isend_pkt_t *isend_pkt = &pkt->isend;
    int rank = 0, tag = 0;

    /* Use tag to distinguish different receive ranks on the same ghost.
     * TODO: allow user to provide hint */
    tag = isend_pkt->tag + isend_pkt->peer_rank * CSP_OFFLOAD_TAG_FACTOR;

    /* We trust user always passes the valid buffer address. */
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(isend_pkt->g_ugcomm, &rank));
    CSP_CALLMPI(JUMP, PMPI_Isend((const void *) isend_pkt->g_bufaddr, isend_pkt->count,
                                 isend_pkt->g_datatype, isend_pkt->g_peer_rank, tag,
                                 isend_pkt->g_ugcomm, &pkt->g_req));

    CSPG_DBG_PRINT("OFFLOAD, isend pkt=%p, req=0x%x, buf 0x%lx, count %d, g_datatype 0x%x,"
                   "peer %d/%d(my %d), tag %d->%d, g_ugcomm 0x%x\n", pkt, pkt->g_req,
                   isend_pkt->g_bufaddr, isend_pkt->count, isend_pkt->g_datatype,
                   isend_pkt->peer_rank, isend_pkt->g_peer_rank, rank, isend_pkt->tag,
                   tag, isend_pkt->g_ugcomm);

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspg.h"

void CSPG_irecv_cmpl_handler(CSP_offload_pkt_t * pkt, MPI_Status g_stat)
{
    MPI_Status *u_stat_ptr = &pkt->stat;

    /* Set up user status */
    u_stat_ptr->MPI_TAG = pkt->irecv.tag;
    u_stat_ptr->MPI_ERROR = g_stat.MPI_ERROR;

    /* FIXME: how to translate ANY_SOURCE ? */
    CSP_ASSERT(pkt->irecv.g_peer_rank != MPI_ANY_SOURCE);
    u_stat_ptr->MPI_SOURCE = pkt->irecv.peer_rank;

    /* Set completion */
    OPA_store_int(&pkt->complet_flag, 1);

    CSPG_DBG_PRINT("OFFLOAD, irecv cmpl: pkt=%p, stat.MPI_TAG %d, MPI_ERROR %d, MPI_SOURCE %d\n",
                   pkt, u_stat_ptr->MPI_TAG, u_stat_ptr->MPI_ERROR, u_stat_ptr->MPI_SOURCE);
}

int CSPG_irecv_offload_handler(CSP_offload_pkt_t * pkt)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_offload_irecv_pkt_t *irecv_pkt = &pkt->irecv;
    int rank, tag;

    /* Use tag to distinguish different receive ranks on the same ghost.
     * TODO: allow user to provide hint */
    tag = irecv_pkt->tag + irecv_pkt->rank * CSP_OFFLOAD_TAG_FACTOR;
    /* FIXME: this workaround does not support any_tag. */
    CSP_ASSERT(irecv_pkt->tag != MPI_ANY_TAG);

    /* We trust user always passes the valid buffer address. */
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(irecv_pkt->g_ugcomm, &rank));
    CSP_CALLMPI(JUMP, PMPI_Irecv((void *) irecv_pkt->g_bufaddr, irecv_pkt->count,
                                 irecv_pkt->g_datatype, irecv_pkt->g_peer_rank, tag,
                                 irecv_pkt->g_ugcomm, &pkt->g_req));

    CSPG_DBG_PRINT("OFFLOAD, irecv pkt=%p, req=0x%x, g_bufaddr 0x%lx, count %d, g_datatype 0x%x,"
                   "peer %d/%d(my %d), tag %d->%d, g_ugcomm 0x%x\n", pkt, pkt->g_req,
                   irecv_pkt->g_bufaddr, irecv_pkt->count, irecv_pkt->g_datatype,
                   irecv_pkt->peer_rank, irecv_pkt->g_peer_rank, rank, irecv_pkt->tag, tag,
                   irecv_pkt->g_ugcomm);

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

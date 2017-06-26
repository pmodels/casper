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
    CSP_offload_irecv_pkt_t *irecv_pkt = &pkt->irecv;
    CSPG_comm_t *cspg_comm = NULL;

    cspg_comm = (CSPG_comm_t *) irecv_pkt->g_ugcomm_handle;

    u_stat_ptr->MPI_SOURCE = pkt->irecv.peer_rank;
    u_stat_ptr->MPI_TAG = pkt->irecv.tag;
    u_stat_ptr->MPI_ERROR = g_stat.MPI_ERROR;

    if (cspg_comm->type == CSP_COMM_ASYNC) {
        /* Translate source ant tag. */
        /* FIXME: format tag. */
        if (irecv_pkt->g_peer_rank == MPI_ANY_SOURCE)
            u_stat_ptr->MPI_SOURCE = CSP_TRANS_TAG_OFF(g_stat.MPI_TAG);
        if (pkt->irecv.tag == MPI_ANY_TAG)
            u_stat_ptr->MPI_TAG = CSP_TRANS_TAG_UTAG(g_stat.MPI_TAG);
    }

    /* Set completion */
    OPA_store_int(&pkt->complet_flag, 1);

    CSPG_DBG_PRINT("OFFLOAD, irecv cmpl: pkt=%p, stat.MPI_TAG %d, MPI_ERROR %d, MPI_SOURCE %d\n",
                   pkt, u_stat_ptr->MPI_TAG, u_stat_ptr->MPI_ERROR, u_stat_ptr->MPI_SOURCE);
}

int CSPG_irecv_offload_handler(CSP_offload_pkt_t * pkt)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_offload_irecv_pkt_t *irecv_pkt = &pkt->irecv;
    CSPG_comm_t *cspg_comm = NULL;
    MPI_Comm ug_comm = MPI_COMM_NULL;
    int rank, tag;

    cspg_comm = (CSPG_comm_t *) irecv_pkt->g_ugcomm_handle;
    CSP_DBG_ASSERT(cspg_comm->type >= CSP_COMM_ASYNC);

    /* Use tag translation. */
    if (cspg_comm->type == CSP_COMM_ASYNC_NODUP) {
        /* Use tag to distinguish different receive ranks on the same ghost. */
        /* FIXME: format tag. */
        tag = CSP_TRANS_TAG(irecv_pkt->tag, irecv_pkt->recv_offset);
        ug_comm = cspg_comm->ug_comm;
    }
    /* Use dupcomm. */
    else {
        ug_comm = cspg_comm->dup_ug_comms[irecv_pkt->recv_offset];
        tag = irecv_pkt->tag;

        if (cspg_comm->wildcard_info & CSP_COMM_INFO_WD_ANYSRC)
            tag = MPI_ANY_TAG;
    }

    /* We trust user always passes the valid buffer address. */
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_comm, &rank));
    CSP_CALLMPI(JUMP, PMPI_Irecv((void *) irecv_pkt->g_bufaddr, irecv_pkt->count,
                                 irecv_pkt->g_datatype, irecv_pkt->g_peer_rank, tag,
                                 ug_comm, &pkt->g_req));

    CSPG_DBG_PRINT
        ("OFFLOAD (%s), irecv pkt=%p, req=0x%x, g_bufaddr 0x%lx, count %d, g_datatype 0x%x,"
         "peer %d/%d(my %d), tag %d->%d, g_ugcomm 0x%x (roff %d)\n",
         CSP_ug_comm_type_name[cspg_comm->type], pkt, pkt->g_req, irecv_pkt->g_bufaddr,
         irecv_pkt->count, irecv_pkt->g_datatype, irecv_pkt->peer_rank, irecv_pkt->g_peer_rank,
         rank, irecv_pkt->tag, tag, ug_comm, irecv_pkt->recv_offset);

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

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
        /* Translate source and tag.
         * Note that MPI_SOURCE is ugrank, need translation on user process.*/
        if (irecv_pkt->peer_rank == MPI_ANY_SOURCE)
            u_stat_ptr->MPI_SOURCE = CSPG_UGCOMM_OFF2RANK(g_stat.MPI_SOURCE,
                                                          CSPG_TRANS_TAG_OFF(g_stat.MPI_TAG));
        if (pkt->irecv.tag == MPI_ANY_TAG)
            u_stat_ptr->MPI_TAG = CSPG_TRANS_TAG_UTAG(g_stat.MPI_TAG);
    }

    /* Set completion */
    OPA_store_int(&pkt->complet_flag, 1);

    CSPG_DBG_PRINT
        ("OFFLOAD, irecv cmpl: pkt=%p; gstat.SOURCE %d, TAG 0x%x; stat.SOURCE %d, TAG %d, ERROR %d\n",
         pkt, g_stat.MPI_SOURCE, g_stat.MPI_TAG, u_stat_ptr->MPI_SOURCE, u_stat_ptr->MPI_TAG,
         u_stat_ptr->MPI_ERROR);
}

int CSPG_irecv_offload_handler(CSP_offload_pkt_t * pkt)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_offload_irecv_pkt_t *irecv_pkt = &pkt->irecv;
    CSPG_comm_t *cspg_comm = NULL;
    MPI_Comm ug_comm = MPI_COMM_NULL;
    int tag, peer_g_rank, recv_offset;
    int g_rank = 0;

    cspg_comm = (CSPG_comm_t *) irecv_pkt->g_ugcomm_handle;
    CSP_DBG_ASSERT(cspg_comm->type >= CSP_COMM_ASYNC);

    if (irecv_pkt->peer_rank != MPI_ANY_SOURCE)
        peer_g_rank = cspg_comm->g_ranks_bound[irecv_pkt->peer_rank];
    else
        peer_g_rank = MPI_ANY_SOURCE;

    recv_offset = CSPG_UGCOMM_RANK2OFF(cspg_comm->g_ranks_bound[irecv_pkt->rank],
                                       irecv_pkt->ugrank);

    /* Use tag translation with recv_offset. */
    if (cspg_comm->type == CSP_COMM_ASYNC_NODUP) {
        tag = CSPG_TRANS_TAG(irecv_pkt->tag, recv_offset);
        ug_comm = cspg_comm->ug_comm;
    }
    /* Use dupcomm[recv_offset]. */
    else {
        ug_comm = cspg_comm->dup_ug_comms[recv_offset];
        tag = irecv_pkt->tag;

        if (cspg_comm->wildcard_info & CSP_COMM_INFO_WD_ANYSRC)
            tag = MPI_ANY_TAG;
    }

    /* We trust user always passes the valid buffer address. */
    CSP_CALLMPI(JUMP, PMPI_Irecv((void *) irecv_pkt->g_bufaddr, irecv_pkt->count,
                                 irecv_pkt->g_datatype, peer_g_rank, tag, ug_comm, &pkt->g_req));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_comm, &g_rank));
    CSPG_DBG_PRINT("OFFLOAD (%s), irecv pkt=%p, req=0x%x, buf 0x%lx, count %d, g_datatype 0x%x,"
                   "me %d/%d (g %d), peer %d (g %d), tag %d->0x%x, g_ugcomm 0x%x (roff %d)\n",
                   CSP_ug_comm_type_name[cspg_comm->type], pkt, pkt->g_req, irecv_pkt->g_bufaddr,
                   irecv_pkt->count, irecv_pkt->g_datatype, irecv_pkt->rank, irecv_pkt->ugrank,
                   g_rank, irecv_pkt->peer_rank, peer_g_rank, irecv_pkt->tag, tag, ug_comm,
                   recv_offset);

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

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
    CSPG_comm_t *cspg_comm = NULL;
    MPI_Comm ug_comm = MPI_COMM_NULL;

    cspg_comm = (CSPG_comm_t *) isend_pkt->g_ugcomm_handle;
    CSP_DBG_ASSERT(cspg_comm->type >= CSP_COMM_ASYNC);

    /* Use tag translation. */
    if (cspg_comm->type == CSP_COMM_ASYNC_NODUP) {
        /* Use tag to distinguish different receive ranks on the same ghost. */
        /* FIXME: format tag. */
        tag = CSP_TRANS_TAG(isend_pkt->tag, isend_pkt->recv_offset);
        ug_comm = cspg_comm->ug_comm;
    }
    /* Use dupcomm. */
    else {
        ug_comm = cspg_comm->dup_ug_comms[isend_pkt->recv_offset];
        tag = isend_pkt->tag;

        if (cspg_comm->wildcard_info & CSP_COMM_INFO_WD_ANYSRC) {
            tag = CSP_TRANS_TAG(isend_pkt->tag, isend_pkt->send_offset);
        }
    }

    /* We trust user always passes the valid buffer address. */
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_comm, &rank));
    CSP_CALLMPI(JUMP, PMPI_Isend((const void *) isend_pkt->g_bufaddr, isend_pkt->count,
                                 isend_pkt->g_datatype, isend_pkt->g_peer_rank, tag,
                                 ug_comm, &pkt->g_req));

    CSPG_DBG_PRINT("OFFLOAD (%s), isend pkt=%p, req=0x%x, buf 0x%lx, count %d, g_datatype 0x%x,"
                   "peer %d/%d(my %d), tag %d->%d, g_ugcomm 0x%x(soff %d, roff %d)\n",
                   CSP_ug_comm_type_name[cspg_comm->type], pkt, pkt->g_req, isend_pkt->g_bufaddr,
                   isend_pkt->count, isend_pkt->g_datatype, isend_pkt->peer_rank,
                   isend_pkt->g_peer_rank, rank, isend_pkt->tag, tag, ug_comm,
                   isend_pkt->send_offset, isend_pkt->recv_offset);

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

static inline int isend_impl(MPI_Aint g_bufaddr, int count, MPI_Datatype datatype,
                             int dest, int tag, MPI_Comm comm, MPI_Request * request,
                             CSPU_comm_t * ug_comm)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_offload_cell_t *cell = NULL;
    CSP_offload_pkt_t *pkt = NULL;
    CSP_offload_isend_pkt_t *isend_pkt = NULL;
    int rank = 0;

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_comm->ug_comm, &rank));

    mpi_errno = CSPU_offload_new_cell(&cell);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    pkt = &cell->pkt;
    isend_pkt = &pkt->isend;
    isend_pkt->rank = rank;

    CSPU_offload_init_pkt(pkt, CSP_OFFLOAD_ISEND);
    isend_pkt->g_bufaddr = g_bufaddr;
    isend_pkt->count = count;
    isend_pkt->g_peer_rank = ug_comm->g_ranks_bound[dest].g_rank;
    isend_pkt->peer_rank = dest;
    isend_pkt->g_ugcomm_handle = ug_comm->g_ugcomm_bound;
    isend_pkt->send_offset = ug_comm->g_ranks_bound[rank].u_offset;
    isend_pkt->recv_offset = ug_comm->g_ranks_bound[dest].u_offset;
    isend_pkt->tag = tag;

    /* Get datatype handle on the bound ghost process  */
    mpi_errno = CSPU_datatype_get_g_handle(datatype, CSPU_offload_get_ghost(),
                                           &isend_pkt->g_datatype);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    CSPU_offload_issue(cell);

    (*request) = pkt->req;

    CSP_DBG_PRINT("isend: offload [g_bufaddr=0x%lx, count=%d, datatype=0x%x/0x%x, "
                  "dest=%d/%d, tag=%d, comm=0x%x/0x%lx, soffset=%d, roffset=%d], "
                  "req 0x%x, cell %p(%s)\n", g_bufaddr, count, datatype, isend_pkt->g_datatype,
                  dest, isend_pkt->g_peer_rank, tag, comm, isend_pkt->g_ugcomm_handle,
                  isend_pkt->send_offset, isend_pkt->recv_offset, (*request), cell,
                  (cell->type == CSP_OFFLOAD_CELL_SHM ? "shm" : "pending"));

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

#define ORIG_MPI_FNC() do {                                                     \
    mpi_errno = PMPI_Isend(buf, count, datatype, dest, tag, comm, request);     \
    CSP_DBG_PRINT("isend: [buf=%p, count=%d, datatype=0x%x, dest=%d, tag=%d, comm=0x%x]\n", \
                  buf, count, datatype, dest, tag, comm);                       \
} while (0)

int MPI_Isend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
              MPI_Comm comm, MPI_Request * request)
{
    int mpi_errno = MPI_SUCCESS;
    CSPU_comm_t *ug_comm = NULL;
    int buf_found_flag = 0;
    MPI_Aint g_bufaddr = -1;

    if (comm == MPI_COMM_WORLD)
        comm = CSP_COMM_USER_WORLD;

    /* Skip internal processing when disabled */
    if (CSP_IS_DISABLED || CSP_IS_MODE_DISABLED(PT2PT)) {
        ORIG_MPI_FNC();
        return mpi_errno;
    }

    CSPU_COMM_ERRHAN_SET_EXTOBJ();

    CSPU_fetch_ug_comm_from_cache(comm, &ug_comm);
    CSPU_shmbuf_translate_g_addr((void *) buf, &g_bufaddr, &buf_found_flag);
    CSP_DBG_PRINT("isend: comm 0x%x->ug_comm=%p, buf=%p, g_bufaddr=0x%lx, "
                  "buf_found_flag=%d\n", comm, ug_comm, buf, g_bufaddr, buf_found_flag);

    if (ug_comm && ug_comm->type >= CSP_COMM_ASYNC && buf_found_flag) {
        /* Asynchronous enabled comm and registered shared buffer. */
        mpi_errno = isend_impl(g_bufaddr, count, datatype, dest, tag, comm, request, ug_comm);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }
    else {
        /* normal comm. */
        CSPU_ERRHAN_RESET_EXTOBJ();     /* reset before calling original MPI */

        ORIG_MPI_FNC();
        return mpi_errno;
    }

  fn_exit:
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before return */
    return mpi_errno;

  fn_fail:
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before error handling */
    CSPU_COMM_ERRHANLDING(comm, &mpi_errno);
    goto fn_exit;
}

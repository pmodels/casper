/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

static inline int irecv_impl(MPI_Aint g_bufaddr, int count, MPI_Datatype datatype,
                             int src, int tag, MPI_Comm comm, MPI_Request * request,
                             CSPU_comm_t * ug_comm)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_offload_cell_t *cell = NULL;
    CSP_offload_pkt_t *pkt = NULL;
    CSP_offload_irecv_pkt_t *irecv_pkt = NULL;
    int rank = 0, ugrank = 0;

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_comm->comm, &rank));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_comm->ug_comm, &ugrank));

    mpi_errno = CSPU_offload_new_cell(&cell);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    pkt = &cell->pkt;
    irecv_pkt = &pkt->irecv;
    CSPU_offload_init_pkt(pkt, ug_comm, CSP_OFFLOAD_IRECV);

    irecv_pkt->rank = rank;
    irecv_pkt->ugrank = ugrank;
    irecv_pkt->peer_rank = src; /* peer_ugrank is unused. */
    irecv_pkt->g_bufaddr = g_bufaddr;
    irecv_pkt->count = count;
    irecv_pkt->g_ugcomm_handle = (MPI_Comm) ug_comm->g_ugcomm_bound;
    irecv_pkt->tag = tag;

    /* Get datatype handle on the bound ghost process  */
    mpi_errno = CSPU_datatype_get_g_handle(datatype, CSPU_offload_get_ghost(),
                                           &irecv_pkt->g_datatype);

    CSPU_offload_issue(cell);

    (*request) = pkt->req;

    CSP_DBG_PRINT("OFFLOAD irecv: offload [g_bufaddr=0x%lx, count=%d, "
                  "datatype=0x%x/0x%x, me=%d/%d, src=%d, tag=%d, comm=0x%x/0x%lx], "
                  "req 0x%x, cell %p(%s)\n", g_bufaddr, count, datatype, irecv_pkt->g_datatype,
                  rank, ugrank, src, tag, comm, irecv_pkt->g_ugcomm_handle, (*request),
                  cell, (cell->type == CSP_OFFLOAD_CELL_SHM ? "shm" : "pending"));

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

#define ORIG_MPI_FNC() do {                                                     \
    mpi_errno = PMPI_Irecv(buf, count, datatype, src, tag, comm, request);      \
    CSP_DBG_PRINT("irecv: [buf=%p, count=%d, datatype=0x%x, src=%d, tag=%d, comm=0x%x]\n",  \
                  buf, count, datatype, src, tag, comm);                        \
} while (0)

int MPI_Irecv(void *buf, int count, MPI_Datatype datatype, int src, int tag,
              MPI_Comm comm, MPI_Request * request)
{
    int mpi_errno = MPI_SUCCESS;
    CSPU_comm_t *ug_comm = NULL;
    int buf_found_flag = 0, offsz_flag = 0;
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
    CSPU_shmbuf_translate_g_addr(buf, &g_bufaddr, &buf_found_flag);
    CSPU_offload_checksz(count, datatype, ug_comm, &offsz_flag);

    CSP_DBG_PRINT("irecv: comm 0x%x->ug_comm=%p, buf=%p, count=%d, g_bufaddr=0x%lx, "
                  "buf_found_flag=%d, offsz_flag=%d\n", comm, ug_comm, buf, count,
                  g_bufaddr, buf_found_flag, offsz_flag);

    if (ug_comm && ug_comm->type >= CSP_COMM_ASYNC_DUP && buf_found_flag && offsz_flag) {
        /* Asynchronous enabled comm and registered shared buffer. */
        mpi_errno = irecv_impl(g_bufaddr, count, datatype, src, tag, comm, request, ug_comm);
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

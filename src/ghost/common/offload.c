/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cspg.h"

CSPG_offload_server_t CSPG_offload_server;

#define OFFLOAD_LRANK_TO_CH_IDX(local_rank) (local_rank - CSP_ENV.num_g)
#define OFFLOAD_CH_IDX_TO_LRANK(idx) (idx + CSP_ENV.num_g)

/* Statically bind local user processes.
 * See CSPU_offload_bind_ghost on user side.*/
static inline int offload_bind_users(int *user_local_rank_sta, int *user_local_rank_end)
{
    int mpi_errno = MPI_SUCCESS;
    int local_rank, local_size;
    int np_per_ghost = 0;
    int num_user = 0;

    CSP_CALLMPI(RETURN, PMPI_Comm_rank(CSP_PROC.local_comm, &local_rank));
    CSP_CALLMPI(RETURN, PMPI_Comm_size(CSP_PROC.local_comm, &local_size));

    num_user = local_size - CSP_ENV.num_g;

    if (CSP_ENV.num_g > num_user) {
        /* weird case: ghost is more than user... */
        if (local_rank < num_user) {
            (*user_local_rank_sta) = local_rank + CSP_ENV.num_g;
            (*user_local_rank_end) = local_rank + CSP_ENV.num_g;
        }
        else {
            (*user_local_rank_sta) = 0;
            (*user_local_rank_end) = 0;
        }
    }
    else {
        np_per_ghost = num_user / CSP_ENV.num_g;
        (*user_local_rank_sta) = CSP_ENV.num_g + np_per_ghost * local_rank;
        if (local_rank == CSP_ENV.num_g - 1) {
            (*user_local_rank_end) = local_size - 1;
        }
        else {
            (*user_local_rank_end) = (*user_local_rank_sta) + np_per_ghost - 1;
        }
    }


    return mpi_errno;
}

static inline int offload_set_tag_ub(void)
{
    int mpi_errno = MPI_SUCCESS;

    CSP_CALLMPI(RETURN, PMPI_Bcast(&CSPG_offload_server.tag_trans,
                                   sizeof(CSP_offload_tag_trans_t), MPI_BYTE,
                                   CSP_ENV.num_g /* first local user rank */ ,
                                   CSP_PROC.local_comm));

    CSPG_DBG_PRINT("OFFLOAD: tag: mpi_tag_ub = %d, trans_tag_nbits = %d, "
                   "user_tag_ub = %d, user_tag_nbits=%d, trans_mask=0x%x, user_tag_mask=0x%x\n",
                   CSPG_offload_server.tag_trans.mpi_tag_ub,
                   CSPG_offload_server.tag_trans.trans_tag_nbits,
                   CSPG_offload_server.tag_trans.user_tag_ub,
                   CSPG_offload_server.tag_trans.user_tag_nbits,
                   CSPG_offload_server.tag_trans.trans_tag_mask,
                   CSPG_offload_server.tag_trans.user_tag_mask);
    return mpi_errno;
}

/* Register handler functions for offloaded communication packet. */
static inline void register_offload_handlers(void)
{
    CSPG_offload_server.pkt_handlers[CSP_OFFLOAD_ISEND] = CSPG_isend_offload_handler;
    CSPG_offload_server.pkt_handlers[CSP_OFFLOAD_IRECV] = CSPG_irecv_offload_handler;

    CSPG_offload_server.cmpl_handlers[CSP_OFFLOAD_ISEND] = CSPG_isend_cmpl_handler;
    CSPG_offload_server.cmpl_handlers[CSP_OFFLOAD_IRECV] = CSPG_irecv_cmpl_handler;
}

static inline void initialize_issued_list(void)
{
    CSPG_offload_server.issued_list.head = NULL;
    CSPG_offload_server.issued_list.nissued = 0;
    CSPG_offload_server.issued_list.noutstanding = 0;
}

static inline int initialize_channels(void)
{
    int mpi_errno = MPI_SUCCESS;
    void *baseptr = NULL;
    int local_size;
    int dst = 0;

    CSP_CALLMPI(JUMP, PMPI_Comm_size(CSP_PROC.local_comm, &local_size));
    CSPG_offload_server.channels =
        CSP_calloc(local_size - CSP_ENV.num_g, sizeof(CSPG_offload_channel_t));

    /* Create shared memory region for pt2pt/collectives offload channel */
    /* [shm_recvq + 64 cells] per user process */
    CSP_CALLMPI(JUMP, PMPI_Win_allocate_shared(0, sizeof(char), MPI_INFO_NULL,
                                               CSP_PROC.local_comm, &baseptr,
                                               &CSPG_offload_server.shm_win));
    /* Setup shared channel for each local user */
    for (dst = 0; dst < local_size; dst++) {
        int r_disp_unit;
        MPI_Aint r_size;
        int idx = 0;

        /* skip ghost processes */
        if (dst < CSP_ENV.num_g)
            continue;

        idx = OFFLOAD_LRANK_TO_CH_IDX(dst);
        CSP_CALLMPI(JUMP, PMPI_Win_shared_query(CSPG_offload_server.shm_win, dst, &r_size,
                                                &r_disp_unit,
                                                &CSPG_offload_server.channels[idx].shm_base));
        CSPG_offload_server.channels[idx].shm_recvq_ptr =
            (CSP_offload_shmqueue_t *) (CSPG_offload_server.channels[idx].shm_base);

        CSPG_DBG_PRINT("OFFLOAD: channels[%d] local_rank=%d, shm_base=0x%lx, shm_recvq_ptr=%p\n",
                       idx, dst, CSPG_offload_server.channels[idx].shm_base,
                       CSPG_offload_server.channels[idx].shm_recvq_ptr);
    }

    /* Ensure no one access the queue before each user initializes. */
    CSP_CALLMPI(JUMP, PMPI_Barrier(CSP_PROC.local_comm));

  fn_exit:
    return mpi_errno;
  fn_fail:
    /* Free global objects in main function. */
    goto fn_exit;
}

static inline void offload_reset_stat(MPI_Status * stat)
{
    /* MPI may not update status if it is not a wildcard receive message.
     * We never call multiple completion (e.g., Testall) on ghost, so
     * ERROR will never be updated in that case. */
    memset(stat, 0, sizeof(MPI_Status));
    stat->MPI_ERROR = MPI_SUCCESS;
}

static inline int offload_poll_completion(void)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_offload_cell_t *cell = NULL, *tmp = NULL;

    DL_FOREACH_SAFE2(CSPG_offload_server.issued_list.head, cell, tmp, CSP_OFFLOAD_ABS_PT_DECL(next)) {
        int flag;
        MPI_Status stat;
        CSP_offload_pkt_t *pkt_ptr = &cell->pkt;

        offload_reset_stat(&stat);
        CSP_CALLMPI(JUMP, PMPI_Test(&pkt_ptr->g_req, &flag, &stat));
        if (flag) {
            /* Remove from local issued list. */
            CSPG_offload_issued_list_remove(&cell);

            /* Set completion on user.
             * The cell will be recycled by user. */
            CSP_DBG_ASSERT(cell->pkt.type < CSP_OFFLOAD_MAX &&
                           CSPG_offload_server.cmpl_handlers[cell->pkt.type]);
            CSPG_offload_server.cmpl_handlers[cell->pkt.type] (pkt_ptr, stat);
        }
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    /* Free global objects in main function. */
    goto fn_exit;
}

static inline int offload_poll_channel(CSPG_offload_channel_t * channel)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_offload_shmqueue_t *recvq_ptr = channel->shm_recvq_ptr;
    MPI_Aint shm_base = channel->shm_base;

    CSP_DBG_ASSERT(recvq_ptr != NULL);

    /* Polls all received cells at a time. */
    while (!CSP_offload_recvq_consumer_empty(shm_base, recvq_ptr)) {
        CSP_offload_cell_t *cell = NULL;
        CSP_offload_pkt_t *pkt_ptr = NULL;

        CSP_offload_recvq_dequeue(shm_base, recvq_ptr, &cell);
        pkt_ptr = &cell->pkt;

        /* Handles packet */
        CSP_DBG_ASSERT(cell->pkt.type < CSP_OFFLOAD_MAX &&
                       CSPG_offload_server.pkt_handlers[cell->pkt.type]);

        mpi_errno = CSPG_offload_server.pkt_handlers[cell->pkt.type] (pkt_ptr);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);

        /* Append into local polling list. */
        CSPG_offload_issued_list_reset_cellpt(cell);
        CSPG_offload_issued_list_append(cell);
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    /* Free global objects in main function. */
    goto fn_exit;
}

int CSPG_offload_poll_progress(void)
{
    int mpi_errno = MPI_SUCCESS;
    int idx, idx_sta, idx_end;

    idx_sta = OFFLOAD_LRANK_TO_CH_IDX(CSPG_offload_server.urange.lrank_sta);
    idx_end = OFFLOAD_LRANK_TO_CH_IDX(CSPG_offload_server.urange.lrank_end);

    /* do not bound to any user */
    if (idx_sta < 0 || idx_end < 0)
        goto fn_exit;

    mpi_errno = offload_poll_completion();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* Check each receive queue on bound user processes. */
    for (idx = idx_sta; idx <= idx_end; idx++) {
        mpi_errno = offload_poll_channel(&CSPG_offload_server.channels[idx]);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

    mpi_errno = offload_poll_completion();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

int CSPG_offload_destroy(void)
{
    int mpi_errno = MPI_SUCCESS;
    if (CSPG_offload_server.channels != NULL)
        free(CSPG_offload_server.channels);
    CSPG_offload_server.channels = NULL;

    /* Terminate ensures all local users have been finalizing.
     * It is safe to free shared memory region now. */

    CSP_ASSERT(CSPG_offload_server.issued_list.noutstanding == 0);

    CSPG_DBG_PRINT("OFFLOAD destroy: issued %d\n", CSPG_offload_server.issued_list.nissued);
    CSPG_DBG_PRINT("OFFLOAD destroy: free shm_win 0x%x\n", CSPG_offload_server.shm_win);

    CSP_CALLMPI(JUMP, PMPI_Win_free(&CSPG_offload_server.shm_win));
    CSPG_offload_server.shm_win = MPI_WIN_NULL;

  fn_exit:
    return mpi_errno;
  fn_fail:
    /* Free global objects in main function. */
    goto fn_exit;
}

int CSPG_offload_init(void)
{
    int mpi_errno = MPI_SUCCESS;

    memset(&CSPG_offload_server, 0, sizeof(CSPG_offload_server_t));

    mpi_errno = initialize_channels();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    initialize_issued_list();

    mpi_errno = offload_bind_users(&CSPG_offload_server.urange.lrank_sta,
                                   &CSPG_offload_server.urange.lrank_end);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    CSPG_DBG_PRINT("OFFLOAD: bound urange %d-%d\n", CSPG_offload_server.urange.lrank_sta,
                   CSPG_offload_server.urange.lrank_end);

    mpi_errno = offload_set_tag_ub();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    register_offload_handlers();

  fn_exit:
    return mpi_errno;
  fn_fail:
    /* Free global objects in main function. */
    goto fn_exit;
}

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cspg.h"

static inline int ugcomm_release(CSPG_comm_t * cspg_comm)
{
    int i;
    int mpi_errno = MPI_SUCCESS;

    if (cspg_comm) {
        if (cspg_comm->ug_comm && cspg_comm->ug_comm != MPI_COMM_NULL) {
            CSPG_DBG_PRINT("COMM: free cspg_comm->ug_comm 0x%x\n", cspg_comm->ug_comm);
            CSP_CALLMPI(RETURN, PMPI_Comm_free(&cspg_comm->ug_comm));
        }

        if (cspg_comm->type == CSP_COMM_ASYNC && cspg_comm->dup_ug_comms) {
            for (i = 1; i < cspg_comm->num_ug_comms; i++) {
                if (cspg_comm->dup_ug_comms[i] && cspg_comm->dup_ug_comms[i] != MPI_COMM_NULL) {
                    CSPG_DBG_PRINT("COMM: free cspg_comm->ug_comms[%d] 0x%x\n", i,
                                   cspg_comm->dup_ug_comms[i]);
                    CSP_CALLMPI(RETURN, PMPI_Comm_free(&cspg_comm->dup_ug_comms[i]));
                }
            }

            free(cspg_comm->dup_ug_comms);
        }

        if (cspg_comm->g_ranks_bound)
            free(cspg_comm->g_ranks_bound);

        free(cspg_comm);
    }
    return mpi_errno;
}

static int ugcomm_create_impl(CSP_cwp_fnc_ugcomm_create_pkt_t * ugcomm_create_pkt)
{
    int mpi_errno = MPI_SUCCESS;
    int ug_comm_nproc = 0, user_local_root = 0;
    int *ug_ranks = NULL;
    MPI_Group ug_group = MPI_GROUP_NULL;
    MPI_Aint *tmp_g_ugcomm_handles = NULL;
    int local_nprocs = 0, local_rank = 0, ug_rank = 0;
    CSPG_comm_t *cspg_comm = NULL;
    int i;

    CSP_CALLMPI(JUMP, PMPI_Comm_size(CSP_PROC.local_comm, &local_nprocs));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_PROC.local_comm, &local_rank));

    ug_comm_nproc = ugcomm_create_pkt->ug_comm_nproc;

    cspg_comm = CSP_calloc(1, sizeof(CSPG_comm_t));
    ug_ranks = CSP_calloc(ug_comm_nproc, sizeof(int));
    CSP_ASSERT(cspg_comm != NULL && ug_ranks != NULL);

    user_local_root = ugcomm_create_pkt->user_local_root;
    cspg_comm->type = ugcomm_create_pkt->type;
    cspg_comm->num_ug_comms = ugcomm_create_pkt->num_ug_comms;
    cspg_comm->wildcard_info = ugcomm_create_pkt->wildcard_info;
    cspg_comm->user_nproc = ugcomm_create_pkt->user_nproc;

    mpi_errno = CSPG_cwp_recv_param(ug_ranks, ug_comm_nproc * sizeof(int), user_local_root);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* Create the new ug_comm.
     * We always create from COMM_WORLD because the parent communicator may not be
     * extended to ug_comm depending on user hint. */
    CSP_CALLMPI(JUMP, PMPI_Group_incl(CSP_PROC.wgroup, ug_comm_nproc, ug_ranks, &ug_group));
    CSP_CALLMPI(JUMP, PMPI_Comm_create_group(CSP_PROC.wcomm, ug_group, 0, &cspg_comm->ug_comm));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(cspg_comm->ug_comm, &ug_rank));
    CSPG_DBG_PRINT("COMM: created cspg_comm=%p, ug_rank=%d/%d, num_ug_comms=%d, "
                   "ug_comm=0x%x (type: %s)\n", cspg_comm, ug_rank, ug_comm_nproc,
                   cspg_comm->num_ug_comms, cspg_comm->ug_comm,
                   CSP_ug_comm_type_name[cspg_comm->type]);

    if (cspg_comm->type == CSP_COMM_ASYNC) {
        CSP_ASSERT(cspg_comm->num_ug_comms >= 1);
        cspg_comm->dup_ug_comms = CSP_calloc(cspg_comm->num_ug_comms, sizeof(MPI_Comm));
        CSP_ASSERT(cspg_comm->dup_ug_comms != NULL);

        /* Reuse the first ug_comm, and duplicate others when async is enabled.
         * Note that it is not needed for CSP_COMM_ASYNC_NODUP type.*/
        cspg_comm->dup_ug_comms[0] = cspg_comm->ug_comm;
        for (i = 1; i < cspg_comm->num_ug_comms; i++) {
            CSP_CALLMPI(JUMP, PMPI_Comm_dup(cspg_comm->ug_comm, &cspg_comm->dup_ug_comms[i]));
            CSPG_DBG_PRINT("COMM: dup ug_comm[%d]=0x%x\n", i, cspg_comm->dup_ug_comms[i]);
        }
    }

    /* Receive bound ghost rank of every user ranks. */
    cspg_comm->g_ranks_bound = CSP_calloc(cspg_comm->user_nproc, sizeof(int));
    mpi_errno = CSPG_cwp_recv_param(cspg_comm->g_ranks_bound, cspg_comm->user_nproc * sizeof(int),
                                    user_local_root);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* Scatter my cspg_comm address to users.
     * Thus the cspg_comm can be found at offload call or comm free.*/
    tmp_g_ugcomm_handles = CSP_calloc(local_nprocs, sizeof(MPI_Aint));
    tmp_g_ugcomm_handles[local_rank] = (MPI_Aint) cspg_comm;
    CSP_CALLMPI(JUMP, PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, tmp_g_ugcomm_handles,
                                     1, MPI_AINT, CSP_PROC.local_comm));
    CSPG_DBG_PRINT("COMM: scatter handle cspg_comm=%p\n", cspg_comm);

  fn_exit:
    if (ug_group && ug_group != MPI_GROUP_NULL)
        CSP_CALLMPI_EXIT(PMPI_Group_free(&ug_group));
    if (ug_ranks)
        free(ug_ranks);
    if (tmp_g_ugcomm_handles)
        free(tmp_g_ugcomm_handles);
    return mpi_errno;
  fn_fail:
    mpi_errno = ugcomm_release(cspg_comm);
    goto fn_exit;
}

static int ugcomm_free_impl(CSP_cwp_fnc_ugcomm_free_pkt_t * ugcomm_free_pkt)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint cspg_comm_handle = 0;
    CSPG_comm_t *cspg_comm = NULL;

    /* Receive the handle of my ug_comm from user root */
    CSP_CALLMPI(NOSTMT, PMPI_Recv(&cspg_comm_handle, 1, MPI_AINT, ugcomm_free_pkt->user_local_root,
                                  CSP_CWP_PARAM_TAG, CSP_PROC.local_comm, MPI_STATUS_IGNORE));

    cspg_comm = (CSPG_comm_t *) cspg_comm_handle;
    CSPG_DBG_PRINT("COMM: free cspg_comm %p\n", cspg_comm);

    mpi_errno = ugcomm_release(cspg_comm);
    return mpi_errno;
}

int CSPG_ugcomm_free_cwp_root_handler(CSP_cwp_pkt_t * pkt,
                                      int user_local_rank CSP_ATTRIBUTE((unused)))
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Request ibcast_req = MPI_REQUEST_NULL;
    CSP_cwp_fnc_ugcomm_free_pkt_t *ugcomm_free_pkt = &pkt->u.fnc_ugcomm_free;

    /* broadcast to other local ghosts */
    mpi_errno = CSPG_cwp_try_bcast(pkt, &ibcast_req);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    CSP_CALLMPI(JUMP, PMPI_Wait(&ibcast_req, MPI_STATUS_IGNORE));

    mpi_errno = ugcomm_free_impl(ugcomm_free_pkt);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    /* Release local lock after a locked command finished. */
    mpi_errno = CSPG_mlock_release();
    return mpi_errno;

  fn_fail:
    /* Error is handled in CSPG_main. */
    goto fn_exit;
}

int CSPG_ugcomm_free_cwp_handler(CSP_cwp_pkt_t * pkt)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_cwp_fnc_ugcomm_free_pkt_t *ugcomm_free_pkt = &pkt->u.fnc_ugcomm_free;

    mpi_errno = ugcomm_free_impl(ugcomm_free_pkt);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    return mpi_errno;

  fn_fail:
    /* Error is handled in CSPG_main. */
    goto fn_exit;
}

int CSPG_ugcomm_create_cwp_root_handler(CSP_cwp_pkt_t * pkt,
                                        int user_local_rank CSP_ATTRIBUTE((unused)))
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Request ibcast_req = MPI_REQUEST_NULL;
    CSP_cwp_fnc_ugcomm_create_pkt_t *ugcomm_create_pkt = &pkt->u.fnc_ugcomm_create;

    /* broadcast to other local ghosts */
    mpi_errno = CSPG_cwp_try_bcast(pkt, &ibcast_req);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    CSP_CALLMPI(JUMP, PMPI_Wait(&ibcast_req, MPI_STATUS_IGNORE));

    mpi_errno = ugcomm_create_impl(ugcomm_create_pkt);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    /* Release local lock after a locked command finished. */
    mpi_errno = CSPG_mlock_release();
    return mpi_errno;

  fn_fail:
    /* Error is handled in CSPG_main. */
    goto fn_exit;
}

int CSPG_ugcomm_create_cwp_handler(CSP_cwp_pkt_t * pkt)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_cwp_fnc_ugcomm_create_pkt_t *ugcomm_create_pkt = &pkt->u.fnc_ugcomm_create;

    mpi_errno = ugcomm_create_impl(ugcomm_create_pkt);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    return mpi_errno;

  fn_fail:
    /* Error is handled in CSPG_main. */
    goto fn_exit;
}

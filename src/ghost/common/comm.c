/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cspg.h"


static int ugcomm_create_impl(CSP_cwp_fnc_ugcomm_create_pkt_t * ugcomm_create_pkt)
{
    int mpi_errno = MPI_SUCCESS;
    int num_ghosts_unique = 0, user_nprocs = 0, user_local_root = 0;
    int *ug_ranks = NULL;
    MPI_Group ug_group = MPI_GROUP_NULL;
    MPI_Comm ug_comm = MPI_COMM_NULL;
    MPI_Aint *tmp_g_ugcomm_handles = NULL;
    int local_nprocs = 0, local_rank = 0, ug_rank = 0;

    num_ghosts_unique = ugcomm_create_pkt->num_ghosts_unique;
    user_nprocs = ugcomm_create_pkt->user_nprocs;
    user_local_root = ugcomm_create_pkt->user_local_root;

    CSP_CALLMPI(JUMP, PMPI_Comm_size(CSP_PROC.local_comm, &local_nprocs));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_PROC.local_comm, &local_rank));

    ug_ranks = CSP_calloc(user_nprocs + num_ghosts_unique, sizeof(int));
    CSP_ASSERT(ug_ranks != NULL);

    mpi_errno = CSPG_cwp_recv_param(ug_ranks, (user_nprocs + num_ghosts_unique) * sizeof(int),
                                    user_local_root);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* Create the new ug_comm.
     * We always create from COMM_WORLD because the parent communicator may not be
     * extended to ug_comm depending on user hint. */
    CSP_CALLMPI(JUMP, PMPI_Group_incl(CSP_PROC.wgroup, user_nprocs + num_ghosts_unique,
                                      ug_ranks, &ug_group));
    CSP_CALLMPI(JUMP, PMPI_Comm_create_group(MPI_COMM_WORLD, ug_group, 0, &ug_comm));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_comm, &ug_rank));
    CSPG_DBG_PRINT("COMM: created ug_comm=0x%x, ug_rank=%d\n", ug_comm, ug_rank);

    /* Scatter my ug_comm address to users.
     * Thus the ug_comm can be found at offload call or comm free.*/
    tmp_g_ugcomm_handles = CSP_calloc(local_nprocs, sizeof(MPI_Aint));
    tmp_g_ugcomm_handles[local_rank] = (MPI_Aint) ug_comm;
    CSP_CALLMPI(JUMP, PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, tmp_g_ugcomm_handles,
                                     1, MPI_AINT, CSP_PROC.local_comm));
    CSPG_DBG_PRINT("COMM: scatter handle ug_comm=0x%x\n", ug_comm);

  fn_exit:
    if (ug_group && ug_group != MPI_GROUP_NULL)
        CSP_CALLMPI_EXIT(PMPI_Group_free(&ug_group));
    if (ug_ranks)
        free(ug_ranks);
    if (tmp_g_ugcomm_handles)
        free(tmp_g_ugcomm_handles);
    return mpi_errno;
  fn_fail:
    if (ug_comm)
        CSP_CALLMPI_EXIT(PMPI_Comm_free(&ug_comm));
    goto fn_exit;
}

static int ugcomm_free_impl(CSP_cwp_fnc_ugcomm_free_pkt_t * ugcomm_free_pkt)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint ugcomm_handle = 0;
    MPI_Comm ug_comm = MPI_COMM_NULL;

    /* Receive the handle of my ug_comm from user root */
    CSP_CALLMPI(NOSTMT, PMPI_Recv(&ugcomm_handle, 1, MPI_AINT, ugcomm_free_pkt->user_local_root,
                                  CSP_CWP_PARAM_TAG, CSP_PROC.local_comm, MPI_STATUS_IGNORE));

    ug_comm = (MPI_Comm) ugcomm_handle;
    CSPG_DBG_PRINT("COMM: free ug_comm 0x%x\n", ug_comm);

    CSP_CALLMPI(RETURN, PMPI_Comm_free(&ug_comm));
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

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspg.h"

static int shmbuf_regist_impl(CSP_cwp_shmbuf_regist_pkt_t * shmbuf_regist_pkt)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint ugcomm_handle = 0;
    MPI_Comm ug_comm = MPI_COMM_NULL;
    void *base = NULL;
    MPI_Win shmbuf_win = MPI_WIN_NULL;
    int lrank = 0, ug_nproc = 0, i = 0;
    MPI_Aint *user_bases = NULL;
    MPI_Request *reqs = NULL;
    int nreqs = 0;
    MPI_Group local_group = MPI_GROUP_NULL, ug_group = MPI_GROUP_NULL;
    int *bound_lranks = NULL, *bound_ug_uranks = NULL, nbound = 0, bound_idx = 0;

    /* Receive my ug_comm handle */
    mpi_errno = CSPG_cwp_recv_param(&ugcomm_handle, sizeof(MPI_Aint),
                                    shmbuf_regist_pkt->user_local_root);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    ug_comm = (MPI_Comm) ugcomm_handle;
    CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_comm, &ug_nproc));

    /* Translate all bound ranks in the ug_comm. */
    CSP_CALLMPI(JUMP, PMPI_Comm_group(CSP_PROC.local_comm, &local_group));
    CSP_CALLMPI(JUMP, PMPI_Comm_group(ug_comm, &ug_group));

    nbound = CSPG_offload_server.urange.lrank_end - CSPG_offload_server.urange.lrank_sta + 1;
    bound_lranks = CSP_calloc(nbound, sizeof(int));
    bound_ug_uranks = CSP_calloc(nbound, sizeof(int));

    for (lrank = CSPG_offload_server.urange.lrank_sta;
         lrank <= CSPG_offload_server.urange.lrank_end; lrank++)
        bound_lranks[bound_idx++] = lrank;

    CSP_CALLMPI(JUMP, PMPI_Group_translate_ranks(local_group, nbound,
                                                 bound_lranks, ug_group, bound_ug_uranks));

    /* Create shared buffer window. */
    CSP_CALLMPI(JUMP, PMPI_Win_allocate_shared(0, 1, MPI_INFO_NULL, ug_comm, &base, &shmbuf_win));
    CSPG_DBG_PRINT("SHMBUF: regist ug_comm=0x%x, base=%p, shmbuf_win=0x%x\n",
                   ug_comm, base, shmbuf_win);

    /* Query address for my bound users and send to each user.
     * user_bases is accessed by local ranks.*/
    user_bases = CSP_calloc(ug_nproc, sizeof(MPI_Aint));
    reqs = CSP_calloc(ug_nproc + 1, sizeof(MPI_Request));
    for (i = 0; i < nbound; i++) {
        MPI_Aint r_size = 0;
        int r_disp_unit = 0;
        CSP_CALLMPI(JUMP, PMPI_Win_shared_query(shmbuf_win, bound_ug_uranks[i], &r_size,
                                                &r_disp_unit, (void *) &user_bases[i]));

        mpi_errno = CSPG_cwp_try_send_param(&user_bases[i],
                                            sizeof(MPI_Aint), bound_lranks[i], &reqs[nreqs++]);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
        CSPG_DBG_PRINT("SHMBUF: regist urank %d, user_base=0x%lx\n", bound_ug_uranks[i],
                       user_bases[i]);
    }

    /* Notify user root the handle of ghost win. */
    MPI_Aint shmbuf_win_handle = (MPI_Aint) shmbuf_win;
    mpi_errno = CSPG_cwp_try_send_param(&shmbuf_win_handle,
                                        sizeof(MPI_Aint), shmbuf_regist_pkt->user_local_root,
                                        &reqs[nreqs++]);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);
    CSPG_DBG_PRINT("SHMBUF: regist sent user root %d my shmbuf_win handle=0x%lx\n",
                   shmbuf_regist_pkt->user_local_root, shmbuf_win_handle);

    CSP_CALLMPI(JUMP, PMPI_Waitall(nreqs, reqs, MPI_STATUS_IGNORE));

    if (local_group != MPI_GROUP_NULL)
        CSP_CALLMPI(JUMP, PMPI_Group_free(&local_group));
    if (ug_group != MPI_GROUP_NULL)
        CSP_CALLMPI(JUMP, PMPI_Group_free(&ug_group));

  fn_exit:
    if (user_bases)
        free(user_bases);
    if (reqs)
        free(reqs);
    if (bound_lranks)
        free(bound_lranks);
    if (bound_ug_uranks)
        free(bound_ug_uranks);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int shmbuf_free_impl(CSP_cwp_shmbuf_free_pkt_t * shmbuf_free_pkt)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint shmbufwin_handle = 0;
    MPI_Win shmbuf_win = MPI_COMM_NULL;

    /* Receive the handle of my ug_comm from user root */
    CSP_CALLMPI(NOSTMT, PMPI_Recv(&shmbufwin_handle, 1, MPI_AINT, shmbuf_free_pkt->user_local_root,
                                  CSP_CWP_PARAM_TAG, CSP_PROC.local_comm, MPI_STATUS_IGNORE));

    shmbuf_win = (MPI_Win) shmbufwin_handle;
    CSPG_DBG_PRINT("SHMBUF: free shmbuf_win 0x%x\n", shmbuf_win);

    CSP_CALLMPI(RETURN, PMPI_Win_free(&shmbuf_win));
    return mpi_errno;
}

int CSPG_shmbuf_free_cwp_root_handler(CSP_cwp_pkt_t * pkt,
                                      int user_local_rank CSP_ATTRIBUTE((unused)))
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Request ibcast_req = MPI_REQUEST_NULL;
    CSP_cwp_shmbuf_free_pkt_t *shmbuf_free_pkt = &pkt->u.fnc_shmbuf_free;

    /* broadcast to other local ghosts */
    mpi_errno = CSPG_cwp_try_bcast(pkt, &ibcast_req);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    CSP_CALLMPI(JUMP, PMPI_Wait(&ibcast_req, MPI_STATUS_IGNORE));

    mpi_errno = shmbuf_free_impl(shmbuf_free_pkt);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    return mpi_errno;

  fn_fail:
    /* Error is handled in CSPG_main. */
    goto fn_exit;
}


int CSPG_shmbuf_free_cwp_handler(CSP_cwp_pkt_t * pkt)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_cwp_shmbuf_free_pkt_t *shmbuf_free_pkt = &pkt->u.fnc_shmbuf_free;

    mpi_errno = shmbuf_free_impl(shmbuf_free_pkt);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    return mpi_errno;

  fn_fail:
    /* Error is handled in CSPG_main. */
    goto fn_exit;
}

int CSPG_shmbuf_regist_cwp_root_handler(CSP_cwp_pkt_t * pkt,
                                        int user_local_rank CSP_ATTRIBUTE((unused)))
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Request ibcast_req = MPI_REQUEST_NULL;
    CSP_cwp_shmbuf_regist_pkt_t *shmbuf_regist_pkt = &pkt->u.fnc_shmbuf_regist;

    /* broadcast to other local ghosts */
    mpi_errno = CSPG_cwp_try_bcast(pkt, &ibcast_req);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    CSP_CALLMPI(JUMP, PMPI_Wait(&ibcast_req, MPI_STATUS_IGNORE));

    mpi_errno = shmbuf_regist_impl(shmbuf_regist_pkt);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    return mpi_errno;

  fn_fail:
    /* Error is handled in CSPG_main. */
    goto fn_exit;
}

int CSPG_shmbuf_regist_cwp_handler(CSP_cwp_pkt_t * pkt)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_cwp_shmbuf_regist_pkt_t *shmbuf_regist_pkt = &pkt->u.fnc_shmbuf_regist;

    mpi_errno = shmbuf_regist_impl(shmbuf_regist_pkt);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    return mpi_errno;

  fn_fail:
    /* Error is handled in CSPG_main. */
    goto fn_exit;
}

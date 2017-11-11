/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */


#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

const char *CSP_ug_comm_type_name[CSP_COMM_TYPE_MAX] =
    { "refer", "shmbuf", "async_dup", "async_tag" };

static int ugcomm_gather_ranks(CSPU_comm_t * ug_newcomm, int *ug_nproc, int *ug_ranks,
                               int *num_max_g_users)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Group lugroup = MPI_GROUP_NULL;
    int urrank = 0, urnproc = 0, lnproc = 0;
    int max_g_users = 0, gmax_g_users = 1;
    int lurank = 0, lunproc = 0;
    int *tmp_ug_wranks = NULL, *tmp_lurank_buf = NULL;
    int i, j;

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_newcomm->local_user_comm, &lurank));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_newcomm->local_user_comm, &lunproc));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(CSP_PROC.local_comm, &lnproc));

    if (lurank == 0) {
        CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_newcomm->user_root_comm, &urnproc));
        CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_newcomm->user_root_comm, &urrank));
    }
    CSP_CALLMPI(JUMP, PMPI_Bcast(&urnproc, 1, MPI_INT, 0, ug_newcomm->local_user_comm));

    /* Every user root computes [number of user+ghost, g0, g0's bound users, g1,...].
     * All ghosts are included (even no bound user in this comm) for simpler coding... */
    tmp_ug_wranks = CSP_calloc((lnproc + 1) * urnproc, sizeof(int));

    if (lurank == 0) {
        int local_ug_nproc = 0;
        int *user_luranks_ptr = NULL, *tmp_ug_lranks_ptr = NULL, *user_lranks_ptr = NULL;
        int *tmp_ug_wranks_ptr = &tmp_ug_wranks[(lnproc + 1) * urrank];

        /* Each temp buffer at most lnproc size. */
        tmp_lurank_buf = CSP_calloc(lnproc * 3, sizeof(int));
        user_luranks_ptr = tmp_lurank_buf;
        tmp_ug_lranks_ptr = &tmp_lurank_buf[lnproc];
        user_lranks_ptr = &tmp_lurank_buf[lnproc * 2];

        CSP_CALLMPI(JUMP, PMPI_Comm_group(ug_newcomm->local_user_comm, &lugroup));

        for (i = 0; i < lunproc; i++)
            user_luranks_ptr[i] = i;

        /* Get all local user rank in global local comm, local use only. */
        CSP_CALLMPI(JUMP, PMPI_Group_translate_ranks(lugroup, lunproc,
                                                     user_luranks_ptr, CSP_PROC.lgroup,
                                                     user_lranks_ptr));
        for (i = 0; i < CSP_ENV.num_g; i++) {
            int g_users = 0;

            tmp_ug_lranks_ptr[local_ug_nproc++] = i;
            /* Group user ranks by bound ghost. */
            for (j = 0; j < lunproc; j++) {
                int lrank = user_lranks_ptr[j];
                if (CSPU_offload_ch.bound_g_lranks_local[lrank - CSP_ENV.num_g] == i) {
                    tmp_ug_lranks_ptr[local_ug_nproc++] = lrank;
                    g_users++;
                }
            }
            if (max_g_users < g_users)
                max_g_users = g_users;
        }

        /* Translate to world ranks and exchange with other usre root. */
        CSP_CALLMPI(JUMP, PMPI_Group_translate_ranks(CSP_PROC.lgroup, local_ug_nproc,
                                                     tmp_ug_lranks_ptr, CSP_PROC.wgroup,
                                                     &tmp_ug_wranks_ptr[1]));
        tmp_ug_wranks_ptr[0] = local_ug_nproc;
        CSP_CALLMPI(JUMP, PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, tmp_ug_wranks,
                                         lnproc + 1, MPI_INT, ug_newcomm->user_root_comm));

        if (ug_newcomm->type == CSP_COMM_ASYNC_DUP)
            CSP_CALLMPI(JUMP, PMPI_Allreduce(&max_g_users, &gmax_g_users, 1, MPI_INT,
                                             MPI_MAX, ug_newcomm->user_root_comm));

#ifdef CSP_DEBUG
        CSP_DBG_PRINT("COMM: urrank %d:\n", urrank);
        for (i = 0; i < local_ug_nproc; i++) {
            CSP_DBG_PRINT("         tmp_ug_lranks[%d/%d] %d, tmp_ug_wrank %d\n",
                          i, local_ug_nproc, tmp_ug_lranks_ptr[i], tmp_ug_wranks_ptr[i + 1]);
        }
#endif
    }

    /* Every user root broadcasts to its local users. */
    CSP_CALLMPI(JUMP, PMPI_Bcast(tmp_ug_wranks, (lnproc + 1) * urnproc, MPI_INT, 0,
                                 ug_newcomm->local_user_comm));
    if (ug_newcomm->type == CSP_COMM_ASYNC_DUP)
        CSP_CALLMPI(JUMP, PMPI_Bcast(&gmax_g_users, 1, MPI_INT, 0, ug_newcomm->local_user_comm));

    /* Copy noncontiguous tmp_ug_wranks to final output. */
    int doff = 0, soff = 0;
    for (i = 0; i < urnproc; i++) {
        int sz = tmp_ug_wranks[soff];
        memcpy(&ug_ranks[doff], &tmp_ug_wranks[soff + 1], sz * sizeof(int));
        doff += sz;
        soff += lnproc + 1;
        (*ug_nproc) += sz;
    }

    *num_max_g_users = gmax_g_users;

    if (lugroup != MPI_GROUP_NULL)
        CSP_CALLMPI(JUMP, PMPI_Group_free(&lugroup));

  fn_exit:
    if (tmp_ug_wranks)
        free(tmp_ug_wranks);
    if (tmp_lurank_buf)
        free(tmp_lurank_buf);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int ugcomm_issue_ghost_cmd(CSPU_comm_t * ug_comm)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_cwp_pkt_t pkt;
    CSP_cwp_fnc_ugcomm_free_pkt_t *ugcomm_free_pkt = &pkt.u.fnc_ugcomm_free;
    MPI_Request *reqs = NULL;
    int i, local_rank = 0;

    reqs = CSP_calloc(CSP_ENV.num_g, sizeof(MPI_Request));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_PROC.local_comm, &local_rank));

    /* Ensure all user roots have arrived before start lock. */
    CSP_CALLMPI(JUMP, PMPI_Barrier(ug_comm->user_root_comm));

    /* Lock ghost processes on all nodes. */
    mpi_errno = CSPU_mlock_acquire(ug_comm->user_root_comm);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* Send command to root ghost. */
    CSP_cwp_init_pkt(CSP_CWP_FNC_UGCOMM_FREE, &pkt);
    ugcomm_free_pkt->user_local_root = local_rank;

    mpi_errno = CSPU_cwp_issue(&pkt);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* Send the handle of ug_comm to each ghost. */
    for (i = 0; i < CSP_ENV.num_g; i++) {
        CSP_CALLMPI(JUMP, PMPI_Isend(&ug_comm->g_ugcomm_handles[i], 1, MPI_AINT,
                                     CSP_PROC.user.g_lranks[i], CSP_CWP_PARAM_TAG,
                                     CSP_PROC.local_comm, &reqs[i]));
    }

    CSP_CALLMPI(JUMP, PMPI_Waitall(CSP_ENV.num_g, reqs, MPI_STATUS_IGNORE));

  fn_exit:
    if (reqs)
        free(reqs);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int ugcomm_release(CSPU_comm_t * ug_comm)
{
    int mpi_errno = MPI_SUCCESS;
    int i;

    if (ug_comm->user_root_comm && ug_comm->user_root_comm != MPI_COMM_NULL &&
        ug_comm->user_root_comm != CSP_PROC.user.ur_comm) {
        CSP_CALLMPI(JUMP, PMPI_Comm_free(&ug_comm->user_root_comm));
    }
    if (ug_comm->local_user_comm && ug_comm->local_user_comm != MPI_COMM_NULL &&
        ug_comm->local_user_comm != CSP_PROC.user.u_local_comm) {
        CSP_CALLMPI(JUMP, PMPI_Comm_free(&ug_comm->local_user_comm));
    }
    if (ug_comm->ug_comm && ug_comm->ug_comm != MPI_COMM_NULL) {
        CSP_CALLMPI(JUMP, PMPI_Comm_free(&ug_comm->ug_comm));
    }

    if (ug_comm->group && ug_comm->group != MPI_GROUP_NULL)
        CSP_CALLMPI(JUMP, PMPI_Group_free(&ug_comm->group));
    if (ug_comm->ug_group && ug_comm->ug_group != MPI_GROUP_NULL)
        CSP_CALLMPI(JUMP, PMPI_Group_free(&ug_comm->ug_group));

    if (ug_comm->type == CSP_COMM_ASYNC_DUP && ug_comm->dup_ug_comms) {
        /* Skip the first, which reuses ug_comm. */
        for (i = 1; i < ug_comm->num_ug_comms; i++) {
            if (ug_comm->dup_ug_comms[i] && ug_comm->dup_ug_comms[i] != MPI_COMM_NULL) {
                CSP_CALLMPI(JUMP, PMPI_Comm_free(&ug_comm->dup_ug_comms[i]));
            }
        }
        free(ug_comm->dup_ug_comms);
    }

    if (ug_comm->g_ugcomm_handles)
        free(ug_comm->g_ugcomm_handles);

    mpi_errno = CSPU_remove_ug_comm_from_cache(ug_comm->comm);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    CSP_DBG_PRINT("COMM: free ug_comm %p\n", ug_comm);
    free(ug_comm);

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int ugcomm_exchange_granks_bound(CSPU_comm_t * ug_newcomm)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Group ug_newgroup = MPI_GROUP_NULL;
    int user_newrank, user_newnproc, ulrank = -1;
    int *g_ranks_bound = NULL;

    CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_newcomm->comm, &user_newnproc));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_newcomm->comm, &user_newrank));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_newcomm->local_user_comm, &ulrank));

    /* Exchange bound ghost rank in new ug_comm.
     * Ghost is statically bound at MPI init, thus each ghost only needs to
     * check fixed offload queues. */
    g_ranks_bound = CSP_calloc(user_newnproc, sizeof(int));
    CSP_ASSERT(g_ranks_bound != NULL);

    CSP_CALLMPI(JUMP, PMPI_Comm_group(ug_newcomm->ug_comm, &ug_newgroup));
    CSP_CALLMPI(JUMP, PMPI_Group_translate_ranks(CSP_PROC.lgroup, 1,
                                                 &CSPU_offload_ch.bound_g_lrank, ug_newgroup,
                                                 &g_ranks_bound[user_newrank]));
    CSP_CALLMPI(JUMP, PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, g_ranks_bound,
                                     1, MPI_INT, ug_newcomm->comm));

    CSP_DBG_PRINT("COMM: exchanged my g_ranks_bound[%d] g_rank %d\n", user_newrank,
                  g_ranks_bound[user_newrank]);

    if (ulrank == 0) {
        /* Then broadcast g_ranks_bound to all local ghosts. */
        mpi_errno = CSPU_cwp_bcast_params(g_ranks_bound, user_newnproc * sizeof(int));
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

    if (ug_newgroup && ug_newgroup != MPI_GROUP_NULL)
        CSP_CALLMPI(JUMP, PMPI_Group_free(&ug_newgroup));

  fn_exit:
    if (g_ranks_bound)
        free(g_ranks_bound);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int ugcomm_gather_handles(CSPU_comm_t * ug_newcomm)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint *tmp_g_ugcomm_handles = NULL;
    int local_nprocs, ulrank;

    CSP_CALLMPI(JUMP, PMPI_Comm_size(CSP_PROC.local_comm, &local_nprocs));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_newcomm->local_user_comm, &ulrank));

    /* Dummy input on user process. */
    tmp_g_ugcomm_handles = CSP_calloc(local_nprocs, sizeof(MPI_Aint));
    CSP_CALLMPI(JUMP, PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, tmp_g_ugcomm_handles,
                                     1, MPI_AINT, CSP_PROC.local_comm));
    if (ulrank == 0) {
        int i;
        /* Gather ug_comm handles from each ghost. Used at comm_free. */
        ug_newcomm->g_ugcomm_handles = CSP_calloc(CSP_ENV.num_g, sizeof(MPI_Aint));
        for (i = 0; i < CSP_ENV.num_g; i++) {
            ug_newcomm->g_ugcomm_handles[i] = tmp_g_ugcomm_handles[i];
            CSP_DBG_PRINT("COMM: gathered g_ugcomm_handles[%d] 0x%lx\n", i,
                          ug_newcomm->g_ugcomm_handles[i]);
        }
    }

    /* Get the ug_comm address on my bound ghost. Used at offloading. */
    ug_newcomm->g_ugcomm_bound = tmp_g_ugcomm_handles[CSPU_offload_ch.bound_g_lrank];

    CSP_DBG_PRINT("COMM: received my g_ugcomm_bound=0x%lx\n", ug_newcomm->g_ugcomm_bound);

  fn_exit:
    if (tmp_g_ugcomm_handles)
        free(tmp_g_ugcomm_handles);
    return mpi_errno;

  fn_fail:
    ugcomm_release(ug_newcomm);
    goto fn_exit;
}

static int ugcomm_create_comm(CSPU_comm_t * ug_newcomm)
{
    int mpi_errno = MPI_SUCCESS;
    int ulrank = 0, user_newnproc = 0;
    int *ug_ranks = NULL;
    MPI_Group ug_newgroup = MPI_GROUP_NULL;
    int num_max_g_users = 0;
    int i;

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_newcomm->local_user_comm, &ulrank));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_newcomm->comm, &user_newnproc));

    ug_ranks = CSP_calloc(user_newnproc + CSP_ENV.num_g * CSP_PROC.num_nodes, sizeof(int));
    CSP_ASSERT(ug_ranks != NULL);

    /* Get all ghost ranks and user ranks in COMM_WORLD. Reorder user ranks
     * to ensure each ghost rank followed by all bound user ranks. Thus
     * the offset translation becomes easy at offload.  */
    mpi_errno = ugcomm_gather_ranks(ug_newcomm, &ug_newcomm->ug_comm_nproc,
                                    ug_ranks, &num_max_g_users);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);
    ug_newcomm->num_ug_comms = num_max_g_users;

    if (ulrank == 0) {
        int lrank = 0;
        CSP_cwp_pkt_t pkt;
        CSP_cwp_fnc_ugcomm_create_pkt_t *ugcomm_create_pkt = &pkt.u.fnc_ugcomm_create;

        /* Lock ghost processes on all nodes. */
        mpi_errno = CSPU_mlock_acquire(ug_newcomm->user_root_comm);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);

        CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_PROC.local_comm, &lrank));

        /* First send start packet to root ghost. */
        CSP_cwp_init_pkt(CSP_CWP_FNC_UGCOMM_CREATE, &pkt);
        ugcomm_create_pkt->type = ug_newcomm->type;
        ugcomm_create_pkt->user_nproc = user_newnproc;
        ugcomm_create_pkt->ug_comm_nproc = ug_newcomm->ug_comm_nproc;
        ugcomm_create_pkt->user_local_root = lrank;
        ugcomm_create_pkt->wildcard_info = ug_newcomm->info_args.wildcard_used;
        ugcomm_create_pkt->num_ug_comms = num_max_g_users;

        mpi_errno = CSPU_cwp_issue(&pkt);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);

        /* Then broadcast ranks to all local ghosts. */
        mpi_errno = CSPU_cwp_bcast_params(ug_ranks, ug_newcomm->ug_comm_nproc * sizeof(int));
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

    /* Create the new ug_comm.
     * We always create from COMM_WORLD because the parent communicator may not be
     * extended to ug_comm depending on user hint. */
    CSP_CALLMPI(JUMP, PMPI_Group_incl(CSP_PROC.wgroup, ug_newcomm->ug_comm_nproc,
                                      ug_ranks, &ug_newgroup));
    CSP_CALLMPI(JUMP, PMPI_Comm_create_group(CSP_PROC.wcomm, ug_newgroup, 0, &ug_newcomm->ug_comm));
    CSP_DBG_PRINT("COMM: created ug_newcomm->ug_comm=0x%x, ug_comm_nproc=%d, num_ug_comms=%d\n",
                  ug_newcomm->ug_comm, ug_newcomm->ug_comm_nproc, ug_newcomm->num_ug_comms);

    if (ug_newcomm->type == CSP_COMM_ASYNC_DUP) {
        /* Reuse the first ug_comm, and duplicate others when async is enabled.
         * Note that it is not needed for CSP_COMM_ASYNC_TAG type.*/
        ug_newcomm->dup_ug_comms = CSP_calloc(num_max_g_users, sizeof(MPI_Comm));
        ug_newcomm->dup_ug_comms[0] = ug_newcomm->ug_comm;
        for (i = 1; i < num_max_g_users; i++) {
            CSP_CALLMPI(JUMP, PMPI_Comm_dup(ug_newcomm->ug_comm, &ug_newcomm->dup_ug_comms[i]));
            CSP_DBG_PRINT("COMM: dup ug_newcomm->dup_ug_comms[%d]=0x%x\n", i,
                          ug_newcomm->dup_ug_comms[i]);
        }
    }

    if (ug_newgroup && ug_newgroup != MPI_GROUP_NULL)
        CSP_CALLMPI(JUMP, PMPI_Group_free(&ug_newgroup));

  fn_exit:
    if (ug_ranks)
        free(ug_ranks);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static inline void ugcomm_info_init(CSPU_comm_t * ug_comm, CSPU_comm_t * ug_newcomm)
{
    /* Get info from parent reference info. */
    if (ug_comm) {
        ug_newcomm->info_args.wildcard_used = ug_comm->ref_info_args.wildcard_used;
        ug_newcomm->info_args.shmbuf_regist = ug_comm->info_args.shmbuf_regist;
        ug_newcomm->info_args.offload_min_msgsz = ug_comm->info_args.offload_min_msgsz;
    }
    else {
        /* Reset info if no parent (COMM_WORLD) */
        ug_newcomm->info_args.wildcard_used = CSP_COMM_INFO_WD_ANYSRC;
        ug_newcomm->info_args.shmbuf_regist = 0;
        ug_newcomm->info_args.offload_min_msgsz = CSP_ENV.offload_min_msgsz;
    }

    /* Reset my reference info for child. */
    ug_newcomm->ref_info_args.wildcard_used = CSP_COMM_INFO_WD_ANYSRC;
    ug_newcomm->ref_info_args.shmbuf_regist = 0;
    ug_newcomm->ref_info_args.offload_min_msgsz = CSP_ENV.offload_min_msgsz;
}

static inline int ugcomm_print_info(CSPU_comm_t * ug_comm)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank = 0;

    CSP_CALLMPI(RETURN, PMPI_Comm_rank(ug_comm->comm, &user_rank));
    if (user_rank == 0) {
        const char *strs[3];
        char wc_joined_str[64];
        int nstrs = 0;

        if (ug_comm->info_args.wildcard_used & CSP_COMM_INFO_WD_NONE)
            strs[nstrs++] = "none";
        if (ug_comm->info_args.wildcard_used & CSP_COMM_INFO_WD_ANYSRC)
            strs[nstrs++] = "anysrc";
        if (ug_comm->info_args.wildcard_used & CSP_COMM_INFO_WD_ANYTAG_NOTAG)
            strs[nstrs++] = "anysrc_notag";
        CSP_strjoin(strs, nstrs, "|", 64, &wc_joined_str[0]);

        CSP_msg_print(CSP_MSG_CONFIG_COMM, "CASPER comm: 0x%lx (%s) "
                      "offload_min_msgsz = %ld, wildcard_used = %s, count of communicators = %d\n",
                      (unsigned long) ug_comm->comm, CSP_ug_comm_type_name[ug_comm->type],
                      ug_comm->info_args.offload_min_msgsz, wc_joined_str, ug_comm->num_ug_comms);
    }
    return mpi_errno;
}

int CSPU_ugcomm_set_info(CSPU_comm_info_args_t * info_args, MPI_Info info)
{
    int mpi_errno = MPI_SUCCESS;

    if (info != MPI_INFO_NULL) {
        int info_flag = 0;
        char info_value[MPI_MAX_INFO_VAL + 1];

        /* Check if user specifies wildcard types */
        memset(info_value, 0, sizeof(info_value));
        CSP_CALLMPI(JUMP, PMPI_Info_get(info, "wildcard_used", MPI_MAX_INFO_VAL,
                                        info_value, &info_flag));
        if (info_flag == 1) {
            int wildcard_used = 0;
            char *type = NULL;

            type = strtok(info_value, ",|;");
            while (type != NULL) {
                if (!strncmp(type, "none", strlen("none"))) {
                    wildcard_used = (int) CSP_COMM_INFO_WD_NONE;
                    break;      /* do not check other types */
                }
                else if (!strncmp(type, "anysrc", strlen("anysrc"))) {
                    wildcard_used |= (int) CSP_COMM_INFO_WD_ANYSRC;
                }
                else if (!strncmp(type, "anytag_notag", strlen("anytag_notag"))) {
                    wildcard_used |= (int) CSP_COMM_INFO_WD_ANYTAG_NOTAG;
                }
                type = strtok(NULL, "|");
            }

            info_args->wildcard_used = wildcard_used;
        }

        /* Check if user specifies message offloading threshold types */
        memset(info_value, 0, sizeof(info_value));
        CSP_CALLMPI(JUMP, PMPI_Info_get(info, "offload_min_msgsz", MPI_MAX_INFO_VAL,
                                        info_value, &info_flag));
        if (info_flag == 1)
            info_args->offload_min_msgsz = atol(info_value);

        mpi_errno =
            CSPU_info_get_bool(info, "shmbuf_regist", "true", "false", &info_args->shmbuf_regist);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int CSPU_ugcomm_free(MPI_Comm comm)
{
    int mpi_errno = MPI_SUCCESS;
    int ulrank = 0;
    CSPU_comm_t *ug_comm = NULL;

    CSPU_fetch_ug_comm_from_cache(comm, &ug_comm);
    if (ug_comm) {

        /* NOTE: reference ugcomm does not have ghost-included structure. */
        if (ug_comm->type > CSP_COMM_REFER) {
            /* Local user root issues command to ghosts. */
            CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_comm->local_user_comm, &ulrank));
            if (ulrank == 0) {
                mpi_errno = ugcomm_issue_ghost_cmd(ug_comm);
                CSP_CHKMPIFAIL_JUMP(mpi_errno);
            }
        }

        mpi_errno = ugcomm_release(ug_comm);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int CSPU_ugcomm_create(MPI_Comm comm, MPI_Info info, MPI_Comm user_newcomm)
{
    int mpi_errno = MPI_SUCCESS;
    int ulrank = 0;
    CSPU_comm_t *ug_newcomm = NULL, *ug_comm = NULL;

    if (comm != MPI_COMM_NULL)
        CSPU_fetch_ug_comm_from_cache(comm, &ug_comm);

    ug_newcomm = CSP_calloc(1, sizeof(CSPU_comm_t));
    CSP_ASSERT(ug_newcomm != NULL);

    ug_newcomm->comm = user_newcomm;
    ug_newcomm->type = CSP_COMM_REFER;

    /* First init my info by using parent's ref_info. */
    ugcomm_info_init(ug_comm, ug_newcomm);

    /* Overwrite info if set. But note that it cannot be update by
     * later comm_set_info. */
    mpi_errno = CSPU_ugcomm_set_info(&ug_newcomm->info_args, info);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* Mark as shmbuf communicator. */
    if (ug_newcomm->info_args.shmbuf_regist) {
        ug_newcomm->type = CSP_COMM_SHMBUF;
    }

    /* Enable async progress if ignore status or no ANY_SRC + specific TAG. */
    if (!(ug_newcomm->info_args.wildcard_used & CSP_COMM_INFO_WD_ANYSRC) ||
        (ug_newcomm->info_args.wildcard_used & CSP_COMM_INFO_WD_ANYTAG_NOTAG) ||
        (ug_newcomm->info_args.wildcard_used & CSP_COMM_INFO_WD_NONE)) {
        ug_newcomm->type = CSP_COMM_ASYNC_DUP;
    }
    /* Use tag translation instead of dupcomm. */
    if ((ug_newcomm->info_args.wildcard_used & CSP_COMM_INFO_WD_NONE) &&
        CSPU_offload_ch.tag_trans.trans_tag_nbits > 0 /* ensure sufficient bits exist. */) {
        ug_newcomm->type = CSP_COMM_ASYNC_TAG;
    }

    /* Return empty ug_comm if it is only reference use. */
    if (ug_newcomm->type == CSP_COMM_REFER)
        goto fn_refer;

    /* Create user root communicator */
    if (user_newcomm == CSP_COMM_USER_WORLD) {
        ug_newcomm->local_user_comm = CSP_PROC.user.u_local_comm;
        ug_newcomm->user_root_comm = CSP_PROC.user.ur_comm;
        CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_newcomm->local_user_comm, &ulrank));
    }
    else {
        CSP_CALLMPI(JUMP, PMPI_Comm_split_type(user_newcomm, MPI_COMM_TYPE_SHARED, 0,
                                               MPI_INFO_NULL, &ug_newcomm->local_user_comm));
        CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_newcomm->local_user_comm, &ulrank));
        CSP_CALLMPI(JUMP, PMPI_Comm_split(user_newcomm, ulrank == 0, 1,
                                          &ug_newcomm->user_root_comm));
    }

    mpi_errno = ugcomm_create_comm(ug_newcomm);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    mpi_errno = ugcomm_exchange_granks_bound(ug_newcomm);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    mpi_errno = ugcomm_gather_handles(ug_newcomm);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    CSP_CALLMPI(JUMP, PMPI_Comm_group(ug_newcomm->comm, &ug_newcomm->group));
    CSP_CALLMPI(JUMP, PMPI_Comm_group(ug_newcomm->ug_comm, &ug_newcomm->ug_group));

  fn_refer:
    /* Cache ug_comm in user newcomm. */
    mpi_errno = CSPU_cache_ug_comm(ug_newcomm->comm, ug_newcomm);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    CSP_DBG_PRINT
        ("COMM: create user_newcomm 0x%x -> ug_newcomm %p ug_comm 0x%x (type: %s)\n",
         user_newcomm, ug_newcomm, ug_newcomm->ug_comm, CSP_ug_comm_type_name[ug_newcomm->type]);

    ugcomm_print_info(ug_newcomm);

  fn_exit:
    return mpi_errno;

  fn_fail:
    ugcomm_release(ug_newcomm);
    goto fn_exit;
}

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */


#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

const char *CSP_ug_comm_type_name[CSP_COMM_TYPE_MAX] =
    { "refer", "shmbuf", "async", "async_nodup" };

static int ugcomm_gather_ranks(MPI_Comm user_newcomm, int *num_ghosts_unique,
                               int *g_wranks_unique, int *u_wranks, int *num_max_g_users)
{
    int mpi_errno = MPI_SUCCESS;
    int user_newnproc, world_nproc;
    int tmp_num_ghosts, i, j;
    int *gp_bitmap = NULL;
    int *user_newuranks = NULL, *user_uwranks = NULL;
    MPI_Group user_newgroup = MPI_GROUP_NULL, uwgroup = MPI_GROUP_NULL;
    int max_g_users = 0;

    CSP_CALLMPI(JUMP, PMPI_Comm_size(MPI_COMM_WORLD, &world_nproc));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(user_newcomm, &user_newnproc));
    CSP_CALLMPI(JUMP, PMPI_Comm_group(user_newcomm, &user_newgroup));
    CSP_CALLMPI(JUMP, PMPI_Comm_group(CSP_COMM_USER_WORLD, &uwgroup));

    gp_bitmap = CSP_calloc(world_nproc, sizeof(int));
    user_newuranks = CSP_calloc(user_newnproc, sizeof(int));
    user_uwranks = CSP_calloc(user_newnproc, sizeof(int));
    if (gp_bitmap == NULL || user_newuranks == NULL || user_uwranks == NULL)
        goto fn_fail;

    for (i = 0; i < user_newnproc; i++)
        user_newuranks[i] = i;

    /* Get all user rank in COMM_WORLD, used to create ug_comm. */
    CSP_CALLMPI(JUMP, PMPI_Group_translate_ranks(user_newgroup, user_newnproc,
                                                 user_newuranks, CSP_PROC.wgroup, u_wranks));
    /* Get all user rank in COMM_USER_WORLD, local use only. */
    CSP_CALLMPI(JUMP, PMPI_Group_translate_ranks(user_newgroup, user_newnproc,
                                                 user_newuranks, uwgroup, user_uwranks));

    /* Get unique ghost world ranks of each USER process.
     * Used to create communicator. We use world rank instead of rank in
     * parent ug_comm to minimize the additional memory per communicator. */
    tmp_num_ghosts = 0;
    for (i = 0; i < user_newnproc; i++) {
        int uwrank = user_uwranks[i];

        for (j = 0; j < CSP_ENV.num_g; j++) {
            int g_wrank = CSP_PROC.user.g_wranks_per_user[uwrank * CSP_ENV.num_g + j];

            /* Unique ghost ranks */
            if (!gp_bitmap[g_wrank]) {
                g_wranks_unique[tmp_num_ghosts++] = g_wrank;
                CSP_ASSERT(tmp_num_ghosts <= CSP_ENV.num_g * CSP_PROC.num_nodes);
            }
            gp_bitmap[g_wrank]++;

            /* Maximum number of users per ghost process. */
            if (max_g_users < gp_bitmap[g_wrank])
                max_g_users = gp_bitmap[g_wrank];
        }
    }

    *num_ghosts_unique = tmp_num_ghosts;
    *num_max_g_users = max_g_users;

    if (user_newgroup)
        CSP_CALLMPI(JUMP, PMPI_Group_free(&user_newgroup));
    if (uwgroup)
        CSP_CALLMPI(JUMP, PMPI_Group_free(&uwgroup));

  fn_exit:
    if (user_uwranks)
        free(user_uwranks);
    if (user_newuranks)
        free(user_newuranks);
    if (gp_bitmap)
        free(gp_bitmap);
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

    if (ug_comm->type == CSP_COMM_ASYNC && ug_comm->dup_ug_comms) {
        /* Skip the first, which reuses ug_comm. */
        for (i = 1; i < ug_comm->num_max_g_users; i++) {
            if (ug_comm->dup_ug_comms[i] && ug_comm->dup_ug_comms[i] != MPI_COMM_NULL) {
                CSP_CALLMPI(JUMP, PMPI_Comm_free(&ug_comm->dup_ug_comms[i]));
            }
        }
        free(ug_comm->dup_ug_comms);
    }

    if (ug_comm->g_ranks_bound)
        free(ug_comm->g_ranks_bound);
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
    MPI_Group lgroup = MPI_GROUP_NULL, ug_newgroup = MPI_GROUP_NULL;
    int user_newrank, user_newnproc;
    int *local_g_ranks_bound = NULL, lurank = 0, lunproc = 0;
    int i, u_offset = 0;

    CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_newcomm->comm, &user_newnproc));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_newcomm->comm, &user_newrank));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_newcomm->local_user_comm, &lurank));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_newcomm->local_user_comm, &lunproc));

    /* Decide my offset on the bound ghost process according to the local rank. */
    local_g_ranks_bound = CSP_calloc(lunproc, sizeof(int));
    CSP_ASSERT(local_g_ranks_bound != NULL);
    local_g_ranks_bound[lurank] = CSPU_offload_ch.bound_g_lrank;
    CSP_CALLMPI(JUMP, PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, local_g_ranks_bound,
                                     1, MPI_INT, ug_newcomm->local_user_comm));
    u_offset = 0;
    for (i = 0; i < lunproc; i++) {
        if (i == lurank)
            break;
        if (local_g_ranks_bound[i] == CSPU_offload_ch.bound_g_lrank)
            u_offset++;
    }

    /* Exchange bound ghost rank in new ug_comm.
     * Ghost is statically bound at MPI init, thus each ghost only needs to
     * check fixed offload queues. */
    ug_newcomm->g_ranks_bound = CSP_calloc(user_newnproc, sizeof(int) * 2);
    CSP_ASSERT(ug_newcomm->g_ranks_bound != NULL);

    CSP_CALLMPI(JUMP, PMPI_Comm_group(CSP_PROC.local_comm, &lgroup));
    CSP_CALLMPI(JUMP, PMPI_Comm_group(ug_newcomm->ug_comm, &ug_newgroup));

    CSP_CALLMPI(JUMP, PMPI_Group_translate_ranks(lgroup, 1,
                                                 &CSPU_offload_ch.bound_g_lrank, ug_newgroup,
                                                 &ug_newcomm->g_ranks_bound[user_newrank].g_rank));
    ug_newcomm->g_ranks_bound[user_newrank].u_offset = u_offset;

    CSP_CALLMPI(JUMP, PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, ug_newcomm->g_ranks_bound,
                                     2, MPI_INT, ug_newcomm->comm));
    CSP_DBG_PRINT("COMM: exchanged my g_ranks_bound[%d] g_rank %d, u_offset %d\n",
                  user_newrank, ug_newcomm->g_ranks_bound[user_newrank].g_rank,
                  ug_newcomm->g_ranks_bound[user_newrank].u_offset);

    if (lgroup && lgroup != MPI_GROUP_NULL)
        CSP_CALLMPI(JUMP, PMPI_Group_free(&lgroup));
    if (ug_newgroup && ug_newgroup != MPI_GROUP_NULL)
        CSP_CALLMPI(JUMP, PMPI_Group_free(&ug_newgroup));

  fn_exit:
    if (local_g_ranks_bound)
        free(local_g_ranks_bound);
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
    int *g_wranks_unique_ptr, *u_wranks_ptr;
    MPI_Group ug_newgroup = MPI_GROUP_NULL;
    int num_max_g_users = 0;
    int i;

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_newcomm->local_user_comm, &ulrank));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_newcomm->comm, &user_newnproc));

    /* Prepare u_ranks, g_ranks_unique in parent ug_comm.
     * The upper size of g_ranks_unique can be num_ghost * num_nodes.
     * The same array is reused to create new group, so be sure to put
     * the non-fixed-size g_ranks_unique at the end. */
    ug_ranks = CSP_calloc(user_newnproc + CSP_ENV.num_g * CSP_PROC.num_nodes, sizeof(int));
    CSP_ASSERT(ug_ranks != NULL);

    u_wranks_ptr = ug_ranks;
    g_wranks_unique_ptr = &ug_ranks[user_newnproc];

    /* Get all ghost ranks and user ranks in COMM_WORLD. */
    mpi_errno = ugcomm_gather_ranks(ug_newcomm->comm, &ug_newcomm->num_ghosts_unique,
                                    g_wranks_unique_ptr, u_wranks_ptr, &num_max_g_users);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);
    ug_newcomm->num_max_g_users = num_max_g_users;

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
        ugcomm_create_pkt->num_ghosts_unique = ug_newcomm->num_ghosts_unique;
        ugcomm_create_pkt->user_nprocs = user_newnproc;
        ugcomm_create_pkt->user_local_root = lrank;
        ugcomm_create_pkt->wildcard_info = ug_newcomm->info_args.wildcard_used;
        if (ug_newcomm->type == CSP_COMM_ASYNC)
            ugcomm_create_pkt->num_ug_comms = num_max_g_users;

        mpi_errno = CSPU_cwp_issue(&pkt);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);

        /* Then broadcast ranks to all local ghosts. */
        mpi_errno = CSPU_cwp_bcast_params(ug_ranks,
                                          (user_newnproc +
                                           ug_newcomm->num_ghosts_unique) * sizeof(int));
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

    /* Create the new ug_comm.
     * We always create from COMM_WORLD because the parent communicator may not be
     * extended to ug_comm depending on user hint. */
    CSP_CALLMPI(JUMP, PMPI_Group_incl(CSP_PROC.wgroup,
                                      user_newnproc + ug_newcomm->num_ghosts_unique,
                                      ug_ranks, &ug_newgroup));
    CSP_CALLMPI(JUMP, PMPI_Comm_create_group(MPI_COMM_WORLD, ug_newgroup, 0, &ug_newcomm->ug_comm));
    CSP_DBG_PRINT("COMM: created ug_newcomm->ug_comm=0x%x, num_max_g_users=%d\n",
                  ug_newcomm->ug_comm, ug_newcomm->num_max_g_users);

    if (ug_newcomm->type == CSP_COMM_ASYNC) {
        /* Reuse the first ug_comm, and duplicate others when async is enabled.
         * Note that it is not needed for CSP_COMM_ASYNC_NODUP type.*/
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
    }
    else {
        /* Reset info if no parent (COMM_WORLD) */
        ug_newcomm->info_args.wildcard_used = CSP_COMM_INFO_WD_ANYSRC;
        ug_newcomm->info_args.shmbuf_regist = 0;
    }

    /* Reset my reference info for child. */
    ug_newcomm->ref_info_args.wildcard_used = CSP_COMM_INFO_WD_ANYSRC;
    ug_newcomm->ref_info_args.shmbuf_regist = 0;
}

static inline int ugcomm_print_info(CSPU_comm_t * ug_comm)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank = 0;

    CSP_CALLMPI(RETURN, PMPI_Comm_rank(ug_comm->comm, &user_rank));
    if (user_rank == 0) {
        CSP_msg_print(CSP_MSG_CONFIG_COMM, "CASPER comm: 0x%x (%s)\n"
                      "    wildcard_used  = %s|%s|%s\n"
                      "    shmbuf_regist  = %d\n",
                      ug_comm->comm, CSP_ug_comm_type_name[ug_comm->type],
                      (ug_comm->info_args.wildcard_used & CSP_COMM_INFO_WD_NONE ? "none" : ""),
                      (ug_comm->info_args.wildcard_used & CSP_COMM_INFO_WD_ANYSRC ? "anysrc" : ""),
                      (ug_comm->
                       info_args.wildcard_used & CSP_COMM_INFO_WD_ANYTAG_NOTAG ? "anytag_notag" :
                       ""), ug_comm->info_args.shmbuf_regist);
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
        ug_newcomm->type = CSP_COMM_ASYNC;
    }
    /* Use tag translation instead of dupcomm. */
    if ((ug_newcomm->info_args.wildcard_used & CSP_COMM_INFO_WD_NONE) &&
        CSPU_offload_ch.trans_tag_nbits > 0 /* ensure sufficient bits exist. */) {
        ug_newcomm->type = CSP_COMM_ASYNC_NODUP;
    }

    /* Return empty ug_comm if it is only reference use. */
    if (ug_newcomm->type == CSP_COMM_REFER)
        goto no_async;

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

  no_async:
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

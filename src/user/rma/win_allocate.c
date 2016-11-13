/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include "cspu.h"

#include <ctype.h>

/* String of CSPU_target_epoch_stat_t enum (for debug). */
const char *CSPU_target_epoch_stat_name[4] = {
    "NO_EPOCH",
    "LOCK",
    "PSCW"
};

/* String of CSPU_win_epoch_stat_t enum (for debug). */
const char *CSPU_win_epoch_stat_name[4] = {
    "NO_EPOCH",
    "FENCE",
    "LOCK_ALL",
    "PER_TARGET"
};

static int read_win_info(MPI_Info info, CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank = -1;

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->user_comm, &user_rank));

    ug_win->info_args.no_local_load_store = 0;
    ug_win->info_args.epochs_used = CSP_EPOCH_LOCK_ALL | CSP_EPOCH_LOCK |
        CSP_EPOCH_PSCW | CSP_EPOCH_FENCE;
    ug_win->info_args.async_config = CSP_ENV.async_config;      /* default */

    if (info != MPI_INFO_NULL) {
        int info_flag = 0;
        char info_value[MPI_MAX_INFO_VAL + 1];

        /* Check if user wants to turn off async */
        memset(info_value, 0, sizeof(info_value));
        CSP_CALLMPI(JUMP, PMPI_Info_get(info, "async_config", MPI_MAX_INFO_VAL,
                                        info_value, &info_flag));

        if (info_flag == 1) {
            if (!strncmp(info_value, "off", strlen("off"))) {
                ug_win->info_args.async_config = CSP_ASYNC_CONFIG_OFF;
            }
            else if (!strncmp(info_value, "on", strlen("on"))) {
                ug_win->info_args.async_config = CSP_ASYNC_CONFIG_ON;
            }
        }

        /* Check if we are allowed to ignore force-lock for local target,
         * require force-lock by default. */
        memset(info_value, 0, sizeof(info_value));
        CSP_CALLMPI(JUMP, PMPI_Info_get(info, "no_local_load_store", MPI_MAX_INFO_VAL,
                                        info_value, &info_flag));

        if (info_flag == 1) {
            if (!strncmp(info_value, "true", strlen("true")))
                ug_win->info_args.no_local_load_store = 1;
        }

        /* Check if user specifies epoch types */
        memset(info_value, 0, sizeof(info_value));
        CSP_CALLMPI(JUMP, PMPI_Info_get(info, "epochs_used", MPI_MAX_INFO_VAL,
                                        info_value, &info_flag));

        if (info_flag == 1) {
            int user_epochs_used = 0;
            char *type = NULL;

            type = strtok(info_value, ",|;");
            while (type != NULL) {
                if (!strncmp(type, "lockall", strlen("lockall"))) {
                    user_epochs_used |= (int) CSP_EPOCH_LOCK_ALL;
                }
                else if (!strncmp(type, "lock", strlen("lock"))) {
                    user_epochs_used |= (int) CSP_EPOCH_LOCK;
                }
                else if (!strncmp(type, "pscw", strlen("pscw"))) {
                    user_epochs_used |= (int) CSP_EPOCH_PSCW;
                }
                else if (!strncmp(type, "fence", strlen("fence"))) {
                    user_epochs_used |= (int) CSP_EPOCH_FENCE;
                }
                type = strtok(NULL, "|");
            }

            if (user_epochs_used != 0)
                ug_win->info_args.epochs_used = user_epochs_used;
        }

        /* Check if user sets window name.
         * It is not passed to MPI, only for casper debug use). For the name
         * user wants to pass to MPI, should call MPI_Win_set_name instead. */
        memset(ug_win->info_args.win_name, 0, sizeof(ug_win->info_args.win_name));
        memset(info_value, 0, sizeof(info_value));
        CSP_CALLMPI(JUMP, PMPI_Info_get(info, "win_name", MPI_MAX_INFO_VAL,
                                        info_value, &info_flag));

        if (info_flag == 1) {
            strncpy(ug_win->info_args.win_name, info_value, MPI_MAX_OBJECT_NAME);
        }
    }

    CSP_DBG_PRINT("no_local_load_store %d, epochs_used=%s|%s|%s|%s\n",
                  ug_win->info_args.no_local_load_store,
                  ((ug_win->info_args.epochs_used & CSP_EPOCH_LOCK_ALL) ? "lockall" : ""),
                  ((ug_win->info_args.epochs_used & CSP_EPOCH_LOCK) ? "lock" : ""),
                  ((ug_win->info_args.epochs_used & CSP_EPOCH_PSCW) ? "pscw" : ""),
                  ((ug_win->info_args.epochs_used & CSP_EPOCH_FENCE) ? "fence" : ""));

    if (user_rank == 0) {
        CSP_msg_print(CSP_MSG_CONFIG_WIN, "CASPER Window: %s \n"
                      "    no_local_load_store = %s\n"
                      "    epochs_used = %s%s%s%s\n"
                      "    async_config = %s\n\n",
                      ug_win->info_args.win_name,
                      (ug_win->info_args.no_local_load_store ? "TRUE" : " FALSE"),
                      ((ug_win->info_args.epochs_used & CSP_EPOCH_LOCK_ALL) ? "lockall" : ""),
                      ((ug_win->info_args.epochs_used & CSP_EPOCH_LOCK) ? "|lock" : ""),
                      ((ug_win->info_args.epochs_used & CSP_EPOCH_PSCW) ? "|pscw" : ""),
                      ((ug_win->info_args.epochs_used & CSP_EPOCH_FENCE) ? "|fence" : ""),
                      ((ug_win->info_args.async_config == CSP_ASYNC_CONFIG_ON) ? "on" : "off"));
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

/* Gather parameters from all the local ghosts (blocking call).
 * It is OK to be blocking, since all of them are guaranteed to be in current function. */
static int gather_ghost_cmd_params(void *params, size_t size)
{
    int mpi_errno = MPI_SUCCESS;
    int i;
    MPI_Status *stats = NULL;
    MPI_Request *reqs = NULL;
    size_t offset = 0;

    reqs = (MPI_Request *) CSP_calloc(CSP_ENV.num_g, sizeof(MPI_Request));
    stats = (MPI_Status *) CSP_calloc(CSP_ENV.num_g, sizeof(MPI_Status));

    /* ghosts are always start from rank 0 on local communicator. */
    for (i = 0; i < CSP_ENV.num_g; i++) {
        offset = i * size;
        CSP_CALLMPI(JUMP, PMPI_Irecv(((char *) params + offset), size, MPI_CHAR, i,
                                     CSP_CWP_PARAM_TAG, CSP_PROC.local_comm, &reqs[i]));
    }

    CSP_CALLMPI(JUMP, PMPI_Waitall(CSP_ENV.num_g, reqs, stats));

  fn_exit:
    if (stats)
        free(stats);
    if (reqs)
        free(reqs);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}


/* Distribute parameters to all the local ghosts (blocking call).
 * It is OK to be blocking, since all of them are guaranteed to be in current function. */
static int bcast_ghost_cmd_params(void *params, size_t size)
{
    int mpi_errno = MPI_SUCCESS;
    int i;
    MPI_Status *stats = NULL;
    MPI_Request *reqs = NULL;

    reqs = (MPI_Request *) CSP_calloc(CSP_ENV.num_g, sizeof(MPI_Request));
    stats = (MPI_Status *) CSP_calloc(CSP_ENV.num_g, sizeof(MPI_Status));

    /* ghosts are always start from rank 0 on local communicator. */
    for (i = 0; i < CSP_ENV.num_g; i++) {
        CSP_CALLMPI(JUMP, PMPI_Isend(params, size, MPI_CHAR, i, CSP_CWP_PARAM_TAG,
                                     CSP_PROC.local_comm, &reqs[i]));
    }

    CSP_CALLMPI(JUMP, PMPI_Waitall(CSP_ENV.num_g, reqs, stats));

  fn_exit:
    if (stats)
        free(stats);
    if (reqs)
        free(reqs);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int issue_ghost_cmd(int user_nprocs, MPI_Info info, CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_cwp_pkt_t pkt;
    CSP_cwp_fnc_winalloc_pkt_t *winalloc_pkt = &pkt.u.fnc_winalloc;
    CSP_info_keyval_t *info_keyvals = NULL;
    int user_local_rank = 0;
    int npairs = 0;

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_PROC.local_comm, &user_local_rank));

    /* Previous allgather ensures all user roots have arrived, thus skip
     * barrier(user_root_comm). */

    /* Lock ghost processes on all nodes. */
    mpi_errno = CSPU_mlock_acquire(ug_win->user_root_comm);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    CSP_cwp_init_pkt(CSP_CWP_FNC_WIN_ALLOCATE, &pkt);
    winalloc_pkt->user_local_root = user_local_rank;
    winalloc_pkt->user_nprocs = user_nprocs;
    winalloc_pkt->epochs_used = ug_win->info_args.epochs_used;
    winalloc_pkt->max_local_user_nprocs = ug_win->max_local_user_nprocs;
    winalloc_pkt->is_u_world = (ug_win->user_comm == CSP_COMM_USER_WORLD) ? 1 : 0;

    mpi_errno = CSP_info_deserialize(info, &info_keyvals, &npairs);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    winalloc_pkt->info_npairs = npairs;

    /* Only send start request to root ghost. */
    mpi_errno = CSPU_cwp_issue(&pkt);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* Send user info */
    if (npairs > 0 && info_keyvals) {
        mpi_errno = bcast_ghost_cmd_params(info_keyvals, sizeof(CSP_info_keyval_t) * npairs);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
        CSP_DBG_PRINT(" Send parameters: info\n");
    }

  fn_exit:
    if (info_keyvals && npairs > 0)
        free(info_keyvals);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int gather_ranks(CSPU_win_t * win, int *num_ghosts, int *gp_ranks_in_world,
                        int *unique_gp_ranks_in_world)
{
    int mpi_errno = MPI_SUCCESS;
    int user_nprocs;
    int *gp_bitmap = NULL;
    int user_world_rank, tmp_num_ghosts;
    int i, j, gp_rank;
    int world_nprocs;

    CSP_CALLMPI(JUMP, PMPI_Comm_size(MPI_COMM_WORLD, &world_nprocs));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(win->user_comm, &user_nprocs));

    gp_bitmap = CSP_calloc(world_nprocs, sizeof(int));
    if (gp_bitmap == NULL)
        goto fn_fail;

    /* Get ghost ranks of each USER process.
     *
     * The ghosts of user_world rank x are stored as x*num_g: (x+1)*num_g-1,
     * it is used to catch ghosts for a target rank in epoch.
     * Unique ghost ranks are only used for creating communicators.*/
    tmp_num_ghosts = 0;
    for (i = 0; i < user_nprocs; i++) {
        user_world_rank = win->targets[i].user_world_rank;

        for (j = 0; j < CSP_ENV.num_g; j++) {
            gp_rank = CSP_PROC.user.g_wranks_per_user[user_world_rank * CSP_ENV.num_g + j];
            gp_ranks_in_world[i * CSP_ENV.num_g + j] = gp_rank;

            /* Unique ghost ranks */
            if (!gp_bitmap[gp_rank]) {
                unique_gp_ranks_in_world[tmp_num_ghosts++] = gp_rank;
                gp_bitmap[gp_rank] = 1;

                CSP_ASSERT(tmp_num_ghosts <= CSP_PROC.num_nodes * CSP_ENV.num_g);
            }
        }
    }
    *num_ghosts = tmp_num_ghosts;

  fn_exit:
    if (gp_bitmap)
        free(gp_bitmap);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}



static int create_ug_comm(int num_ghosts, int *gp_ranks_in_world, CSPU_win_t * win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_nprocs, user_rank, world_nprocs;
    int *ug_ranks_in_world = NULL;
    int i, num_ug_ranks;

    CSP_CALLMPI(JUMP, PMPI_Comm_size(win->user_comm, &user_nprocs));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(win->user_comm, &user_rank));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(MPI_COMM_WORLD, &world_nprocs));

    /* maximum amount equals to world size */
    ug_ranks_in_world = CSP_calloc(world_nprocs, sizeof(int));
    if (ug_ranks_in_world == NULL)
        goto fn_fail;

    /* -Create ug communicator including all USER processes and Ghost processes. */
    num_ug_ranks = num_ghosts;
    memcpy(ug_ranks_in_world, gp_ranks_in_world, num_ghosts * sizeof(int));
    for (i = 0; i < user_nprocs; i++) {
        ug_ranks_in_world[num_ug_ranks++] = win->targets[i].world_rank;
    }

    CSP_ASSERT(num_ug_ranks <= world_nprocs);

    CSP_CALLMPI(JUMP, PMPI_Group_incl(CSP_PROC.wgroup, num_ug_ranks,
                                      ug_ranks_in_world, &win->ug_group));
    CSP_CALLMPI(JUMP, PMPI_Comm_create_group(MPI_COMM_WORLD, win->ug_group, 0, &win->ug_comm));

  fn_exit:
    if (ug_ranks_in_world)
        free(ug_ranks_in_world);

    return mpi_errno;

  fn_fail:
    if (win->ug_comm != MPI_COMM_NULL) {
        CSP_CALLMPI_EXIT(PMPI_Comm_free(&win->ug_comm));
        win->ug_comm = MPI_COMM_NULL;
    }
    if (win->ug_group != MPI_GROUP_NULL) {
        CSP_CALLMPI_EXIT(PMPI_Group_free(&win->ug_group));
        win->ug_group = MPI_GROUP_NULL;
    }

    goto fn_exit;
}

static int create_communicators(CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int *cmd_params = NULL;
    int *user_ranks_in_world = NULL;
    int *gp_ranks_in_world = NULL, *unique_gp_rank_in_world = NULL;
    int num_ghosts = 0, max_num_ghosts;
    int user_nprocs, user_local_rank;
    int *gp_ranks_in_ug = NULL;
    int i;

    CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_win->user_comm, &user_nprocs));
    max_num_ghosts = CSP_ENV.num_g * CSP_PROC.num_nodes;

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->local_user_comm, &user_local_rank));
    ug_win->num_g_ranks_in_ug = CSP_ENV.num_g * ug_win->num_nodes;

    /* Optimization for user world communicator */
    if (ug_win->user_comm == CSP_COMM_USER_WORLD) {

        /* Create communicators
         *  local_ug_comm: including local USER and Ghost processes
         *  ug_comm: including all USER and Ghost processes
         */
        ug_win->local_ug_comm = CSP_PROC.local_comm;
        ug_win->ug_comm = MPI_COMM_WORLD;
        CSP_CALLMPI(JUMP, PMPI_Comm_group(ug_win->local_ug_comm, &ug_win->local_ug_group));
        CSP_CALLMPI(JUMP, PMPI_Comm_group(ug_win->ug_comm, &ug_win->ug_group));

        /* -Get all Ghost rank in ug communicator */
        memcpy(ug_win->g_ranks_in_ug, CSP_PROC.user.g_wranks_unique,
               ug_win->num_g_ranks_in_ug * sizeof(int));
        for (i = 0; i < user_nprocs; i++)
            memcpy(ug_win->targets[i].g_ranks_in_ug,
                   &CSP_PROC.user.g_wranks_per_user[i * CSP_ENV.num_g],
                   sizeof(int) * CSP_ENV.num_g);
    }
    else {
        /* ghost ranks for every user process, used for ghost fetching in epoch */
        gp_ranks_in_world = CSP_calloc(CSP_ENV.num_g * user_nprocs, sizeof(int));
        unique_gp_rank_in_world = CSP_calloc(CSP_ENV.num_g * ug_win->num_nodes, sizeof(int));
        gp_ranks_in_ug = CSP_calloc(CSP_ENV.num_g * user_nprocs, sizeof(int));

        /* Gather user rank information */
        mpi_errno = gather_ranks(ug_win, &num_ghosts, gp_ranks_in_world, unique_gp_rank_in_world);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);

        if (user_local_rank == 0) {
            /* Set parameters to local Ghosts
             *  [0]: num_ghosts
             *  [1:N]: user ranks in comm_world
             *  [N+1:]: ghost ranks in comm_world
             */
            int cmd_param_size = user_nprocs + max_num_ghosts + 1;
            cmd_params = CSP_calloc(cmd_param_size, sizeof(int));

            cmd_params[0] = num_ghosts;

            /* user ranks in comm_world */
            user_ranks_in_world = &cmd_params[1];
            for (i = 0; i < user_nprocs; i++)
                user_ranks_in_world[i] = ug_win->targets[i].world_rank;

            /* ghost ranks in comm_world */
            memcpy(&cmd_params[user_nprocs + 1], unique_gp_rank_in_world, num_ghosts * sizeof(int));
            mpi_errno = bcast_ghost_cmd_params(cmd_params, sizeof(int) * cmd_param_size);
            CSP_CHKMPIFAIL_JUMP(mpi_errno);
        }

        /* Create communicators
         *  ug_comm: including all USER and Ghost processes
         *  local_ug_comm: including local USER and Ghost processes
         */
        mpi_errno = create_ug_comm(num_ghosts, unique_gp_rank_in_world, ug_win);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);

#ifdef CSP_DEBUG
        {
            int ug_rank, ug_nprocs;
            CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->ug_comm, &ug_rank));
            CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_win->ug_comm, &ug_nprocs));
            CSP_DBG_PRINT("created ug_comm, my rank %d/%d\n", ug_rank, ug_nprocs);
        }
#endif

        CSP_CALLMPI(JUMP, PMPI_Comm_split_type(ug_win->ug_comm, MPI_COMM_TYPE_SHARED, 0,
                                               MPI_INFO_NULL, &ug_win->local_ug_comm));

        CSP_CALLMPI(JUMP, PMPI_Comm_group(ug_win->local_ug_comm, &ug_win->local_ug_group));

#ifdef CSP_DEBUG
        {
            int ug_rank, ug_nprocs;
            CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->local_ug_comm, &ug_rank));
            CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_win->local_ug_comm, &ug_nprocs));
            CSP_DBG_PRINT("created local_ug_comm, my rank %d/%d\n", ug_rank, ug_nprocs);
        }
#endif

        /* Get all ghost rank in ug communicator */
        CSP_CALLMPI(JUMP, PMPI_Group_translate_ranks(CSP_PROC.wgroup, num_ghosts,
                                                     unique_gp_rank_in_world, ug_win->ug_group,
                                                     ug_win->g_ranks_in_ug));

        CSP_CALLMPI(JUMP, PMPI_Group_translate_ranks(CSP_PROC.wgroup, user_nprocs * CSP_ENV.num_g,
                                                     gp_ranks_in_world, ug_win->ug_group,
                                                     gp_ranks_in_ug));

        for (i = 0; i < user_nprocs; i++)
            memcpy(ug_win->targets[i].g_ranks_in_ug, &gp_ranks_in_ug[i * CSP_ENV.num_g],
                   sizeof(int) * CSP_ENV.num_g);
    }

#ifdef CSP_DEBUG
    {
        int j;
        CSP_DBG_PRINT("%d unique g_ranks:\n", ug_win->num_g_ranks_in_ug);
        for (i = 0; i < ug_win->num_g_ranks_in_ug; i++) {
            CSP_DBG_PRINT("\t[%d] %d\n", i, ug_win->g_ranks_in_ug[i]);
        }
        CSP_DBG_PRINT("  per target g_ranks:\n");
        for (i = 0; i < user_nprocs; i++) {
            for (j = 0; j < CSP_ENV.num_g; j++) {
                CSP_DBG_PRINT("\ttarget[%d].g_ranks_in_ug[%d] %d\n", i, j,
                              ug_win->targets[i].g_ranks_in_ug[j]);
            }
        }
    }
#endif

  fn_exit:
    if (cmd_params)
        free(cmd_params);
    if (gp_ranks_in_world)
        free(gp_ranks_in_world);
    if (gp_ranks_in_ug)
        free(gp_ranks_in_ug);
    if (unique_gp_rank_in_world)
        free(unique_gp_rank_in_world);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int gather_base_offsets(CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint tmp_u_offsets;
    int i, j;
    int user_local_rank, user_local_nprocs, user_rank, user_nprocs;
    MPI_Aint *base_g_offsets = NULL;
    MPI_Aint root_g_size = 0;

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->local_user_comm, &user_local_rank));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_win->local_user_comm, &user_local_nprocs));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->user_comm, &user_rank));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_win->user_comm, &user_nprocs));

    base_g_offsets = CSP_calloc(user_nprocs * CSP_ENV.num_g, sizeof(MPI_Aint));

#ifdef CSP_ENABLE_GRANT_LOCK_HIDDEN_BYTE
    /* All the ghosts use the byte located on ghost 0. */
    ug_win->grant_lock_g_offset = 0;
#endif

    /* Calculate the window size of ghost 0, because it contains extra space
     * for sync. */
    root_g_size = CSP_GP_SHARED_SG_SIZE;
#ifdef CSP_ENABLE_GRANT_LOCK_HIDDEN_BYTE
    root_g_size = CSP_MAX(root_g_size, sizeof(CSP_GRANT_LOCK_DATATYPE));
#endif

    /* Calculate my offset on the local shared buffer.
     * Note that all the ghosts start the window from baseptr of ghost 0,
     * hence all the local ghosts use the same offset of user buffers.
     * My offset is the total window size of all ghosts and all users in front of
     * me on the node (loop world ranks to get its window size without rank translate).*/
    tmp_u_offsets = root_g_size + CSP_GP_SHARED_SG_SIZE * (CSP_ENV.num_g - 1);

    i = 0;
    while (i < user_rank) {
        if (ug_win->targets[i].node_id == ug_win->node_id) {
            tmp_u_offsets += ug_win->targets[i].size;   /* size in bytes */
        }
        i++;
    }

    for (j = 0; j < CSP_ENV.num_g; j++) {
        base_g_offsets[user_rank * CSP_ENV.num_g + j] = tmp_u_offsets;
    }

    CSP_DBG_PRINT("[%d] local base_g_offset 0x%lx\n", user_rank, tmp_u_offsets);

    /* -Receive the address of all the shared user buffers on Ghost processes. */
    CSP_CALLMPI(JUMP, PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, base_g_offsets,
                                     CSP_ENV.num_g, MPI_AINT, ug_win->user_comm));

    for (i = 0; i < user_nprocs; i++) {
        for (j = 0; j < CSP_ENV.num_g; j++) {
            ug_win->targets[i].base_g_offsets[j] = base_g_offsets[i * CSP_ENV.num_g + j];
            CSP_DBG_PRINT("\t.base_g_offsets[%d] = 0x%lx\n",
                          j, ug_win->targets[i].base_g_offsets[j]);
        }
    }

  fn_exit:
    if (base_g_offsets)
        free(base_g_offsets);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

/* Allocate shared window with local GHOSTs and USERs.
 * Return the size of whole shared memory buffer across processes, and
 * the shared window is stored as a CSPU_win_t struct member.*/
static int alloc_shared_window(MPI_Aint size, int disp_unit, MPI_Info info, CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Info shared_info = MPI_INFO_NULL;
    int user_rank;

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->user_comm, &user_rank));

    /* -Always set alloc_shm to true, same_size to false, noncontig to false
     *  for the shared internal window.
     *
     * We only pass user specified alloc_shm to win_create windows.
     * - If alloc_shm is true, MPI implementation can still provide shm optimization;
     * - If alloc_shm is false, those win_create windows are just handled as normal windows in MPI. */
    if (info != MPI_INFO_NULL) {
        CSP_CALLMPI(JUMP, PMPI_Info_dup(info, &shared_info));
        CSP_CALLMPI(JUMP, PMPI_Info_set(shared_info, "alloc_shm", "true"));
        CSP_CALLMPI(JUMP, PMPI_Info_set(shared_info, "same_size", "false"));
        CSP_CALLMPI(JUMP, PMPI_Info_set(shared_info, "alloc_shared_noncontig", "false"));
    }

    CSP_CALLMPI(JUMP, PMPI_Win_allocate_shared(size, disp_unit, shared_info, ug_win->local_ug_comm,
                                               &ug_win->base, &ug_win->local_ug_win));
    CSP_DBG_PRINT("[%d] allocate shared base = %p\n", user_rank, ug_win->base);

    /* Set RETURN error handler for all internal windows.
     * Thus any error happened on them will be returned and handled by the
     * first level in CASPER. */
    CSPU_WIN_ERRHAN_SET_INTERN(ug_win->local_ug_win);

    /* Gather user offsets on corresponding ghost processes */
    mpi_errno = gather_base_offsets(ug_win);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    /* Only release local variables. local_ug_win is released at the end of win_alloc. */
    if (shared_info && shared_info != MPI_INFO_NULL)
        CSP_CALLMPI_EXIT(PMPI_Info_free(&shared_info));
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int create_lock_windows(MPI_Aint size, int disp_unit, MPI_Info info, CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int i;
    int user_rank, user_nprocs;

    CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_win->user_comm, &user_nprocs));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->user_comm, &user_rank));

    ug_win->num_ug_wins = ug_win->max_local_user_nprocs;
    ug_win->ug_wins = CSP_calloc(ug_win->num_ug_wins, sizeof(MPI_Win));
    for (i = 0; i < ug_win->num_ug_wins; i++) {
        CSP_CALLMPI(JUMP, PMPI_Win_create(ug_win->base, size, disp_unit, info,
                                          ug_win->ug_comm, &ug_win->ug_wins[i]));

        /* Set RETURN error handler for all internal windows.
         * Thus any error happened on them will be returned and handled by the
         * first level in CASPER. */
        CSPU_WIN_ERRHAN_SET_INTERN(ug_win->ug_wins[i]);
    }

    for (i = 0; i < user_nprocs; i++) {
        int win_off;
        CSP_DBG_PRINT("[%d] targets[%d].\n", user_rank, i);

        /* unique window of each process, used in lock/flush */
        win_off = ug_win->targets[i].local_user_rank % ug_win->num_ug_wins;
        ug_win->targets[i].ug_win = ug_win->ug_wins[win_off];
        CSP_DBG_PRINT("\t\t .ug_win=0x%x (win_off %d)\n", ug_win->targets[i].ug_win, win_off);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int check_valid_input(MPI_Aint size, int disp_unit)
{
    int mpi_errno = MPI_SUCCESS;

    if (size < 0)
        mpi_errno = MPI_ERR_SIZE;
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    if (disp_unit <= 0)
        mpi_errno = MPI_ERR_DISP;
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

int MPI_Win_allocate(MPI_Aint size, int disp_unit, MPI_Info info,
                     MPI_Comm user_comm, void *baseptr, MPI_Win * win)
{
    int mpi_errno = MPI_SUCCESS;
    int ug_rank, ug_nprocs, user_nprocs, user_rank, user_world_rank, world_rank,
        user_local_rank, user_local_nprocs, ug_local_rank, ug_local_nprocs;
    CSPU_win_t *ug_win = NULL;
    int i;
    void **base_pp = (void **) baseptr;
    MPI_Aint *tmp_gather_buf = NULL;
    int tmp_bcast_buf[2];

    CSPU_ERRHAN_EXTOBJ_LOCAL_DCL();
    CSPU_COMM_ERRHAN_SET_EXTOBJ();

    ug_win = CSP_calloc(1, sizeof(CSPU_win_t));

    if (user_comm == MPI_COMM_WORLD)
        user_comm = CSP_COMM_USER_WORLD;
    ug_win->user_comm = user_comm;

    /* Read window configuration */
    mpi_errno = read_win_info(info, ug_win);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* If user turns off asynchronous redirection, simply return normal window; */
    if (ug_win->info_args.async_config == CSP_ASYNC_CONFIG_OFF) {
        CSPU_ERRHAN_RESET_EXTOBJ();     /* reset before calling original MPI */
        CSP_CALLMPI(NOSTMT, PMPI_Win_allocate(size, disp_unit, info, user_comm, baseptr, win));
        CSP_DBG_PRINT("User turns off async in win_allocate, return normal win 0x%x\n", *win);

        goto fn_noasync;
    }

    /* Start allocating casper window */

    /* Check any invalid input which can only be checked by MPI calls after ghost joined.
     * TODO: how to interrupt ghost win_allocate if MPI error reported on user side ?*/
    mpi_errno = check_valid_input(size, disp_unit);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* Initialize basic communicators and information. */
    if (user_comm == CSP_COMM_USER_WORLD) {
        /* Fast patch for comm_world. */
        ug_win->local_user_comm = CSP_PROC.user.u_local_comm;
        ug_win->user_root_comm = CSP_PROC.user.ur_comm;

        ug_win->node_id = CSP_PROC.node_id;
        ug_win->num_nodes = CSP_PROC.num_nodes;
        ug_win->user_comm = user_comm;
    }
    else {
        CSP_CALLMPI(JUMP, PMPI_Comm_split_type(user_comm, MPI_COMM_TYPE_SHARED, 0,
                                               MPI_INFO_NULL, &ug_win->local_user_comm));

        /* Create a user root communicator in order to figure out node_id and
         * num_nodes of this user communicator */
        CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->local_user_comm, &user_local_rank));
        CSP_CALLMPI(JUMP, PMPI_Comm_split(ug_win->user_comm,
                                          user_local_rank == 0, 1, &ug_win->user_root_comm));

        if (user_local_rank == 0) {
            int node_id, num_nodes;
            CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_win->user_root_comm, &num_nodes));
            CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->user_root_comm, &node_id));

            tmp_bcast_buf[0] = node_id;
            tmp_bcast_buf[1] = num_nodes;
        }

        CSP_CALLMPI(JUMP, PMPI_Bcast(tmp_bcast_buf, 2, MPI_INT, 0, ug_win->local_user_comm));
        ug_win->node_id = tmp_bcast_buf[0];
        ug_win->num_nodes = tmp_bcast_buf[1];
    }

    CSP_CALLMPI(JUMP, PMPI_Comm_group(user_comm, &ug_win->user_group));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(user_comm, &user_nprocs));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(user_comm, &user_rank));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_win->local_user_comm, &user_local_nprocs));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->local_user_comm, &user_local_rank));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(MPI_COMM_WORLD, &world_rank));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_COMM_USER_WORLD, &user_world_rank));

    ug_win->g_ranks_in_ug = CSP_calloc(CSP_ENV.num_g * ug_win->num_nodes, sizeof(MPI_Aint));
    ug_win->targets = CSP_calloc(user_nprocs, sizeof(CSPU_win_target_t));
    for (i = 0; i < user_nprocs; i++) {
        ug_win->targets[i].base_g_offsets = CSP_calloc(CSP_ENV.num_g, sizeof(MPI_Aint));
        ug_win->targets[i].g_ranks_in_ug = CSP_calloc(CSP_ENV.num_g, sizeof(MPI_Aint));
    }

    /* Gather users' disp_unit, size, ranks and node_id */
    tmp_gather_buf = CSP_calloc(user_nprocs * 7, sizeof(MPI_Aint));
    tmp_gather_buf[7 * user_rank] = (MPI_Aint) disp_unit;
    tmp_gather_buf[7 * user_rank + 1] = size;   /* MPI_Aint, size in bytes */
    tmp_gather_buf[7 * user_rank + 2] = (MPI_Aint) user_local_rank;
    tmp_gather_buf[7 * user_rank + 3] = (MPI_Aint) world_rank;
    tmp_gather_buf[7 * user_rank + 4] = (MPI_Aint) user_world_rank;
    tmp_gather_buf[7 * user_rank + 5] = (MPI_Aint) ug_win->node_id;
    tmp_gather_buf[7 * user_rank + 6] = (MPI_Aint) user_local_nprocs;

    CSP_CALLMPI(JUMP, PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                                     tmp_gather_buf, 7, MPI_AINT, user_comm));
    for (i = 0; i < user_nprocs; i++) {
        ug_win->targets[i].disp_unit = (int) tmp_gather_buf[7 * i];
        ug_win->targets[i].size = tmp_gather_buf[7 * i + 1];
        ug_win->targets[i].local_user_rank = (int) tmp_gather_buf[7 * i + 2];
        ug_win->targets[i].world_rank = (int) tmp_gather_buf[7 * i + 3];
        ug_win->targets[i].user_world_rank = (int) tmp_gather_buf[7 * i + 4];
        ug_win->targets[i].node_id = (int) tmp_gather_buf[7 * i + 5];
        ug_win->targets[i].local_user_nprocs = (int) tmp_gather_buf[7 * i + 6];

        /* Calculate the maximum number of processes per node */
        ug_win->max_local_user_nprocs = CSP_MAX(ug_win->max_local_user_nprocs,
                                                ug_win->targets[i].local_user_nprocs);
    }

#ifdef CSP_DEBUG
    CSP_DBG_PRINT("my user local rank %d/%d, max_local_user_nprocs=%d, num_nodes=%d\n",
                  user_local_rank, user_local_nprocs, ug_win->max_local_user_nprocs,
                  ug_win->num_nodes);
    for (i = 0; i < user_nprocs; i++) {
        CSP_DBG_PRINT("\t targets[%d].disp_unit=%d, size=%ld, local_user_rank=%d, "
                      "world_rank=%d, user_world_rank=%d, node_id=%d, local_user_nprocs=%d\n",
                      i, ug_win->targets[i].disp_unit, ug_win->targets[i].size,
                      ug_win->targets[i].local_user_rank, ug_win->targets[i].world_rank,
                      ug_win->targets[i].user_world_rank, ug_win->targets[i].node_id,
                      ug_win->targets[i].local_user_nprocs);
    }
#endif

    if (user_local_rank == 0) {
        mpi_errno = issue_ghost_cmd(user_nprocs, info, ug_win);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

#if defined(CSP_ENABLE_THREAD_SAFE)
    /* Creating ug_comm has to be based on MPI_COMM_WORLD, which is unsafe in
     * threaded code. I.e., t0 and t1 are both waiting on comm_create_group(COMM_WORLD),
     * thus t1 might mismatch the collective call with t0 on another user process.
     * This barrier ensures only the correct set of threads can concurrently wait
     * on that call.*/
    CSP_CALLMPI(JUMP, PMPI_Barrier(ug_win->user_comm));
#endif

    /* Create communicators
     *  ug_comm: including all USER and Ghost processes
     *  local_ug_comm: including local USER and Ghost processes
     */
    mpi_errno = create_communicators(ug_win);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->local_ug_comm, &ug_local_rank));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_win->local_ug_comm, &ug_local_nprocs));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_win->ug_comm, &ug_nprocs));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->ug_comm, &ug_rank));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->ug_comm, &ug_win->my_rank_in_ug_comm));
    CSP_DBG_PRINT(" Created ug_comm: %d/%d, local_ug_comm: %d/%d\n",
                  ug_rank, ug_nprocs, ug_local_rank, ug_local_nprocs);

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    ug_win->g_ops_counts = CSP_calloc(ug_nprocs, sizeof(int));
    ug_win->g_bytes_counts = CSP_calloc(ug_nprocs, sizeof(unsigned long));
#endif

    /* Allocate local shared window */
    mpi_errno = alloc_shared_window(size, disp_unit, info, ug_win);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* Bind window to main ghost process */
    mpi_errno = CSPU_win_bind_ghosts(ug_win);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* Create N-windows for lock */
    if (ug_win->info_args.epochs_used & CSP_EPOCH_LOCK) {
        mpi_errno = create_lock_windows(size, disp_unit, info, ug_win);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

    /* Create global window when fence|pscw are specified,
     * or only lock_all is specified.*/
    if ((ug_win->info_args.epochs_used & CSP_EPOCH_FENCE) ||
        (ug_win->info_args.epochs_used & CSP_EPOCH_PSCW) ||
        (ug_win->info_args.epochs_used == CSP_EPOCH_LOCK_ALL)) {
        CSP_CALLMPI(JUMP, PMPI_Win_create(ug_win->base, size, disp_unit, info,
                                          ug_win->ug_comm, &ug_win->global_win));

        /* Set RETURN error handler for all internal windows.
         * Thus any error happened on them will be returned and handled by the
         * first level in CASPER. */
        CSPU_WIN_ERRHAN_SET_INTERN(ug_win->global_win);

        CSP_DBG_PRINT("[%d] Created global window 0x%x\n", user_rank, ug_win->global_win);

        /* Since all processes must be in win_allocate, we do not need worry
         * the possibility losing asynchronous progress.
         * This lock_all guarantees the semantics correctness when internally
         * change to passive mode. */
        CSP_CALLMPI(JUMP, PMPI_Win_lock_all(MPI_MODE_NOCHECK, ug_win->global_win));
    }

    /* Track epoch status for redirecting RMA to different window. */
    ug_win->epoch_stat = CSPU_WIN_NO_EPOCH;
    for (i = 0; i < user_nprocs; i++)
        ug_win->targets->epoch_stat = CSPU_TARGET_NO_EPOCH;

    ug_win->start_counter = 0;
    ug_win->lock_counter = 0;
    ug_win->is_self_locked = 0;

    /* - Only expose user window in order to hide ghosts in all non-wrapped window functions */
    CSP_CALLMPI(JUMP, PMPI_Win_create(ug_win->base, size, disp_unit, info,
                                      ug_win->user_comm, &ug_win->win));

    CSP_DBG_PRINT("[%d] Created window 0x%x\n", user_rank, ug_win->win);

    ug_win->create_flavor = MPI_WIN_FLAVOR_ALLOCATE;
    *win = ug_win->win;
    *base_pp = ug_win->base;

    /* Gather the handle of ghosts' win. */
    if (user_local_rank == 0) {
        ug_win->g_win_handles = CSP_calloc(CSP_ENV.num_g, sizeof(unsigned long));
        mpi_errno = gather_ghost_cmd_params(ug_win->g_win_handles, sizeof(unsigned long));
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

    /* Initialize per window critical section. */
    CSPU_THREAD_INIT_OBJ_CS(ug_win);

    mpi_errno = CSPU_cache_ug_win(ug_win->win, ug_win);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    if (tmp_gather_buf)
        free(tmp_gather_buf);

    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before return */
    return mpi_errno;

  fn_noasync:
    CSPU_win_release(ug_win);
    goto fn_exit;

  fn_fail:

    /* Caching is the last possible error, so we do not need remove
     * cache here. */

    CSPU_win_release(ug_win);

    *win = MPI_WIN_NULL;
    *base_pp = NULL;

    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before error handling */
    CSPU_COMM_ERRHANLDING(user_comm, &mpi_errno);
    goto fn_exit;
}

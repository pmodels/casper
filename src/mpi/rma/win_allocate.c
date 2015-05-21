#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include "csp.h"

#include <ctype.h>

const char *CSP_Win_epoch_stat_name[4] = {
    "NO_EPOCH",
    "FENCE",
    "LOCK",
    "PSCW"
};


static int read_win_info(MPI_Info info, CSP_Win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;

    ug_win->info_args.no_local_load_store = 0;
    ug_win->info_args.epoch_type = CSP_EPOCH_LOCK_ALL | CSP_EPOCH_LOCK |
        CSP_EPOCH_PSCW | CSP_EPOCH_FENCE;

    if (info != MPI_INFO_NULL) {
        int info_flag = 0;
        char info_value[MPI_MAX_INFO_VAL + 1];

        /* Check if we are allowed to ignore force-lock for local target,
         * require force-lock by default. */
        memset(info_value, 0, sizeof(info_value));
        mpi_errno = PMPI_Info_get(info, "no_local_load_store", MPI_MAX_INFO_VAL,
                                  info_value, &info_flag);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        if (info_flag == 1) {
            if (!strncmp(info_value, "true", strlen("true")))
                ug_win->info_args.no_local_load_store = 1;
        }

        /* Check if user specifies epoch types */
        memset(info_value, 0, sizeof(info_value));
        mpi_errno = PMPI_Info_get(info, "epoch_type", MPI_MAX_INFO_VAL, info_value, &info_flag);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        if (info_flag == 1) {
            int user_epoch_type = 0;
            char *type = NULL;

            type = strtok(info_value, "|");
            while (type != NULL) {
                if (!strncmp(type, "lockall", strlen("lockall"))) {
                    user_epoch_type |= CSP_EPOCH_LOCK_ALL;
                }
                else if (!strncmp(type, "lock", strlen("lock"))) {
                    user_epoch_type |= CSP_EPOCH_LOCK;
                }
                else if (!strncmp(type, "pscw", strlen("pscw"))) {
                    user_epoch_type |= CSP_EPOCH_PSCW;
                }
                else if (!strncmp(type, "fence", strlen("fence"))) {
                    user_epoch_type |= CSP_EPOCH_FENCE;
                }
                type = strtok(NULL, "|");
            }

            if (user_epoch_type != 0)
                ug_win->info_args.epoch_type = user_epoch_type;
        }
    }

    CSP_DBG_PRINT("no_local_load_store %d, epoch_type=%s|%s|%s|%s\n",
                     ug_win->info_args.no_local_load_store,
                     ((ug_win->info_args.epoch_type & CSP_EPOCH_LOCK_ALL) ? "lockall" : ""),
                     ((ug_win->info_args.epoch_type & CSP_EPOCH_LOCK) ? "lock" : ""),
                     ((ug_win->info_args.epoch_type & CSP_EPOCH_PSCW) ? "pscw" : ""),
                     ((ug_win->info_args.epoch_type & CSP_EPOCH_FENCE) ? "fence" : "")
);

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int gather_ranks(CSP_Win * win, int *num_ghosts, int *gp_ranks_in_world,
                        int *unique_gp_ranks_in_world)
{
    int mpi_errno = MPI_SUCCESS;
    int user_nprocs;
    int *gp_bitmap = NULL;
    int user_world_rank, tmp_num_ghosts;
    int i, j, gp_rank;
    int world_nprocs;

    PMPI_Comm_size(MPI_COMM_WORLD, &world_nprocs);
    PMPI_Comm_size(win->user_comm, &user_nprocs);

    gp_bitmap = calloc(world_nprocs, sizeof(int));
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
            gp_rank = CSP_ALL_G_RANKS_IN_WORLD[user_world_rank * CSP_ENV.num_g + j];
            gp_ranks_in_world[i * CSP_ENV.num_g + j] = gp_rank;

            /* Unique ghost ranks */
            if (!gp_bitmap[gp_rank]) {
                unique_gp_ranks_in_world[tmp_num_ghosts++] = gp_rank;
                gp_bitmap[gp_rank] = 1;

                CSP_Assert(tmp_num_ghosts <= CSP_NUM_NODES * CSP_ENV.num_g);
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

static void specify_main_gp_binding_by_segments(int n_targets, int *local_targets,
                                                    CSP_Win * ug_win)
{
    MPI_Aint seg_size, t_size, sum_size, size_per_ghost, assigned_size, max_t_size;
    int t_num_segs, t_last_g_off, max_t_num_seg;
    int i, j, g_off, t_rank, user_nprocs;
    MPI_Aint *t_seg_sizes = NULL;

    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);

    /* Calculate size binded to each ghost */
    max_t_size = 0;
    sum_size = 0;
    for (i = 0; i < n_targets; i++) {
        t_rank = local_targets[i];
        CSP_Assert(t_rank < user_nprocs);

        sum_size += ug_win->targets[t_rank].size;
        max_t_size = max(max_t_size, ug_win->targets[t_rank].size);
    }
    /* Never divide less than segment unit */
    size_per_ghost = align(sum_size / CSP_ENV.num_g, CSP_SEGMENT_UNIT);
    max_t_num_seg = sum_size / size_per_ghost + 3;
    t_seg_sizes = calloc(max_t_num_seg, sizeof(MPI_Aint));

    /* Divide segments based on sizes */
    i = 0;
    t_rank = local_targets[0];
    g_off = 0;
    t_last_g_off = 0;
    t_size = ug_win->targets[0].size;
    t_num_segs = 0;
    assigned_size = 0;
    seg_size = size_per_ghost;
    while (assigned_size < sum_size && i < n_targets) {
        if (t_size == 0) {
            ug_win->targets[t_rank].num_segs = t_num_segs;
            ug_win->targets[t_rank].segs = calloc(t_num_segs, sizeof(CSP_Win_target_seg));

            MPI_Aint prev_seg_base = 0, prev_seg_size = 0;
            for (j = 0; j < t_num_segs; j++) {
                ug_win->targets[t_rank].segs[j].base_offset = prev_seg_base + prev_seg_size;
                ug_win->targets[t_rank].segs[j].size = t_seg_sizes[j];
                ug_win->targets[t_rank].segs[j].main_g_off = t_last_g_off + 1 - t_num_segs + j;

                CSP_Assert(ug_win->targets[t_rank].segs[j].main_g_off < CSP_ENV.num_g);

                prev_seg_base = ug_win->targets[t_rank].segs[j].base_offset;
                prev_seg_size = ug_win->targets[t_rank].segs[j].size;
            }

            assigned_size += ug_win->targets[t_rank].size;

            /* next target */
            if (i >= n_targets) {
                break;
            }
            t_rank = local_targets[++i];
            t_size = ug_win->targets[t_rank].size;
            t_num_segs = 0;
        }
        /* finish this target if remaining size is small than seg_size or
         * it is already at the last ghost. */
        else if (t_size < seg_size || g_off == CSP_ENV.num_g - 1) {
            CSP_Assert(t_num_segs < max_t_num_seg);

            t_seg_sizes[t_num_segs++] = t_size;
            seg_size -= t_size;
            /* make sure remaining segment size is aligned */
            seg_size = align(seg_size, CSP_SEGMENT_UNIT);

            t_size = 0;

            t_last_g_off = g_off;
        }
        else if (t_size >= seg_size) {
            CSP_Assert(t_num_segs < max_t_num_seg);

            /* divide large target */
            t_seg_sizes[t_num_segs++] = seg_size;
            t_size -= seg_size;
            seg_size = 0;

            t_last_g_off = g_off;
        }

        /* next ghost */
        if (seg_size == 0) {
            g_off++;
            CSP_Assert(g_off <= CSP_ENV.num_g);
            seg_size = size_per_ghost
                + (g_off == CSP_ENV.num_g - 1 ? (sum_size % CSP_ENV.num_g) : 0);
            /* make sure new segment size is aligned */
            seg_size = align(seg_size, CSP_SEGMENT_UNIT);
        }
    }

    CSP_Assert(assigned_size == sum_size);

    if (t_seg_sizes)
        free(t_seg_sizes);
}

static void specify_main_gp_binding_by_ranks(int n_targets, int *local_targets,
                                                 CSP_Win * ug_win)
{
    int i, g_off, t_rank, user_nprocs;
    int np_per_ghost;

    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);

    np_per_ghost = n_targets / CSP_ENV.num_g;

    int np = np_per_ghost;
    i = 0;
    g_off = 0;
    t_rank = local_targets[i];

    while (i < n_targets) {
        if (np == 0) {
            /* next ghost */
            g_off++;
            np = np_per_ghost +
                ((g_off == CSP_ENV.num_g - 1) ? (n_targets % CSP_ENV.num_g) : 0);
        }
        CSP_Assert(g_off <= CSP_ENV.num_g);

        t_rank = local_targets[i];
        ug_win->targets[t_rank].num_segs = 1;
        ug_win->targets[t_rank].segs = calloc(1, sizeof(CSP_Win_target_seg));
        ug_win->targets[t_rank].segs[0].base_offset = 0;
        ug_win->targets[t_rank].segs[0].size = ug_win->targets[i].size;
        ug_win->targets[t_rank].segs[0].main_g_off = g_off;

        /* next target */
        i++;
        np--;
    }
}

static void specify_main_gp_binding(CSP_Win * ug_win)
{
    int i, j, g_off, user_nprocs;
    int *local_targets = NULL;

    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);
    local_targets = calloc(ug_win->num_nodes * ug_win->max_local_user_nprocs, sizeof(int));

    /* Sort targets by node_ids */
    for (i = 0; i < user_nprocs; i++) {
        int off = ug_win->targets[i].node_id * ug_win->max_local_user_nprocs +
            ug_win->targets[i].local_user_rank;
        local_targets[off] = i;
    }

    /* Specify main ghosts on each node */
    if (CSP_ENV.lock_binding == CSP_LOCK_BINDING_SEGMENT) {
        for (i = 0; i < ug_win->num_nodes; i++) {
            int s_off = i * ug_win->max_local_user_nprocs;
            int s_rank = local_targets[s_off];
            int n_targets = ug_win->targets[s_rank].local_user_nprocs;

            specify_main_gp_binding_by_segments(n_targets, &local_targets[s_off], ug_win);
        }
    }
    else {
        for (i = 0; i < ug_win->num_nodes; i++) {
            int s_off = i * ug_win->max_local_user_nprocs;
            int s_rank = local_targets[s_off];
            int n_targets = ug_win->targets[s_rank].local_user_nprocs;

            specify_main_gp_binding_by_ranks(n_targets, &local_targets[s_off], ug_win);
        }
    }

#ifdef DEBUG
    for (i = 0; i < user_nprocs; i++) {
        CSP_DBG_PRINT("\t target[%d] .num_segs %d\n", i, ug_win->targets[i].num_segs);
        for (j = 0; j < CSP_ENV.num_g; j++) {
            CSP_DBG_PRINT("\t\t .g_rank[%d] %d, offset[%d] 0x%lx \n",
                             j, ug_win->targets[i].g_ranks_in_ug[j],
                             j, ug_win->targets[i].base_g_offsets[j]);
        }
        for (j = 0; j < ug_win->targets[i].num_segs; j++) {
            CSP_DBG_PRINT("\t\t .seg[%d].main_g_off=%d, base_offset=0x%lx, size=0x%x\n",
                             j, ug_win->targets[i].segs[j].main_g_off,
                             ug_win->targets[i].segs[j].base_offset,
                             ug_win->targets[i].segs[j].size);
        }
    }
#endif

    if (local_targets)
        free(local_targets);
}

static int create_ug_comm(int num_ghosts, int *gp_ranks_in_world, CSP_Win * win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_nprocs, user_rank, world_nprocs, tmp_world_rank;
    int *ug_ranks_in_world = NULL, *g_info = NULL;
    int i, j, num_ug_ranks, g_rank;

    PMPI_Comm_size(win->user_comm, &user_nprocs);
    PMPI_Comm_rank(win->user_comm, &user_rank);
    PMPI_Comm_size(MPI_COMM_WORLD, &world_nprocs);

    /* maximum amount equals to world size */
    ug_ranks_in_world = calloc(world_nprocs, sizeof(int));
    if (ug_ranks_in_world == NULL)
        goto fn_fail;

    /* -Create ug communicator including all USER processes and Ghost processes. */
    num_ug_ranks = num_ghosts;
    memcpy(ug_ranks_in_world, gp_ranks_in_world, num_ghosts * sizeof(int));
    for (i = 0; i < user_nprocs; i++) {
        ug_ranks_in_world[num_ug_ranks++] = win->targets[i].world_rank;
    }
    if (num_ug_ranks > world_nprocs) {
        fprintf(stderr, "num_ug_ranks %d > world_nprocs %d, num_ghosts=%d, user_nprocs=%d\n",
                num_ug_ranks, world_nprocs, num_ghosts, user_nprocs);
    }
    CSP_Assert(num_ug_ranks <= world_nprocs);

    PMPI_Group_incl(CSP_GROUP_WORLD, num_ug_ranks, ug_ranks_in_world, &win->ug_group);
    mpi_errno = PMPI_Comm_create_group(MPI_COMM_WORLD, win->ug_group, 0, &win->ug_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    if (ug_ranks_in_world)
        free(ug_ranks_in_world);

    return mpi_errno;

  fn_fail:
    if (win->ug_comm != MPI_COMM_NULL) {
        PMPI_Comm_free(&win->ug_comm);
        win->ug_comm = MPI_COMM_NULL;
    }
    if (win->ug_group != MPI_GROUP_NULL) {
        PMPI_Group_free(&win->ug_group);
        win->ug_group = MPI_GROUP_NULL;
    }

    goto fn_exit;
}

static int create_communicators(CSP_Win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int *func_params = NULL, func_param_size = 0;
    int *user_ranks_in_world = NULL;
    int *gp_ranks_in_world = NULL;
    int num_ghosts = 0, max_num_ghosts;
    int user_nprocs, user_local_rank;
    int *gp_ranks_in_ug = NULL;
    int i;

    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);
    max_num_ghosts = CSP_ENV.num_g * CSP_NUM_NODES;
    func_param_size = user_nprocs + max_num_ghosts + 2;

    PMPI_Comm_rank(ug_win->local_user_comm, &user_local_rank);
    ug_win->num_g_ranks_in_ug = CSP_ENV.num_g * ug_win->num_nodes;

    /* Optimization for user world communicator */
    if (ug_win->user_comm == CSP_COMM_USER_WORLD) {
        if (user_local_rank == 0) {
            func_params = calloc(func_param_size, sizeof(int));

            /* Set parameters to local Ghosts
             *  [0]: is_comm_user_world
             */
            func_params[0] = 1;

            mpi_errno = CSP_Func_set_param((char *) func_params, sizeof(int) * func_param_size,
                                              ug_win->ur_g_comm);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        /* Create communicators
         *  local_ug_comm: including local USER and Ghost processes
         *  ug_comm: including all USER and Ghost processes
         */
        ug_win->local_ug_comm = CSP_COMM_LOCAL;
        ug_win->ug_comm = MPI_COMM_WORLD;
        PMPI_Comm_group(ug_win->local_ug_comm, &ug_win->local_ug_group);
        PMPI_Comm_group(ug_win->ug_comm, &ug_win->ug_group);

        /* -Get all Ghost rank in ug communicator */
        memcpy(ug_win->g_ranks_in_ug, CSP_ALL_UNIQUE_G_RANKS_IN_WORLD,
               ug_win->num_g_ranks_in_ug * sizeof(int));
        for (i = 0; i < user_nprocs; i++)
            memcpy(ug_win->targets[i].g_ranks_in_ug,
                   &CSP_ALL_G_RANKS_IN_WORLD[i * CSP_ENV.num_g],
                   sizeof(int) * CSP_ENV.num_g);
    }
    else {
        /* ghost ranks for every user process, used for ghost fetching in epoch */
        gp_ranks_in_world = calloc(CSP_ENV.num_g * user_nprocs, sizeof(int));
        gp_ranks_in_ug = calloc(CSP_ENV.num_g * user_nprocs, sizeof(int));

        /* Gather user rank information */
        mpi_errno = gather_ranks(ug_win, &num_ghosts, gp_ranks_in_world,
                                 ug_win->g_ranks_in_ug);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        if (user_local_rank == 0) {

            user_ranks_in_world = calloc(user_nprocs, sizeof(int));
            for (i = 0; i < user_nprocs; i++) {
                user_ranks_in_world[i] = ug_win->targets[i].world_rank;
            }

            /* Set parameters to local Ghosts
             *  [0]: is_comm_user_world
             *  [1]: num_ghosts
             *  [2:N+1]: user ranks in comm_world
             *  [N+2:]: ghost ranks in comm_world
             */
            int pidx;
            func_params = calloc(func_param_size, sizeof(int));

            func_params[0] = 0;
            func_params[1] = num_ghosts;
            pidx = 2;
            memcpy(&func_params[pidx], user_ranks_in_world, user_nprocs * sizeof(int));
            pidx += user_nprocs;
            memcpy(&func_params[pidx], ug_win->g_ranks_in_ug, num_ghosts * sizeof(int));
            mpi_errno = CSP_Func_set_param((char *) func_params, sizeof(int) * func_param_size,
                                              ug_win->ur_g_comm);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        /* Create communicators
         *  ug_comm: including all USER and Ghost processes
         *  local_ug_comm: including local USER and Ghost processes
         */
        mpi_errno = create_ug_comm(num_ghosts, ug_win->g_ranks_in_ug, ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

#ifdef DEBUG
        {
            int ug_rank, ug_nprocs;
            PMPI_Comm_rank(ug_win->ug_comm, &ug_rank);
            PMPI_Comm_size(ug_win->ug_comm, &ug_nprocs);
            CSP_DBG_PRINT("created ug_comm, my rank %d/%d\n", ug_rank, ug_nprocs);
        }
#endif

        mpi_errno = PMPI_Comm_split_type(ug_win->ug_comm, MPI_COMM_TYPE_SHARED, 0,
                                         MPI_INFO_NULL, &ug_win->local_ug_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

#ifdef DEBUG
        {
            int ug_rank, ug_nprocs;
            PMPI_Comm_rank(ug_win->local_ug_comm, &ug_rank);
            PMPI_Comm_size(ug_win->local_ug_comm, &ug_nprocs);
            CSP_DBG_PRINT("created local_ug_comm, my rank %d/%d\n", ug_rank, ug_nprocs);
        }
#endif

        /* Get all Ghost rank in ug communicator */
        mpi_errno = PMPI_Group_translate_ranks(CSP_GROUP_WORLD, user_nprocs * CSP_ENV.num_g,
                                               gp_ranks_in_world, ug_win->ug_group,
                                               gp_ranks_in_ug);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        PMPI_Comm_group(ug_win->local_ug_comm, &ug_win->local_ug_group);

        for (i = 0; i < user_nprocs; i++)
            memcpy(ug_win->targets[i].g_ranks_in_ug, &gp_ranks_in_ug[i * CSP_ENV.num_g],
                   sizeof(int) * CSP_ENV.num_g);
    }

#ifdef DEBUG
    {
        CSP_DBG_PRINT("%d unique g_ranks:\n", ug_win->num_g_ranks_in_ug);
        for (i = 0; i < ug_win->num_g_ranks_in_ug; i++) {
            CSP_DBG_PRINT("\t[%d] %d\n", i, ug_win->g_ranks_in_ug[i]);
        }
    }
#endif

  fn_exit:
    if (func_params)
        free(func_params);
    if (user_ranks_in_world)
        free(user_ranks_in_world);
    if (gp_ranks_in_world)
        free(gp_ranks_in_world);
    if (gp_ranks_in_ug)
        free(gp_ranks_in_ug);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int gather_base_offsets(MPI_Aint size, CSP_Win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint tmp_u_offsets;
    int i, j;
    int user_local_rank, user_local_nprocs, user_rank, user_nprocs;
    MPI_Aint *base_g_offsets;
    void *base_ptr = NULL;

    PMPI_Comm_rank(ug_win->local_user_comm, &user_local_rank);
    PMPI_Comm_size(ug_win->local_user_comm, &user_local_nprocs);
    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);

    base_g_offsets = calloc(user_nprocs * CSP_ENV.num_g, sizeof(MPI_Aint));

#ifdef CSP_ENABLE_GRANT_LOCK_HIDDEN_BYTE
    /* All the ghosts use the byte located on ghost 0. */
    ug_win->grant_lock_g_offset = 0;
#endif

    /* -Calculate the offset of local shared buffer and wait_counter.
     * All wait_counters are on ghost 0.  */
    i = 0;
    tmp_u_offsets = 0;
    while (i < user_rank) {
        if (ug_win->targets[i].node_id == ug_win->node_id) {
            tmp_u_offsets += ug_win->targets[i].size;   /* size in bytes */
        }
        i++;
    }

    /* Note that all the ghosts start the window from baseptr of ghost 0.
     * Hence all the local ghosts use the same offset of user buffers */

    /* Calculate the window size of ghost 0, because it contains extra space
     * for sync. */
    int root_g_size = CSP_GP_SHARED_SG_SIZE;
#ifdef CSP_ENABLE_GRANT_LOCK_HIDDEN_BYTE
    root_g_size = max(root_g_size, sizeof(CSP_GRANT_LOCK_DATATYPE));
#endif

    tmp_u_offsets += root_g_size;
    tmp_u_offsets += CSP_GP_SHARED_SG_SIZE * (CSP_ENV.num_g - 1);

    for (j = 0; j < CSP_ENV.num_g; j++) {
        base_g_offsets[user_rank * CSP_ENV.num_g + j] = tmp_u_offsets;
    }

    CSP_DBG_PRINT("[%d] local base_g_offset 0x%lx\n", user_rank, tmp_u_offsets);

    /* -Receive the address of all the shared user buffers on Ghost processes. */
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, base_g_offsets,
                               CSP_ENV.num_g, MPI_AINT, ug_win->user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    for (i = 0; i < user_nprocs; i++) {
        for (j = 0; j < CSP_ENV.num_g; j++) {
            ug_win->targets[i].base_g_offsets[j] = base_g_offsets[i * CSP_ENV.num_g + j];
            CSP_DBG_PRINT("\t.base_g_offsets[%d] = 0x%lx/0x%lx\n",
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

static int create_lock_windows(MPI_Aint size, int disp_unit, MPI_Info info, CSP_Win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int i, j;
    int user_rank, user_nprocs;

    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);
    PMPI_Comm_rank(ug_win->user_comm, &user_rank);

    /* Need multiple windows for single lock synchronization */
    if (ug_win->info_args.epoch_type & CSP_EPOCH_LOCK) {
        ug_win->num_ug_wins = ug_win->max_local_user_nprocs;
    }
    /* Need a single window for lock_all only synchronization */
    else if (ug_win->info_args.epoch_type & CSP_EPOCH_LOCK_ALL) {
        ug_win->num_ug_wins = 1;
    }

    ug_win->ug_wins = calloc(ug_win->num_ug_wins, sizeof(MPI_Win));
    for (i = 0; i < ug_win->num_ug_wins; i++) {
        mpi_errno = PMPI_Win_create(ug_win->base, size, disp_unit, info,
                                    ug_win->ug_comm, &ug_win->ug_wins[i]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    for (i = 0; i < user_nprocs; i++) {
        int win_off;
        CSP_DBG_PRINT("[%d] targets[%d].\n", user_rank, i);

        /* unique windows of each process, used in lock/flush */
        win_off = ug_win->targets[i].local_user_rank % ug_win->num_ug_wins;
        ug_win->targets[i].ug_win = ug_win->ug_wins[win_off];
        CSP_DBG_PRINT("\t\t .ug_win=0x%x (win_off %d)\n", ug_win->targets[i].ug_win, win_off);

        /* windows of each segment, used in OPs */
        for (j = 0; j < ug_win->targets[i].num_segs; j++) {
            win_off = ug_win->targets[i].local_user_rank % ug_win->num_ug_wins;
            ug_win->targets[i].segs[j].ug_win = ug_win->ug_wins[win_off];

            CSP_DBG_PRINT("\t\t .seg[%d].ug_win=0x%x (win_off %d)\n",
                             j, ug_win->targets[i].segs[j].ug_win, win_off);
        }
    }

    /* Setup window for local target */
    ug_win->my_ug_win = ug_win->targets[user_rank].segs[0].ug_win;

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_allocate(MPI_Aint size, int disp_unit, MPI_Info info,
                     MPI_Comm user_comm, void *baseptr, MPI_Win * win)
{
    static const char FCNAME[] = "MPI_Win_allocate";
    int mpi_errno = MPI_SUCCESS;
    MPI_Group ug_group;
    int ug_rank, ug_nprocs, user_nprocs, user_rank, user_world_rank, world_rank,
        user_local_rank, user_local_nprocs, ug_local_rank, ug_local_nprocs;
    CSP_Win *ug_win;
    int i, j;
    void **base_pp = (void **) baseptr;
    MPI_Status stat;
    MPI_Aint *tmp_gather_buf = NULL;
    ;
    int tmp_bcast_buf[2];

    CSP_DBG_PRINT_FCNAME();

    ug_win = calloc(1, sizeof(CSP_Win));

    /* If user specifies comm_world directly, use user comm_world instead;
     * else this communicator directly, because it should be created from user comm_world */
    if (user_comm == MPI_COMM_WORLD) {
        user_comm = CSP_COMM_USER_WORLD;
        ug_win->local_user_comm = CSP_COMM_USER_LOCAL;
        ug_win->user_root_comm = CSP_COMM_UR_WORLD;

        ug_win->node_id = CSP_MY_NODE_ID;
        ug_win->num_nodes = CSP_NUM_NODES;
        ug_win->user_comm = user_comm;
    }
    else {
        mpi_errno = PMPI_Comm_split_type(user_comm, MPI_COMM_TYPE_SHARED, 0,
                                         MPI_INFO_NULL, &ug_win->local_user_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        ug_win->user_comm = user_comm;

        /* Create a user root communicator in order to figure out node_id and
         * num_nodes of this user communicator */
        PMPI_Comm_rank(ug_win->local_user_comm, &user_local_rank);
        mpi_errno = PMPI_Comm_split(ug_win->user_comm,
                                    user_local_rank == 0, 1, &ug_win->user_root_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        if (user_local_rank == 0) {
            int node_id, num_nodes;
            PMPI_Comm_size(ug_win->user_root_comm, &num_nodes);
            PMPI_Comm_rank(ug_win->user_root_comm, &node_id);

            tmp_bcast_buf[0] = node_id;
            tmp_bcast_buf[1] = num_nodes;
        }

        PMPI_Bcast(tmp_bcast_buf, 2, MPI_INT, 0, ug_win->local_user_comm);
        ug_win->node_id = tmp_bcast_buf[0];
        ug_win->num_nodes = tmp_bcast_buf[1];
    }

    PMPI_Comm_group(user_comm, &ug_win->user_group);
    PMPI_Comm_size(user_comm, &user_nprocs);
    PMPI_Comm_rank(user_comm, &user_rank);
    PMPI_Comm_size(ug_win->local_user_comm, &user_local_nprocs);
    PMPI_Comm_rank(ug_win->local_user_comm, &user_local_rank);
    PMPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    PMPI_Comm_rank(CSP_COMM_USER_WORLD, &user_world_rank);

    ug_win->g_ranks_in_ug = calloc(CSP_ENV.num_g * ug_win->num_nodes, sizeof(MPI_Aint));
    ug_win->targets = calloc(user_nprocs, sizeof(CSP_Win_target));
    for (i = 0; i < user_nprocs; i++) {
        ug_win->targets[i].base_g_offsets = calloc(CSP_ENV.num_g, sizeof(MPI_Aint));
        ug_win->targets[i].g_ranks_in_ug = calloc(CSP_ENV.num_g, sizeof(MPI_Aint));
    }

    /* Gather users' disp_unit, size, ranks and node_id */
    tmp_gather_buf = calloc(user_nprocs * 7, sizeof(MPI_Aint));
    tmp_gather_buf[7 * user_rank] = (MPI_Aint) disp_unit;
    tmp_gather_buf[7 * user_rank + 1] = size;   /* MPI_Aint, size in bytes */
    tmp_gather_buf[7 * user_rank + 2] = (MPI_Aint) user_local_rank;
    tmp_gather_buf[7 * user_rank + 3] = (MPI_Aint) world_rank;
    tmp_gather_buf[7 * user_rank + 4] = (MPI_Aint) user_world_rank;
    tmp_gather_buf[7 * user_rank + 5] = (MPI_Aint) ug_win->node_id;
    tmp_gather_buf[7 * user_rank + 6] = (MPI_Aint) user_local_nprocs;

    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                               tmp_gather_buf, 7, MPI_AINT, user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    for (i = 0; i < user_nprocs; i++) {
        ug_win->targets[i].disp_unit = (int) tmp_gather_buf[7 * i];
        ug_win->targets[i].size = tmp_gather_buf[7 * i + 1];
        ug_win->targets[i].local_user_rank = (int) tmp_gather_buf[7 * i + 2];
        ug_win->targets[i].world_rank = (int) tmp_gather_buf[7 * i + 3];
        ug_win->targets[i].user_world_rank = (int) tmp_gather_buf[7 * i + 4];
        ug_win->targets[i].node_id = (int) tmp_gather_buf[7 * i + 5];
        ug_win->targets[i].local_user_nprocs = (int) tmp_gather_buf[7 * i + 6];

        /* Calculate the maximum number of processes per node */
        ug_win->max_local_user_nprocs = max(ug_win->max_local_user_nprocs,
                                            ug_win->targets[i].local_user_nprocs);
    }

#ifdef DEBUG
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

    mpi_errno = read_win_info(info, ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Notify Ghosts start and create user root + ghosts communicator for
     * internal information exchange between users and ghosts. */
    if (user_local_rank == 0) {
        mpi_errno = CSP_Func_start(CSP_FUNC_WIN_ALLOCATE, user_nprocs, user_local_nprocs);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        mpi_errno = CSP_Func_new_ur_g_comm(&ug_win->ur_g_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* Create communicators
     *  ug_comm: including all USER and Ghost processes
     *  local_ug_comm: including local USER and Ghost processes
     */
    mpi_errno = create_communicators(ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    PMPI_Comm_rank(ug_win->local_ug_comm, &ug_local_rank);
    PMPI_Comm_size(ug_win->local_ug_comm, &ug_local_nprocs);
    PMPI_Comm_size(ug_win->ug_comm, &ug_nprocs);
    PMPI_Comm_rank(ug_win->ug_comm, &ug_rank);
    PMPI_Comm_rank(ug_win->ug_comm, &ug_win->my_rank_in_ug_comm);
    CSP_DBG_PRINT(" Created ug_comm: %d/%d, local_ug_comm: %d/%d\n",
                     ug_rank, ug_nprocs, ug_local_rank, ug_local_nprocs);

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    ug_win->g_ops_counts = calloc(ug_nprocs, sizeof(int));
    ug_win->g_bytes_counts = calloc(ug_nprocs, sizeof(unsigned long));
#endif

    /* Allocate a shared window with local Ghosts */
    mpi_errno = PMPI_Win_allocate_shared(size, disp_unit, info, ug_win->local_ug_comm,
                                         &ug_win->base, &ug_win->local_ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    CSP_DBG_PRINT("[%d] allocate shared base = %p\n", user_rank, ug_win->base);

    /* Gather user offsets on corresponding ghost processes */
    mpi_errno = gather_base_offsets(size, ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    specify_main_gp_binding(ug_win);

    /* Create windows using shared buffers. */

    /* Send information to ghosts */
    if (user_local_rank == 0) {
        int func_params[2];
        func_params[0] = ug_win->max_local_user_nprocs;
        func_params[1] = ug_win->info_args.epoch_type;
        mpi_errno = CSP_Func_set_param((char *) func_params, sizeof(func_params),
                                          ug_win->ur_g_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
        CSP_DBG_PRINT(" Send parameters: max_local_user_nprocs %d, epoch_type %d \n",
                         ug_win->max_local_user_nprocs, ug_win->info_args.epoch_type);
    }

    if ((ug_win->info_args.epoch_type & CSP_EPOCH_LOCK) ||
        (ug_win->info_args.epoch_type & CSP_EPOCH_LOCK_ALL)) {

        mpi_errno = create_lock_windows(size, disp_unit, info, ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* - Create global active window */
    if ((ug_win->info_args.epoch_type & CSP_EPOCH_FENCE) ||
        (ug_win->info_args.epoch_type & CSP_EPOCH_PSCW)) {
        mpi_errno = PMPI_Win_create(ug_win->base, size, disp_unit, info,
                                    ug_win->ug_comm, &ug_win->active_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        CSP_DBG_PRINT("[%d] Created active window 0x%x\n", user_rank, ug_win->active_win);

        /* Since all processes must be in win_allocate, we do not need worry
         * the possibility losing asynchronous progress.
         * This lock_all guarantees the semantics correctness when internally
         * change to passive mode. */
        mpi_errno = PMPI_Win_lock_all(MPI_MODE_NOCHECK, ug_win->active_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
        ug_win->start_counter = 0;
    }

    /* Track epoch status for redirecting RMA to different window. */
    ug_win->epoch_stat = CSP_WIN_NO_EPOCH;

    /* - Only expose user window in order to hide ghosts in all non-wrapped window functions */
    mpi_errno = PMPI_Win_create(ug_win->base, size, disp_unit, info,
                                ug_win->user_comm, &ug_win->win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    CSP_DBG_PRINT("[%d] Created window 0x%x\n", user_rank, ug_win->win);

    *win = ug_win->win;
    *base_pp = ug_win->base;

    /* Gather the handle of Ghosts' win. User root is always rank num_g in user
     * root + ghosts communicator */
    /* TODO:
     * How about use handler on user root ?
     * How to solve the case that different processes may have the same handler ? */
    if (user_local_rank == 0) {
        unsigned long tmp_send_buf;
        ug_win->g_win_handles = calloc(CSP_ENV.num_g + 1, sizeof(unsigned long));
        mpi_errno = PMPI_Gather(&tmp_send_buf, 1, MPI_UNSIGNED_LONG, ug_win->g_win_handles,
                                1, MPI_UNSIGNED_LONG, CSP_ENV.num_g, ug_win->ur_g_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    CSP_Cache_ug_win(ug_win->win, ug_win);

  fn_exit:

    if (tmp_gather_buf)
        free(tmp_gather_buf);

    return mpi_errno;

  fn_fail:

    /* Caching is the last possible error, so we do not need remove
     * cache here. */

    if (ug_win->local_ug_win)
        PMPI_Win_free(&ug_win->local_ug_win);
    if (ug_win->win)
        PMPI_Win_free(&ug_win->win);
    if (ug_win->active_win)
        PMPI_Win_free(&ug_win->active_win);
    if (ug_win->num_ug_wins > 0 && ug_win->ug_wins) {
        for (i = 0; i < ug_win->num_ug_wins; i++) {
            if (ug_win->ug_wins)
                PMPI_Win_free(&ug_win->ug_wins[i]);
        }
    }

    if (ug_win->ur_g_comm && ug_win->ur_g_comm != MPI_COMM_NULL)
        PMPI_Comm_free(&ug_win->ur_g_comm);
    if (ug_win->local_ug_comm && ug_win->local_ug_comm != CSP_COMM_LOCAL)
        PMPI_Comm_free(&ug_win->local_ug_comm);
    if (ug_win->ug_comm != MPI_COMM_NULL)
        PMPI_Comm_free(&ug_win->ug_comm);
    if (ug_win->ug_group != MPI_GROUP_NULL)
        PMPI_Group_free(&ug_win->ug_group);
    if (ug_win->local_user_comm && ug_win->local_user_comm != CSP_COMM_USER_LOCAL)
        PMPI_Comm_free(&ug_win->local_user_comm);
    if (ug_win->user_root_comm && ug_win->user_root_comm != CSP_COMM_UR_WORLD)
        PMPI_Comm_free(&ug_win->user_root_comm);

    if (ug_win->local_ug_group != MPI_GROUP_NULL)
        PMPI_Group_free(&ug_win->local_ug_group);
    if (ug_win->ug_group != MPI_GROUP_NULL)
        PMPI_Group_free(&ug_win->ug_group);
    if (ug_win->user_group != MPI_GROUP_NULL)
        PMPI_Group_free(&ug_win->user_group);

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    if (ug_win->g_ops_counts)
        free(ug_win->g_ops_counts);
    if (ug_win->g_bytes_counts)
        free(ug_win->g_bytes_counts);
#endif

    if (ug_win->targets) {
        for (i = 0; i < user_nprocs; i++) {
            if (ug_win->targets[i].base_g_offsets)
                free(ug_win->targets[i].base_g_offsets);
            if (ug_win->targets[i].g_ranks_in_ug)
                free(ug_win->targets[i].g_ranks_in_ug);
            if (ug_win->targets[i].segs)
                free(ug_win->targets[i].segs);
        }
        free(ug_win->targets);
    }
    if (ug_win->g_ranks_in_ug)
        free(ug_win->g_ranks_in_ug);
    if (ug_win->g_win_handles)
        free(ug_win->g_win_handles);
    if (ug_win->ug_wins)
        free(ug_win->ug_wins);
    if (ug_win)
        free(ug_win);

    *win = MPI_WIN_NULL;
    *base_pp = NULL;

    goto fn_exit;
}

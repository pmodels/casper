#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "mtcore.h"

static int gather_ranks(MTCORE_Win * win, int *user_ranks_in_world, int *num_helpers,
                        int *helper_ranks_in_world)
{
    int mpi_errno = MPI_SUCCESS;
    int world_rank, user_nprocs, user_rank;
    int *nodes_bitmap = NULL;
    int tmp_world_rank, node_id, tmp_num_helpers;
    int i;

    nodes_bitmap = calloc(MTCORE_NUM_NODES, sizeof(unsigned short));
    if (nodes_bitmap == NULL)
        goto fn_fail;

    PMPI_Comm_size(win->user_comm, &user_nprocs);
    PMPI_Comm_rank(win->user_comm, &user_rank);

    /* Gather users' world ranks */
    PMPI_Comm_rank(MPI_COMM_WORLD, &user_ranks_in_world[user_rank]);
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, user_ranks_in_world, 1, MPI_INT,
                               win->user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Get helper ranks for all USER processes
     * TODO: support multiple helpers */
    tmp_num_helpers = 0;
    for (i = 0; i < user_nprocs; i++) {
        tmp_world_rank = user_ranks_in_world[i];
        node_id = MTCORE_ALL_NODE_IDS[tmp_world_rank];

        if (!nodes_bitmap[node_id]) {
            helper_ranks_in_world[tmp_num_helpers++] = MTCORE_ALL_H_IN_COMM_WORLD[node_id];
            nodes_bitmap[node_id] = 1;

            MTCORE_Assert(tmp_num_helpers <= MTCORE_NUM_NODES * MTCORE_NUM_H_IN_LOCAL);
        }
    }
    *num_helpers = tmp_num_helpers;

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int create_uh_comm(int *user_ranks_in_world, int num_helpers, int *helper_ranks_in_world,
                          MTCORE_Win * win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_nprocs, user_rank, world_nprocs, tmp_world_rank;
    int *uh_ranks_in_world = NULL, *h_info = NULL;
    int i, j, num_uh_ranks, h_rank, node_id;

    PMPI_Comm_size(win->user_comm, &user_nprocs);
    PMPI_Comm_rank(win->user_comm, &user_rank);
    PMPI_Comm_size(MPI_COMM_WORLD, &world_nprocs);

    /* maximum amount equals to world size */
    uh_ranks_in_world = calloc(world_nprocs, sizeof(int));
    if (uh_ranks_in_world == NULL)
        goto fn_fail;

    /* -Create uh communicator including all USER processes and Helper processes. */
    num_uh_ranks = user_nprocs + num_helpers;
    MTCORE_Assert(num_uh_ranks <= world_nprocs);
    memcpy(uh_ranks_in_world, user_ranks_in_world, user_nprocs * sizeof(int));
    memcpy(&uh_ranks_in_world[user_nprocs], helper_ranks_in_world, num_helpers * sizeof(int));

    PMPI_Group_incl(MTCORE_GROUP_WORLD, num_uh_ranks, uh_ranks_in_world, &win->uh_group);
    mpi_errno = PMPI_Comm_create_group(MPI_COMM_WORLD, win->uh_group, 0, &win->uh_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    if (uh_ranks_in_world)
        free(uh_ranks_in_world);

    return mpi_errno;

  fn_fail:
    if (win->uh_comm != MPI_COMM_NULL) {
        PMPI_Comm_free(&win->uh_comm);
        win->uh_comm = MPI_COMM_NULL;
    }
    if (win->uh_group != MPI_GROUP_NULL) {
        PMPI_Group_free(&win->uh_group);
        win->uh_group = MPI_GROUP_NULL;
    }

    goto fn_exit;
}


static int create_communicators(MTCORE_Win * uh_win, int uh_tag)
{
    int mpi_errno = MPI_SUCCESS;
    int *func_params = NULL, func_param_size = 0;
    int *user_ranks_in_world = NULL;
    int *helper_ranks_in_world = NULL, num_helpers = 0, max_num_helpers;
    int user_nprocs;

    PMPI_Comm_size(uh_win->user_comm, &user_nprocs);
    max_num_helpers = MTCORE_NUM_H_IN_LOCAL * MTCORE_NUM_NODES;
    func_param_size = user_nprocs + max_num_helpers + 3;
    func_params = calloc(func_param_size, sizeof(int));

    /* Optimization for user world communicator */
    if (uh_win->user_comm == MTCORE_COMM_USER_WORLD) {
        /* Set parameters to local Helpers
         *  [0]: is_comm_user_world
         *  [1]: max_local_user_nprocs
         */
        func_params[0] = 1;
        func_params[1] = uh_win->max_local_user_nprocs;
        MTCORE_Func_set_param((char *) func_params, sizeof(int) * func_param_size, uh_tag,
                              uh_win->local_user_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* Create communicators
         *  local_uh_comm: including local USER and Helper processes
         *  uh_comm: including all USER and Helper processes
         */
        uh_win->local_uh_comm = MTCORE_COMM_LOCAL;
        uh_win->uh_comm = MPI_COMM_WORLD;
        PMPI_Comm_group(uh_win->local_uh_comm, &uh_win->local_uh_group);
        PMPI_Comm_group(uh_win->uh_comm, &uh_win->uh_group);

        /* -Get all Helper rank in uh communicator */
        num_helpers = MTCORE_NUM_H_IN_LOCAL * MTCORE_NUM_NODES;
        memcpy(uh_win->h_ranks_in_uh, MTCORE_ALL_H_IN_COMM_WORLD, sizeof(int) * num_helpers);
    }
    else {
        user_ranks_in_world = calloc(user_nprocs, sizeof(int));
        helper_ranks_in_world = calloc(max_num_helpers, sizeof(int));

        /* Gather user rank information */
        mpi_errno = gather_ranks(uh_win, user_ranks_in_world, &num_helpers, helper_ranks_in_world);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* Set parameters to local Helpers
         *  [0]: is_comm_user_world
         *  [1]: max_local_user_nprocs
         *  [2]: num_helpers
         *  [3:N+2]: user ranks in comm_world
         *  [N+3:]: helper ranks in comm_world
         */
        int pidx;
        func_params[0] = 0;
        func_params[1] = uh_win->max_local_user_nprocs;
        func_params[2] = num_helpers;
        pidx = 3;
        memcpy(&func_params[pidx], user_ranks_in_world, user_nprocs * sizeof(int));
        pidx += user_nprocs;
        memcpy(&func_params[pidx], helper_ranks_in_world, num_helpers * sizeof(int));
        MTCORE_Func_set_param((char *) func_params, sizeof(int) * func_param_size, uh_tag,
                              uh_win->local_user_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* Create communicators
         *  uh_comm: including all USER and Helper processes
         *  local_uh_comm: including local USER and Helper processes
         */
        mpi_errno = create_uh_comm(user_ranks_in_world, num_helpers, helper_ranks_in_world, uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
        mpi_errno = PMPI_Comm_split_type(uh_win->uh_comm, MPI_COMM_TYPE_SHARED, 0,
                                         MPI_INFO_NULL, &uh_win->local_uh_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* Get all Helper rank in uh communicator */
        mpi_errno = PMPI_Group_translate_ranks(MTCORE_GROUP_WORLD, num_helpers,
                                               helper_ranks_in_world,
                                               uh_win->uh_group, uh_win->h_ranks_in_uh);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

#ifdef DEBUG
    int user_rank = 0, i;
    PMPI_Comm_rank(uh_win->user_comm, &user_rank);
    for (i = 0; i < num_helpers; i++) {
        MTCORE_DBG_PRINT("[%d]    h_ranks_in_uh[%d] = %d\n", user_rank, i,
                         uh_win->h_ranks_in_uh[i]);
    }
#endif

  fn_exit:
    return mpi_errno;

  fn_fail:
    if (func_params)
        free(func_params);
    if (user_ranks_in_world)
        free(user_ranks_in_world);
    if (helper_ranks_in_world)
        free(helper_ranks_in_world);

    goto fn_exit;
}

int MPI_Win_allocate(MPI_Aint size, int disp_unit, MPI_Info info,
                     MPI_Comm user_comm, void *baseptr, MPI_Win * win)
{
    static const char FCNAME[] = "MPI_Win_allocate";
    int mpi_errno = MPI_SUCCESS;
    MPI_Group uh_group;
    int uh_rank, uh_nprocs, user_nprocs, user_rank, user_world_rank,
        user_local_rank, user_local_nprocs, uh_local_rank, uh_local_nprocs;
    MTCORE_Win *uh_win;
    int uh_tag;
    int i, j;
    void **base_pp = (void **) baseptr;
    MPI_Status stat;
    MPI_Aint *user_local_sizes;

    MTCORE_DBG_PRINT_FCNAME();

    uh_win = calloc(1, sizeof(MTCORE_Win));

    /* If user specifies comm_world directly, use user comm_world instead;
     * else this communicator directly, because it should be created from user comm_world */
    if (user_comm == MPI_COMM_WORLD) {
        user_comm = MTCORE_COMM_USER_WORLD;
        uh_win->local_user_comm = MTCORE_COMM_USER_LOCAL;
    }
    else {
        mpi_errno = PMPI_Comm_split_type(user_comm, MPI_COMM_TYPE_SHARED, 0,
                                         MPI_INFO_NULL, &uh_win->local_user_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
    PMPI_Comm_group(user_comm, &uh_win->user_group);
    PMPI_Comm_size(user_comm, &user_nprocs);
    PMPI_Comm_rank(user_comm, &user_rank);
    PMPI_Comm_size(uh_win->local_user_comm, &user_local_nprocs);
    PMPI_Comm_rank(uh_win->local_user_comm, &user_local_rank);

    uh_win->user_comm = user_comm;
    uh_win->base_h_offsets = calloc(user_nprocs, sizeof(MPI_Aint));
    uh_win->disp_units = calloc(user_nprocs, sizeof(int));
    uh_win->h_ranks_in_uh = calloc(MTCORE_NUM_H_IN_LOCAL * MTCORE_NUM_NODES, sizeof(int));
    uh_win->local_user_ranks = calloc(user_nprocs, sizeof(int));;
    user_local_sizes = calloc(user_local_nprocs, sizeof(MPI_Aint));

    /* Gather disp_unit, used when send RMA operations */
    uh_win->disp_units[user_rank] = disp_unit;
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                               uh_win->disp_units, 1, MPI_INT, user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Gather users' rank in local user communicator, used in RMA and sync calls */
    uh_win->local_user_ranks[user_rank] = user_local_rank;
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                               uh_win->local_user_ranks, 1, MPI_INT, user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Get the maximum number of processes per node */
    mpi_errno = PMPI_Allreduce(&user_local_nprocs, &uh_win->max_local_user_nprocs,
                               1, MPI_INT, MPI_MAX, uh_win->user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Check if we are allowed to ignore force-lock for local target,
     * require force-lock by default. */
    uh_win->is_self_lock_grant_required = 1;
    if (info != MPI_INFO_NULL) {
        int no_local_load_store_flag = 0;
        char no_local_load_store_value[MPI_MAX_INFO_VAL + 1];
        PMPI_Info_get(info, "no_local_load_store", MPI_MAX_INFO_VAL,
                      no_local_load_store_value, &no_local_load_store_flag);
        if (no_local_load_store_flag == 1) {
            if (!strncmp(no_local_load_store_value, "true", strlen("true")))
                uh_win->is_self_lock_grant_required = 0;
        }
    }
    MTCORE_DBG_PRINT("[%d] uh_win->is_self_lock_grant_required %d\n", user_rank,
                     uh_win->is_self_lock_grant_required);

    /* Notify Helper start */
    mpi_errno = MTCORE_Tag_format((int) user_comm, &uh_tag);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    MTCORE_Func_start(MTCORE_FUNC_WIN_ALLOCATE, user_nprocs, user_local_nprocs, uh_tag,
                      uh_win->local_user_comm);

    /* Create communicators
     *  uh_comm: including all USER and Helper processes
     *  local_uh_comm: including local USER and Helper processes
     */
    mpi_errno = create_communicators(uh_win, uh_tag);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    PMPI_Comm_rank(uh_win->local_uh_comm, &uh_local_rank);
    PMPI_Comm_size(uh_win->local_uh_comm, &uh_local_nprocs);

    PMPI_Comm_size(uh_win->uh_comm, &uh_nprocs);
    PMPI_Comm_rank(uh_win->uh_comm, &uh_rank);
    MTCORE_DBG_PRINT(" Created uh_comm: %d/%d, local_uh_comm: %d/%d\n",
                     uh_rank, uh_nprocs, uh_local_rank, uh_local_nprocs);

    /* Allocate a shared window with local Helpers */

    /* -Allocate shared window */
    mpi_errno = PMPI_Win_allocate_shared(size, disp_unit, info, uh_win->local_uh_comm,
                                         &uh_win->base, &uh_win->local_uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* -Gather size from all local user processes and calculate the offset of local shared buffer */
    user_local_sizes[user_local_rank] = size;
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                               user_local_sizes, 1, MPI_AINT, uh_win->local_user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    i = 0;
    uh_win->base_h_offsets[user_rank] = 0;
#ifdef MTCORE_ENABLE_GRANT_LOCK_HIDDEN_BYTE
    /* Add additional bytes on Helper 0 */
    uh_win->base_h_offsets[user_rank] += sizeof(MTCORE_GRANT_LOCK_DATATYPE);
    /* Helper 0 is rank 0 in local_uh_comm, so its offset is 0. */
    uh_win->grant_lock_h_offset = 0;
#endif
    while (i < user_local_rank) {
        uh_win->base_h_offsets[user_rank] += user_local_sizes[i];      /* size in bytes */
        i++;
    }
    MTCORE_DBG_PRINT("[%d] local base_h_offsets = 0x%lx, base=%p\n", user_rank,
                     uh_win->base_h_offsets[user_rank], uh_win->base);

    /* -Receive the address of all the shared user buffers on Helper processes
     * TODO: support multiple helpers */
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, uh_win->base_h_offsets, 1,
                               MPI_AINT, user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

#ifdef DEBUG
    for (i = 0; i < user_nprocs; i++) {
        MTCORE_DBG_PRINT("[%d] base_h_offsets[%d] = 0x%lx\n", user_rank, i,
                         uh_win->base_h_offsets[i]);
    }
#endif

    /* Create windows using shared buffers. */

    /* -Create uh windows.
     *  Every User process has a window used for permission check and accessing Helpers.
     *  User processes in different nodes can share a window.
     *      i.e., win[x] can be shared by processes whose local rank is x. */
    uh_win->uh_wins = calloc(uh_win->max_local_user_nprocs, sizeof(MPI_Win));
    for (i = 0; i < uh_win->max_local_user_nprocs; i++) {
        mpi_errno = PMPI_Win_create(uh_win->base, size, disp_unit, info,
                                    uh_win->uh_comm, &uh_win->uh_wins[i]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        MTCORE_DBG_PRINT("[%d] Created uh windows[%d] 0x%x\n", user_rank, i, uh_win->uh_wins[i]);
    }

    /* - Only expose user window in order to hide helpers in all non-wrapped window functions */
    mpi_errno = PMPI_Win_create(uh_win->base, size, disp_unit, info,
                                uh_win->user_comm, &uh_win->win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    MTCORE_DBG_PRINT("[%d] Created window 0x%x\n", user_rank, uh_win->win);

    *win = uh_win->win;
    *base_pp = uh_win->base;

    /* Receive the handle of Helper win */
    if (user_local_rank == 0) {
        mpi_errno = PMPI_Recv(&uh_win->h_win_handle, 1, MPI_UNSIGNED_LONG,
                              MTCORE_RANK_IN_COMM_LOCAL, uh_tag, MTCORE_COMM_LOCAL, &stat);
        if (mpi_errno != 0)
            goto fn_fail;
    }

    mpi_errno = put_uh_win(*win, uh_win);
    if (mpi_errno != 0)
        goto fn_fail;

  fn_exit:

    if (user_local_sizes)
        free(user_local_sizes);

    return mpi_errno;

  fn_fail:

    if (uh_win->local_uh_win)
        PMPI_Win_free(&uh_win->local_uh_win);
    if (uh_win->win)
        PMPI_Win_free(&uh_win->win);
    if (uh_win->uh_wins) {
        for (i = 0; i < uh_win->max_local_user_nprocs; i++) {
            if (uh_win->uh_wins)
                PMPI_Win_free(&uh_win->uh_wins[i]);
        }
    }

    if (uh_win->local_uh_comm && uh_win->local_uh_comm != MTCORE_COMM_LOCAL)
        PMPI_Comm_free(&uh_win->local_uh_comm);
    if (uh_win->uh_comm != MPI_COMM_NULL)
        PMPI_Comm_free(&uh_win->uh_comm);
    if (uh_win->uh_group != MPI_GROUP_NULL)
        PMPI_Group_free(&uh_win->uh_group);
    if (uh_win->local_user_comm && uh_win->local_user_comm != MTCORE_COMM_USER_LOCAL)
        PMPI_Comm_free(&uh_win->local_user_comm);

    if (uh_win->local_uh_group != MPI_GROUP_NULL)
        PMPI_Group_free(&uh_win->local_uh_group);
    if (uh_win->uh_group != MPI_GROUP_NULL)
        PMPI_Group_free(&uh_win->uh_group);
    if (uh_win->user_group != MPI_GROUP_NULL)
        PMPI_Group_free(&uh_win->user_group);

    if (uh_win->disp_units)
        free(uh_win->disp_units);
    if (uh_win->base_h_offsets)
        free(uh_win->base_h_offsets);
    if (uh_win->local_uh_win_param)
        free(uh_win->local_uh_win_param);
    if (uh_win->h_ranks_in_uh)
        free(uh_win->h_ranks_in_uh);
    if (uh_win->local_user_ranks)
        free(uh_win->local_user_ranks);
    if (uh_win->uh_wins)
        free(uh_win->uh_wins);
    if (uh_win)
        free(uh_win);

    *win = MPI_WIN_NULL;
    *base_pp = NULL;

    goto fn_exit;
}

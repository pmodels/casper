#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "mpiasp.h"

static int gather_ranks(MPIASP_Win * win, int *user_ranks_in_world, int *num_helpers,
                        int *helper_ranks_in_world)
{
    int mpi_errno = MPI_SUCCESS;
    int world_rank, user_nprocs, user_rank;
    int *nodes_bitmap = NULL;
    int tmp_world_rank, node_id, tmp_num_helpers;
    int i;

    nodes_bitmap = calloc(MPIASP_NUM_NODES, sizeof(unsigned short));
    if (nodes_bitmap == NULL)
        goto fn_fail;

    PMPI_Comm_size(win->user_comm, &user_nprocs);
    PMPI_Comm_rank(win->user_comm, &user_rank);

    // Gather user world ranks and user_world ranks together
    PMPI_Comm_rank(MPI_COMM_WORLD, &user_ranks_in_world[user_rank]);
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, user_ranks_in_world, 1, MPI_INT,
                               win->user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    // Get helper ranks for all USER processes
    // TODO: support multiple helpers
    tmp_num_helpers = 0;
    for (i = 0; i < user_nprocs; i++) {
        tmp_world_rank = user_ranks_in_world[i];
        node_id = MPIASP_ALL_NODE_IDS[tmp_world_rank];

        if (!nodes_bitmap[node_id]) {
            helper_ranks_in_world[tmp_num_helpers++] = MPIASP_ALL_ASP_IN_COMM_WORLD[node_id];
            nodes_bitmap[node_id] = 1;

            MPIASP_Assert(tmp_num_helpers <= MPIASP_NUM_NODES * MPIASP_NUM_ASP_IN_LOCAL);
        }
    }
    *num_helpers = tmp_num_helpers;

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int create_ua_comm(int *user_ranks_in_world, int num_helpers, int *helper_ranks_in_world,
                          MPIASP_Win * win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_nprocs, user_rank, world_nprocs, tmp_world_rank;
    int *ua_ranks_in_world = NULL, *asp_info = NULL;
    int i, j, num_ua_ranks, asp_rank, node_id;

    PMPI_Comm_size(win->user_comm, &user_nprocs);
    PMPI_Comm_rank(win->user_comm, &user_rank);
    PMPI_Comm_size(MPI_COMM_WORLD, &world_nprocs);

    // maximum amount equals to world size
    ua_ranks_in_world = calloc(world_nprocs, sizeof(int));
    if (ua_ranks_in_world == NULL)
        goto fn_fail;

    // -Create ua communicator including all USER processes and Helper processes.
    num_ua_ranks = user_nprocs + num_helpers;
    MPIASP_Assert(num_ua_ranks <= world_nprocs);
    memcpy(ua_ranks_in_world, user_ranks_in_world, user_nprocs * sizeof(int));
    memcpy(&ua_ranks_in_world[user_nprocs], helper_ranks_in_world, num_helpers * sizeof(int));

    PMPI_Group_incl(MPIASP_GROUP_WORLD, num_ua_ranks, ua_ranks_in_world, &win->ua_group);
    mpi_errno = PMPI_Comm_create_group(MPI_COMM_WORLD, win->ua_group, 0, &win->ua_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    if (ua_ranks_in_world)
        free(ua_ranks_in_world);

    return mpi_errno;

  fn_fail:
    if (win->ua_comm != MPI_COMM_NULL) {
        PMPI_Comm_free(&win->ua_comm);
        win->ua_comm = MPI_COMM_NULL;
    }
    if (win->ua_group != MPI_GROUP_NULL) {
        PMPI_Group_free(&win->ua_group);
        win->ua_group = MPI_GROUP_NULL;
    }

    goto fn_exit;
}


static int create_communicators(MPIASP_Win * ua_win, int ua_tag)
{
    int mpi_errno = MPI_SUCCESS;
    int *func_params = NULL, func_param_size = 0;
    int *user_ranks_in_world = NULL;
    int *helper_ranks_in_world = NULL, num_helpers = 0, max_num_helpers;
    int user_nprocs;

    PMPI_Comm_size(ua_win->user_comm, &user_nprocs);
    max_num_helpers = MPIASP_NUM_ASP_IN_LOCAL * MPIASP_NUM_NODES;
    func_param_size = user_nprocs + max_num_helpers + 1;
    func_params = calloc(func_param_size, sizeof(int));

    // Optimization for user world communicator
    if (ua_win->user_comm == MPIASP_COMM_USER_WORLD) {
        /* Set parameters to local Helpers
         *  [0]: is_comm_user_world
         */
        func_params[0] = 1;
        MPIASP_Func_set_param((char *) func_params, sizeof(int) * func_param_size, ua_tag,
                              ua_win->local_user_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* Create communicators
         *  local_ua_comm: including local USER and Helper processes
         *  ua_comm: including all USER and Helper processes
         */
        ua_win->local_ua_comm = MPIASP_COMM_LOCAL;
        ua_win->ua_comm = MPI_COMM_WORLD;
        PMPI_Comm_group(ua_win->local_ua_comm, &ua_win->local_ua_group);
        PMPI_Comm_group(ua_win->ua_comm, &ua_win->ua_group);

        // -Get all Helper rank in ua communicator */
        num_helpers = MPIASP_NUM_ASP_IN_LOCAL * MPIASP_NUM_NODES;
        memcpy(ua_win->asp_ranks_in_ua, MPIASP_ALL_ASP_IN_COMM_WORLD, sizeof(int) * num_helpers);
    }
    else {
        user_ranks_in_world = calloc(user_nprocs, sizeof(int));
        helper_ranks_in_world = calloc(max_num_helpers, sizeof(int));

        /* Gather user rank information */
        mpi_errno = gather_ranks(ua_win, user_ranks_in_world, &num_helpers, helper_ranks_in_world);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* Set parameters to local Helpers
         *  [0]: is_comm_user_world
         *  [1]: num_helpers
         *  [2:N+1]: user ranks in comm_world
         *  [N+2:]: helper ranks in comm_world
         */
        int pidx;
        func_params[0] = 0;
        func_params[1] = num_helpers;
        pidx = 2;
        memcpy(&func_params[pidx], user_ranks_in_world, user_nprocs * sizeof(int));
        pidx += user_nprocs;
        memcpy(&func_params[pidx], helper_ranks_in_world, num_helpers * sizeof(int));
        MPIASP_Func_set_param((char *) func_params, sizeof(int) * func_param_size, ua_tag,
                              ua_win->local_user_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* Create communicators
         *  ua_comm: including all USER and Helper processes
         *  local_ua_comm: including local USER and Helper processes
         */
        mpi_errno = create_ua_comm(user_ranks_in_world, num_helpers, helper_ranks_in_world, ua_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
        mpi_errno = PMPI_Comm_split_type(ua_win->ua_comm, MPI_COMM_TYPE_SHARED, 0,
                                         MPI_INFO_NULL, &ua_win->local_ua_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* Get all Helper rank in ua communicator */
        mpi_errno = PMPI_Group_translate_ranks(MPIASP_GROUP_WORLD, num_helpers,
                                               helper_ranks_in_world,
                                               ua_win->ua_group, ua_win->asp_ranks_in_ua);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

#ifdef DEBUG
    int user_rank = 0, i;
    PMPI_Comm_rank(ua_win->user_comm, &user_rank);
    for (i = 0; i < num_helpers; i++) {
        MPIASP_DBG_PRINT("[%d]    asp_ranks_in_ua[%d] = %d\n", user_rank, i,
                         ua_win->asp_ranks_in_ua[i]);
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
    MPI_Group ua_group;
    int ua_rank, ua_nprocs, user_nprocs, user_rank, user_world_rank,
        user_local_rank, user_local_nprocs, ua_local_rank, ua_local_nprocs;
    MPIASP_Win *ua_win;
    int ua_tag;
    int i, j;
    void **base_pp = (void **) baseptr;
    MPI_Status stat;
    MPI_Aint *user_local_sizes;

    MPIASP_DBG_PRINT_FCNAME();

    ua_win = calloc(1, sizeof(MPIASP_Win));

    /* If user specifies comm_world directly, use user comm_world instead;
     * else this communicator directly, because it should be created from user comm_world */
    if (user_comm == MPI_COMM_WORLD) {
        user_comm = MPIASP_COMM_USER_WORLD;
        ua_win->local_user_comm = MPIASP_COMM_USER_LOCAL;
    }
    else {
        mpi_errno = PMPI_Comm_split_type(user_comm, MPI_COMM_TYPE_SHARED, 0,
                                         MPI_INFO_NULL, &ua_win->local_user_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
    PMPI_Comm_group(user_comm, &ua_win->user_group);
    PMPI_Comm_size(user_comm, &user_nprocs);
    PMPI_Comm_rank(user_comm, &user_rank);
    PMPI_Comm_size(ua_win->local_user_comm, &user_local_nprocs);
    PMPI_Comm_rank(ua_win->local_user_comm, &user_local_rank);

    ua_win->base_asp_offset = calloc(user_nprocs, sizeof(MPI_Aint));
    ua_win->user_comm = user_comm;
    ua_win->disp_units = calloc(user_nprocs, sizeof(int));
    ua_win->asp_ranks_in_ua = calloc(MPIASP_NUM_ASP_IN_LOCAL * MPIASP_NUM_NODES, sizeof(int));
    ua_win->ua_wins = calloc(user_nprocs, sizeof(MPI_Win));
    user_local_sizes = calloc(user_local_nprocs, sizeof(MPI_Aint));

    /* Gather disp_unit, used when send RMA operations */
    ua_win->disp_units[user_rank] = disp_unit;
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                               ua_win->disp_units, 1, MPI_INT, user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Check if we are allowed to ignore force-lock for local target,
     * require force-lock by default. */
    ua_win->is_self_lock_grant_required = 1;
    if (info != MPI_INFO_NULL) {
        int no_local_load_store_flag = 0;
        char no_local_load_store_value[MPI_MAX_INFO_VAL + 1];
        PMPI_Info_get(info, "no_local_load_store", MPI_MAX_INFO_VAL,
                      no_local_load_store_value, &no_local_load_store_flag);
        if (no_local_load_store_flag == 1) {
            if (!strncmp(no_local_load_store_value, "true", strlen("true")))
                ua_win->is_self_lock_grant_required = 0;
        }
    }
    MPIASP_DBG_PRINT("[%d] ua_win->is_self_lock_grant_required %d\n", user_rank,
                     ua_win->is_self_lock_grant_required);

    /* Notify Helper start */
    mpi_errno = MPIASP_Tag_format((int) user_comm, &ua_tag);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    MPIASP_Func_start(MPIASP_FUNC_WIN_ALLOCATE, user_nprocs, user_local_nprocs, ua_tag,
                      ua_win->local_user_comm);

    /* Create communicators
     *  ua_comm: including all USER and Helper processes
     *  local_ua_comm: including local USER and Helper processes
     */
    mpi_errno = create_communicators(ua_win, ua_tag);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    PMPI_Comm_rank(ua_win->local_ua_comm, &ua_local_rank);
    PMPI_Comm_size(ua_win->local_ua_comm, &ua_local_nprocs);

    PMPI_Comm_size(ua_win->ua_comm, &ua_nprocs);
    PMPI_Comm_rank(ua_win->ua_comm, &ua_rank);
    MPIASP_DBG_PRINT(" Created ua_comm: %d/%d, local_ua_comm: %d/%d\n",
                     ua_rank, ua_nprocs, ua_local_rank, ua_local_nprocs);

    /* Allocate a shared window with local Helpers */

    // -Allocate shared window
    mpi_errno = PMPI_Win_allocate_shared(size, disp_unit, info, ua_win->local_ua_comm,
                                         &ua_win->base, &ua_win->local_ua_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    // -Gather size from all local user processes and calculate the offset of local shared buffer
    user_local_sizes[user_local_rank] = size;
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                               user_local_sizes, 1, MPI_AINT, ua_win->local_user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    i = 0;
    ua_win->base_asp_offset[user_rank] = 0;
#ifdef MTCORE_ENABLE_GRANT_LOCK_HIDDEN_BYTE
    /* Add additional bytes on Helper 0 */
    ua_win->base_asp_offset[user_rank] += sizeof(MTCORE_GRANT_LOCK_DATATYPE);
    /* Helper 0 is rank 0 in local_ua_comm, so its offset is 0. */
    ua_win->grant_lock_asp_offset = 0;
#endif
    while (i < user_local_rank) {
        ua_win->base_asp_offset[user_rank] += user_local_sizes[i];      // size in bytes
        i++;
    }
    MPIASP_DBG_PRINT("[%d] local base_asp_offset = 0x%lx, base=%p\n", user_rank,
                     ua_win->base_asp_offset[user_rank], ua_win->base);

    // -Receive the address of all the shared user buffers on Helper processes
    // TODO: support multiple helpers
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, ua_win->base_asp_offset, 1,
                               MPI_AINT, user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

#ifdef DEBUG
    for (i = 0; i < user_nprocs; i++) {
        MPIASP_DBG_PRINT("[%d] base_asp_offset[%d] = 0x%lx\n", user_rank, i,
                         ua_win->base_asp_offset[i]);
    }
#endif

    /* Create windows using shared buffers. */

    // -Create ua windows
    //  Every User process has a dedicated window used for permission check and accessing Helpers
    for (i = 0; i < user_nprocs; i++) {
        mpi_errno = PMPI_Win_create(ua_win->base, size, disp_unit, info,
                                    ua_win->ua_comm, &ua_win->ua_wins[i]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        MPIASP_DBG_PRINT("[%d] Created ua windows[%d] 0x%x\n", user_rank, i, ua_win->ua_wins[i]);
    }

    // - Only expose user window in order to hide helpers in all non-wrapped window functions
    mpi_errno = PMPI_Win_create(ua_win->base, size, disp_unit, info,
                                ua_win->user_comm, &ua_win->win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    MPIASP_DBG_PRINT("[%d] Created window 0x%x\n", user_rank, ua_win->win);

    *win = ua_win->win;
    *base_pp = ua_win->base;

    /* Receive the handle of ASP win */
    if (user_local_rank == 0) {
        mpi_errno = PMPI_Recv(&ua_win->asp_win_handle, 1, MPI_UNSIGNED_LONG,
                              MPIASP_RANK_IN_COMM_LOCAL, ua_tag, MPIASP_COMM_LOCAL, &stat);
        if (mpi_errno != 0)
            goto fn_fail;
    }

    mpi_errno = put_ua_win(*win, ua_win);
    if (mpi_errno != 0)
        goto fn_fail;

  fn_exit:

    if (user_local_sizes)
        free(user_local_sizes);

    return mpi_errno;

  fn_fail:

    if (ua_win->local_ua_win)
        PMPI_Win_free(&ua_win->local_ua_win);
    if (ua_win->win)
        PMPI_Win_free(&ua_win->win);
    if (ua_win->ua_wins) {
        for (i = 0; i < user_nprocs; i++) {
            if (ua_win->ua_wins)
                PMPI_Win_free(&ua_win->ua_wins[i]);
        }
    }

    if (ua_win->local_ua_comm && ua_win->local_ua_comm != MPIASP_COMM_LOCAL)
        PMPI_Comm_free(&ua_win->local_ua_comm);
    if (ua_win->ua_comm != MPI_COMM_NULL)
        PMPI_Comm_free(&ua_win->ua_comm);
    if (ua_win->ua_group != MPI_GROUP_NULL)
        PMPI_Group_free(&ua_win->ua_group);
    if (ua_win->local_user_comm && ua_win->local_user_comm != MPIASP_COMM_USER_LOCAL)
        PMPI_Comm_free(&ua_win->local_user_comm);

    if (ua_win->local_ua_group != MPI_GROUP_NULL)
        PMPI_Group_free(&ua_win->local_ua_group);
    if (ua_win->ua_group != MPI_GROUP_NULL)
        PMPI_Group_free(&ua_win->ua_group);
    if (ua_win->user_group != MPI_GROUP_NULL)
        PMPI_Group_free(&ua_win->user_group);

    if (ua_win->disp_units)
        free(ua_win->disp_units);
    if (ua_win->base_asp_offset)
        free(ua_win->base_asp_offset);
    if (ua_win->local_ua_win_param)
        free(ua_win->local_ua_win_param);
    if (ua_win->asp_ranks_in_ua)
        free(ua_win->asp_ranks_in_ua);
    if (ua_win->ua_wins)
        free(ua_win->ua_wins);
    if (ua_win)
        free(ua_win);

    *win = MPI_WIN_NULL;
    *base_pp = NULL;

    goto fn_exit;
}

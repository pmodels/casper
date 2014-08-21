#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "mtcore.h"

static int gather_ranks(MTCORE_Win * win, int *user_ranks_in_world, int *num_helpers,
                        int *helper_ranks_in_world, int *unique_helper_ranks_in_world)
{
    int mpi_errno = MPI_SUCCESS;
    int user_nprocs, user_rank;
    int *helper_bitmap = NULL;
    int user_world_rank, tmp_num_helpers;
    int i, j, helper_rank;
    int *user_ranks = NULL;

    helper_bitmap = calloc(MTCORE_NUM_NODES, sizeof(unsigned short));
    if (helper_bitmap == NULL)
        goto fn_fail;

    PMPI_Comm_size(win->user_comm, &user_nprocs);
    PMPI_Comm_rank(win->user_comm, &user_rank);

    user_ranks = calloc(user_nprocs * 2, sizeof(int));
    if (user_ranks == NULL)
        goto fn_fail;

    /* Gather users' world rank and user world rank */
    PMPI_Comm_rank(MPI_COMM_WORLD, &user_ranks[2 * user_rank]);
    PMPI_Comm_rank(MTCORE_COMM_USER_WORLD, &user_ranks[2 * user_rank + 1]);
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, user_ranks, 2, MPI_INT,
                               win->user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Get helper ranks of each USER process.
     *
     * The helpers of user_world rank x are stored as x*num_h: (x+1)*num_h-1,
     * it is used to catch helpers for a target rank in epoch.
     * Unique helper ranks are only used for creating communicators.*/
    tmp_num_helpers = 0;
    for (i = 0; i < user_nprocs; i++) {
        user_ranks_in_world[i] = user_ranks[2 * user_rank];     /*copy world rank in the same loop */
        user_world_rank = user_ranks[2 * user_rank + 1];

        for (j = 0; j < MTCORE_NUM_H; j++) {
            helper_rank = MTCORE_ALL_H_RANKS_IN_WORLD[user_world_rank * MTCORE_NUM_H + j];
            helper_ranks_in_world[i * MTCORE_NUM_H + j] = helper_rank;

            /* Unique helper ranks */
            if (!helper_bitmap[helper_rank]) {
                unique_helper_ranks_in_world[tmp_num_helpers++] = helper_rank;
                helper_bitmap[helper_rank] = 1;

                MTCORE_Assert(tmp_num_helpers <= MTCORE_NUM_NODES * MTCORE_NUM_H);
            }
        }
    }
    *num_helpers = tmp_num_helpers;

  fn_exit:
    if (helper_bitmap)
        free(helper_bitmap);
    if (user_ranks)
        free(user_ranks);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

#if (MTCORE_LOCK_OPTION == MTCORE_LOCK_OPTION_SERIAL_ASYNC)
static void specify_user_main_helper(MTCORE_Win * uh_win)
{
    int i, off;
    int permission_h_rank, user_nprocs, local_user_rank;

    PMPI_Comm_size(uh_win->user_comm, &user_nprocs);

    /* Specify main helper of each user following the order of users' rank
     * in corresponding local user communicator (i.e., P0, P1, P2 will be specified
     * to H0, H1, H0 respectively). Each main helper is stored in h_ranks_in_uh[rank][0],
     * and the original rank is moved to main helper's place (i.e., P1's helper
     * ranks are stored as H1, H0). */
    for (i = 0; i < user_nprocs; i++) {
        local_user_rank = uh_win->local_user_ranks[i];
        off = local_user_rank % MTCORE_NUM_H;
        permission_h_rank = uh_win->h_ranks_in_uh[i * MTCORE_NUM_H + off];
        uh_win->h_ranks_in_uh[i * MTCORE_NUM_H + off] = uh_win->h_ranks_in_uh[i * MTCORE_NUM_H];
        uh_win->h_ranks_in_uh[i * MTCORE_NUM_H] = permission_h_rank;
    }
}
#endif

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


static int create_communicators(MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int *func_params = NULL, func_param_size = 0;
    int *user_ranks_in_world = NULL;
    int *helper_ranks_in_world = NULL, *unique_helper_ranks_in_world = NULL;
    int num_helpers = 0, max_num_helpers;
    int user_nprocs, user_local_rank;

    PMPI_Comm_size(uh_win->user_comm, &user_nprocs);
    max_num_helpers = MTCORE_NUM_H * MTCORE_NUM_NODES;
    func_param_size = user_nprocs + max_num_helpers + 3;

    PMPI_Comm_rank(uh_win->local_user_comm, &user_local_rank);

    /* Optimization for user world communicator */
    if (uh_win->user_comm == MTCORE_COMM_USER_WORLD) {
        if (user_local_rank == 0) {
            func_params = calloc(func_param_size, sizeof(int));

            /* Set parameters to local Helpers
             *  [0]: is_comm_user_world
             *  [1]: max_local_user_nprocs
             */
            func_params[0] = 1;
            func_params[1] = uh_win->max_local_user_nprocs;

            MTCORE_Func_set_param((char *) func_params, sizeof(int) * func_param_size,
                                  uh_win->ur_h_comm);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        /* Create communicators
         *  local_uh_comm: including local USER and Helper processes
         *  uh_comm: including all USER and Helper processes
         */
        uh_win->local_uh_comm = MTCORE_COMM_LOCAL;
        uh_win->uh_comm = MPI_COMM_WORLD;
        PMPI_Comm_group(uh_win->local_uh_comm, &uh_win->local_uh_group);
        PMPI_Comm_group(uh_win->uh_comm, &uh_win->uh_group);

        /* -Get all Helper rank in uh communicator */
        memcpy(uh_win->h_ranks_in_uh, MTCORE_ALL_H_RANKS_IN_WORLD,
               sizeof(int) * MTCORE_NUM_H * user_nprocs);

#if (MTCORE_LOCK_OPTION == MTCORE_LOCK_OPTION_SERIAL_ASYNC)
        specify_user_main_helper(uh_win);
#endif
    }
    else {
        user_ranks_in_world = calloc(user_nprocs, sizeof(int));
        /* helper ranks for every user process, used for helper fetching in epoch */
        helper_ranks_in_world = calloc(MTCORE_NUM_H * user_nprocs, sizeof(int));
        /* unique helper ranks, used for creating communicators */
        unique_helper_ranks_in_world = calloc(max_num_helpers, sizeof(int));

        /* Gather user rank information */
        mpi_errno = gather_ranks(uh_win, user_ranks_in_world, &num_helpers,
                                 helper_ranks_in_world, unique_helper_ranks_in_world);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        if (user_local_rank == 0) {

            /* Set parameters to local Helpers
             *  [0]: is_comm_user_world
             *  [1]: max_local_user_nprocs
             *  [2]: num_helpers
             *  [3:N+2]: user ranks in comm_world
             *  [N+3:]: helper ranks in comm_world
             */
            int pidx;
            func_params = calloc(func_param_size, sizeof(int));

            func_params[0] = 0;
            func_params[1] = uh_win->max_local_user_nprocs;
            func_params[2] = num_helpers;
            pidx = 3;
            memcpy(&func_params[pidx], user_ranks_in_world, user_nprocs * sizeof(int));
            pidx += user_nprocs;
            memcpy(&func_params[pidx], unique_helper_ranks_in_world, num_helpers * sizeof(int));
            MTCORE_Func_set_param((char *) func_params, sizeof(int) * func_param_size,
                                  uh_win->ur_h_comm);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
        /* Create communicators
         *  uh_comm: including all USER and Helper processes
         *  local_uh_comm: including local USER and Helper processes
         */
        mpi_errno = create_uh_comm(user_ranks_in_world, num_helpers, unique_helper_ranks_in_world,
                                   uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
        mpi_errno = PMPI_Comm_split_type(uh_win->uh_comm, MPI_COMM_TYPE_SHARED, 0,
                                         MPI_INFO_NULL, &uh_win->local_uh_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* Get all Helper rank in uh communicator */
        mpi_errno = PMPI_Group_translate_ranks(MTCORE_GROUP_WORLD, user_nprocs * MTCORE_NUM_H,
                                               helper_ranks_in_world, uh_win->uh_group,
                                               uh_win->h_ranks_in_uh);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

#if (MTCORE_LOCK_OPTION == MTCORE_LOCK_OPTION_SERIAL_ASYNC)
        specify_user_main_helper(uh_win);
#endif
    }

#ifdef DEBUG
    int user_rank = 0, i, j;
    PMPI_Comm_rank(uh_win->user_comm, &user_rank);
    for (i = 0; i < user_nprocs; i++) {
        MTCORE_DBG_PRINT("\t h_ranks_in_uh[%d:%d - %d] =\n", i, i * MTCORE_NUM_H,
                         (i + 1) * MTCORE_NUM_H - 1);
        for (j = 0; j < MTCORE_NUM_H; j++) {
            MTCORE_DBG_PRINT("\t\t %d\n", uh_win->h_ranks_in_uh[i * MTCORE_NUM_H + j]);
        }
    }
#endif

  fn_exit:
    if (func_params)
        free(func_params);
    if (user_ranks_in_world)
        free(user_ranks_in_world);
    if (helper_ranks_in_world)
        free(helper_ranks_in_world);
    if (unique_helper_ranks_in_world)
        free(unique_helper_ranks_in_world);
    return mpi_errno;

  fn_fail:
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
    int i, j;
    void **base_pp = (void **) baseptr;
    MPI_Status stat;
    MPI_Aint *user_local_sizes;
    int *tmp_gather_buf = NULL;

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
    uh_win->h_ranks_in_uh = calloc(MTCORE_NUM_H * user_nprocs, sizeof(int));
    uh_win->local_user_ranks = calloc(user_nprocs, sizeof(int));
    user_local_sizes = calloc(user_local_nprocs, sizeof(MPI_Aint));

#if (MTCORE_LOAD_OPT != MTCORE_LOAD_OPT_NON)
    uh_win->order_h_ranks_in_uh = calloc(user_nprocs, sizeof(int));
    uh_win->is_main_lock_granted = calloc(user_nprocs, sizeof(int));
#endif

    /* Gather users' disp_unit and rank in local user communicator,
     * used in RMA and RMA & sync calls respectively */
    tmp_gather_buf = calloc(user_nprocs * 2, sizeof(int));
    tmp_gather_buf[2 * user_rank] = disp_unit;
    tmp_gather_buf[2 * user_rank + 1] = user_local_rank;
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                               tmp_gather_buf, 2, MPI_INT, user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    for (i = 0; i < user_nprocs; i++) {
        uh_win->disp_units[i] = tmp_gather_buf[2 * i];
        uh_win->local_user_ranks[i] = tmp_gather_buf[2 * i + 1];
    }

#ifdef DEBUG
    MTCORE_DBG_PRINT("my user local rank %d/%d\n", user_local_rank, user_local_nprocs);
    for (i = 0; i < user_nprocs; i++) {
        MTCORE_DBG_PRINT("\t disp_units[%d] = %d\n", i, uh_win->disp_units[i]);
    }
    for (i = 0; i < user_nprocs; i++) {
        MTCORE_DBG_PRINT("\t local_user_ranks[%d] = %d\n", i, uh_win->local_user_ranks[i]);
    }
#endif

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
    MTCORE_DBG_PRINT("uh_win->is_self_lock_grant_required %d\n",
                     uh_win->is_self_lock_grant_required);

    /* Notify Helpers start and create user root + helpers communicator for
     * internal information exchange between users and helpers. */
    if (user_local_rank == 0) {
        mpi_errno = MTCORE_Func_start(MTCORE_FUNC_WIN_ALLOCATE, user_nprocs, user_local_nprocs);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        mpi_errno = MTCORE_Func_new_ur_h_comm(&uh_win->ur_h_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* Create communicators
     *  uh_comm: including all USER and Helper processes
     *  local_uh_comm: including local USER and Helper processes
     */
    mpi_errno = create_communicators(uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    PMPI_Comm_rank(uh_win->local_uh_comm, &uh_local_rank);
    PMPI_Comm_size(uh_win->local_uh_comm, &uh_local_nprocs);
    PMPI_Comm_size(uh_win->uh_comm, &uh_nprocs);
    PMPI_Comm_rank(uh_win->uh_comm, &uh_rank);
    MTCORE_DBG_PRINT(" Created uh_comm: %d/%d, local_uh_comm: %d/%d\n",
                     uh_rank, uh_nprocs, uh_local_rank, uh_local_nprocs);


#if (MTCORE_LOAD_OPT == MTCORE_LOAD_OPT_COUNTING)
    uh_win->h_ops_counts = calloc(uh_nprocs, sizeof(int));
#endif

#if (MTCORE_LOAD_OPT == MTCORE_LOAD_BYTE_COUNTING)
    uh_win->h_bytes_counts = calloc(uh_nprocs, sizeof(int));
#endif

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
    uh_win->base_h_offsets[user_rank] += MTCORE_HELPER_SHARED_SG_SIZE * MTCORE_NUM_H;
    while (i < user_local_rank) {
        uh_win->base_h_offsets[user_rank] += user_local_sizes[i];       /* size in bytes */
        i++;
    }
    MTCORE_DBG_PRINT("[%d] local base_h_offsets = 0x%lx, base=%p\n", user_rank,
                     uh_win->base_h_offsets[user_rank], uh_win->base);

    /* -Receive the address of all the shared user buffers on Helper processes.
     * It is noted that all the helpers have the same offsets since it is a global continuous
     * shared memory across all local users and helpers. */
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

    /* Gather the handle of Helpers' win. User root is always rank num_h in user
     * root + helpers communicator */
    /* TODO:
     * How about use handler on user root ?
     * How to solve the case that different processes may have the same handler ? */
    if (user_local_rank == 0) {
        unsigned long tmp_send_buf;
        uh_win->h_win_handles = calloc(MTCORE_NUM_H + 1, sizeof(unsigned long));
        mpi_errno = PMPI_Gather(&tmp_send_buf, 1, MPI_UNSIGNED_LONG, uh_win->h_win_handles,
                                1, MPI_UNSIGNED_LONG, MTCORE_NUM_H, uh_win->ur_h_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    MTCORE_Cache_uh_win(uh_win->win, uh_win);

  fn_exit:

    if (user_local_sizes)
        free(user_local_sizes);
    if (tmp_gather_buf)
        free(tmp_gather_buf);

    return mpi_errno;

  fn_fail:

    /* Caching is the last possible error, so we do not need remove
     * cache here. */

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

    if (uh_win->ur_h_comm && uh_win->ur_h_comm != MPI_COMM_NULL)
        PMPI_Comm_free(&uh_win->ur_h_comm);
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

#if (MTCORE_LOAD_OPT != MTCORE_LOAD_OPT_NON)
    if (uh_win->is_main_lock_granted)
        free(uh_win->is_main_lock_granted);
    if (uh_win->order_h_ranks_in_uh)
        free(uh_win->order_h_ranks_in_uh);
#endif
#if (MTCORE_LOAD_OPT == MTCORE_LOAD_OPT_COUNTING)
    if (uh_win->h_ops_counts)
        free(uh_win->h_ops_counts);
#endif
#if (MTCORE_LOAD_OPT == MTCORE_LOAD_BYTE_COUNTING)
    if (uh_win->h_bytes_counts)
        free(uh_win->h_bytes_counts);
#endif

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
    if (uh_win->h_win_handles)
        free(uh_win->h_win_handles);
    if (uh_win->uh_wins)
        free(uh_win->uh_wins);
    if (uh_win)
        free(uh_win);

    *win = MPI_WIN_NULL;
    *base_pp = NULL;

    goto fn_exit;
}

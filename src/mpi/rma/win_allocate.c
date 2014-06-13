#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

/**
 * TODO: should implement table[win_handle : ua_win object]
 */
MPIASP_Win *ua_win_table[2];

static int gather_user_ranks(MPI_Comm user_comm, MPIASP_Win * win)
{
    int mpi_errno = MPI_SUCCESS;
    int world_rank, world_nprocs, user_nprocs, user_rank;
    int user_world_rank, user_world_nprocs;

    PMPI_Comm_size(user_comm, &user_nprocs);
    PMPI_Comm_rank(user_comm, &user_rank);
    PMPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    PMPI_Comm_rank(MPIASP_COMM_USER_WORLD, &user_world_rank);

    win->user_ranks_in_world = calloc(user_nprocs, sizeof(int));
    win->user_ranks_in_user_world = calloc(user_nprocs, sizeof(int));
    win->user_ranks_in_world[user_rank] = world_rank;
    win->user_ranks_in_user_world[user_rank] = user_world_rank;

    /*PREF_TODO : translate instead */
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                               win->user_ranks_in_world, 1, MPI_INT, user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                               win->user_ranks_in_user_world, 1, MPI_INT, user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    return mpi_errno;

  fn_fail:
    if (win->user_ranks_in_world)
        free(win->user_ranks_in_world);
    if (win->user_ranks_in_user_world)
        free(win->user_ranks_in_user_world);

    win->user_ranks_in_world = NULL;
    win->user_ranks_in_user_world = NULL;

    goto fn_exit;
}

static int create_ua_comm(MPI_Comm user_comm, int ua_tag, MPIASP_Win * win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_nprocs, user_rank, world_nprocs, user_world_rank, user_local_rank;
    int *ua_ranks_in_world = NULL, *asp_info = NULL;
    int *asp_exists_bitmap = NULL;
    int i, j, num_ua_ranks, asp_rank;

    PMPI_Comm_size(user_comm, &user_nprocs);
    PMPI_Comm_rank(user_comm, &user_rank);
    PMPI_Comm_size(MPI_COMM_WORLD, &world_nprocs);
    PMPI_Comm_rank(win->local_user_comm, &user_local_rank);

    // maximum amount equals to world size
    ua_ranks_in_world = calloc(world_nprocs, sizeof(int));
    asp_exists_bitmap = calloc(world_nprocs, sizeof(int));

    for (i = 0; i < user_nprocs; i++) {
        ua_ranks_in_world[i] = win->user_ranks_in_world[i];
    }

    // -Get the unique ASP ranks for all the USER ranks
    for (i = 0; i < user_nprocs; i++) {
        user_world_rank = win->user_ranks_in_user_world[i];

        asp_rank = MPIASP_ALL_ASP_IN_COMM_WORLD[user_world_rank];
        asp_exists_bitmap[asp_rank] = 1;
    }
    num_ua_ranks = user_nprocs;
    for (i = 0; i < world_nprocs; i++) {
        if (asp_exists_bitmap[i])
            ua_ranks_in_world[num_ua_ranks++] = i;

        if (num_ua_ranks > world_nprocs) {
            MPIASP_ERR_PRINT("[%d] Wrong numer of UA ranks %d > %d\n",
                             user_rank, num_ua_ranks, world_nprocs);
            mpi_errno = -1;
            goto fn_fail;
        }
    }

    // -Send gathered UA ranks to ASP process
    if (user_local_rank == 0) {
        // --First send the number of UA ranks
        mpi_errno = PMPI_Send(&num_ua_ranks, 1, MPI_INT,
                              MPIASP_RANK_IN_COMM_LOCAL, ua_tag, MPIASP_COMM_LOCAL);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
        // --Send UA ranks
        mpi_errno = PMPI_Send(ua_ranks_in_world, num_ua_ranks, MPI_INT,
                              MPIASP_RANK_IN_COMM_LOCAL, ua_tag, MPIASP_COMM_LOCAL);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    // -Create communicator
    PMPI_Group_incl(MPIASP_GROUP_WORLD, num_ua_ranks, ua_ranks_in_world, &win->ua_group);
    mpi_errno = PMPI_Comm_create_group(MPI_COMM_WORLD, win->ua_group, 0, &win->ua_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    if (ua_ranks_in_world)
        free(ua_ranks_in_world);
    if (asp_exists_bitmap)
        free(asp_exists_bitmap);

    return mpi_errno;

  fn_fail:
    if (win->ua_comm != MPI_COMM_NULL)
        PMPI_Comm_free(&win->ua_comm);
    if (win->ua_group != MPI_GROUP_NULL)
        PMPI_Group_free(&win->ua_group);
    win->ua_comm = MPI_COMM_NULL;
    win->ua_group = MPI_GROUP_NULL;

    goto fn_exit;
}

static int create_ua_local_comm(MPI_Comm user_local_comm, int ua_tag, MPIASP_Win * win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_local_rank, user_local_nprocs;
    int *ua_ranks_in_local = NULL;
    int i;

    PMPI_Comm_size(user_local_comm, &user_local_nprocs);
    PMPI_Comm_rank(user_local_comm, &user_local_rank);

    /*PERF_TODO: translate instead */
    // -Gather rank of local USER processes and local ASP in COMM_LOCAL
    ua_ranks_in_local = calloc(user_local_nprocs + MPIASP_NUM_ASP_IN_LOCAL, sizeof(int));
    PMPI_Comm_rank(MPIASP_COMM_LOCAL, &ua_ranks_in_local[user_local_rank]);

    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                               ua_ranks_in_local, 1, MPI_INT, user_local_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    ua_ranks_in_local[user_local_nprocs] = MPIASP_RANK_IN_COMM_LOCAL;

    // -Send USER ranks to ASP process
    if (user_local_rank == 0) {
        mpi_errno = PMPI_Send(ua_ranks_in_local, user_local_nprocs, MPI_INT,
                              MPIASP_RANK_IN_COMM_LOCAL, ua_tag, MPIASP_COMM_LOCAL);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    // -Create communicator
    PMPI_Group_incl(MPIASP_GROUP_LOCAL,
                    user_local_nprocs + MPIASP_NUM_ASP_IN_LOCAL, ua_ranks_in_local,
                    &win->local_ua_group);
    mpi_errno = PMPI_Comm_create_group(MPIASP_COMM_LOCAL, win->local_ua_group,
                                       0, &win->local_ua_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    if (ua_ranks_in_local)
        free(ua_ranks_in_local);

    return mpi_errno;

  fn_fail:
    if (win->local_ua_group != MPI_GROUP_NULL)
        PMPI_Group_free(&win->local_ua_group);
    if (win->local_ua_comm != MPI_COMM_NULL)
        PMPI_Comm_free(&win->local_ua_comm);
    win->local_ua_group = MPI_GROUP_NULL;
    win->local_ua_comm = MPI_COMM_NULL;

    goto fn_exit;
}

int MPI_Win_allocate(MPI_Aint size, int disp_unit, MPI_Info info,
                     MPI_Comm user_comm, void *baseptr, MPI_Win * win)
{
    static const char FCNAME[] = "MPI_Win_allocate";
    int mpi_errno = MPI_SUCCESS;
    MPI_Group ua_group;
    int ua_rank, user_nprocs, user_rank, user_world_rank,
        user_local_rank, user_local_nprocs, ua_local_rank, ua_local_nprocs;
    MPIASP_Win *ua_win;
    int ua_tag;
    int i, j;
    int func_params[1];
    void **base_pp = (void **) baseptr;
    MPI_Status stat;
    int *asp_info = NULL;
    MPI_Aint *user_local_sizes;

#ifdef DEBUG
    int ua_nprocs;
#endif

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
    ua_win->asp_ranks_in_ua = calloc(user_nprocs, sizeof(int));
    user_local_sizes = calloc(user_local_nprocs, sizeof(MPI_Aint));

    /* Gather disp_unit, used when send RMA operations */
    ua_win->disp_units[user_rank] = disp_unit;
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                               ua_win->disp_units, 1, MPI_INT, user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Gather size from all local user processes,
     * used for calculating the ASP offset of shared buffers  */
    user_local_sizes[user_local_rank] = size;
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                               user_local_sizes, 1, MPI_AINT, ua_win->local_user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Gather user rank information */
    mpi_errno = gather_user_ranks(user_comm, ua_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Ask ASP start function */
    mpi_errno = MPIASP_Tag_format((int) user_comm, &ua_tag);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    MPIASP_Func_start(MPIASP_FUNC_WIN_ALLOCATE, user_local_nprocs, ua_tag, ua_win->local_user_comm);

    /* Send parameters to ASP */
    func_params[0] = (user_comm == MPIASP_COMM_USER_WORLD);
    MPIASP_Func_set_param((char *) func_params, sizeof(func_params), ua_tag,
                          ua_win->local_user_comm);

    /* Allocate a shared window with local ASP */
    MPIASP_DBG_PRINT("[%d] ---- Start allocating shared window\n", user_rank);

    // -Get communicator including local USER processes and local ASP
    if (user_comm == MPIASP_COMM_USER_WORLD) {
        ua_win->local_ua_comm = MPIASP_COMM_LOCAL;
        PMPI_Comm_group(ua_win->local_ua_comm, &ua_win->local_ua_group);
    }
    else {
        mpi_errno = create_ua_local_comm(ua_win->local_user_comm, ua_tag, ua_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    PMPI_Comm_rank(ua_win->local_ua_comm, &ua_local_rank);
    PMPI_Comm_size(ua_win->local_ua_comm, &ua_local_nprocs);

    MPIASP_DBG_PRINT("[%d] Created local_ua_comm, %d/%d\n", user_rank,
                     ua_local_rank, ua_local_nprocs);

    // -Allocate shared window
    mpi_errno = PMPI_Win_allocate_shared(size, disp_unit, info,
                                         ua_win->local_ua_comm, &ua_win->base,
                                         &ua_win->local_ua_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    // -Calculate the offset of local shared buffer
    i = 0;
    ua_win->base_asp_offset[user_rank] = 0;
    while (i < user_local_rank) {
        ua_win->base_asp_offset[user_rank] += user_local_sizes[i];      // size in bytes
        i++;
    }
    MPIASP_DBG_PRINT("[%d] local base_asp_offset = 0x%lx, base=%p\n", user_rank,
                     ua_win->base_asp_offset[user_rank], ua_win->base);

    // -Receive the address of all the shared user buffers on ASP processes
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                               ua_win->base_asp_offset, 1, MPI_AINT, user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

#ifdef DEBUG
    for (i = 0; i < user_nprocs; i++) {
        MPIASP_DBG_PRINT("[%d] base_asp_offset[%d] = 0x%lx\n", user_rank, i,
                         ua_win->base_asp_offset[i]);
    }
#endif

    /* Create a window including all USER processes and ASP processes
     * using shared buffers. */
    MPIASP_DBG_PRINT("[%d] ---- Start create UA window\n", user_rank);

    // -Get the communicator including all USER processes + ASP processes
    if (user_comm == MPIASP_COMM_USER_WORLD) {
        ua_win->ua_comm = MPI_COMM_WORLD;
    }
    else {
        mpi_errno = create_ua_comm(user_comm, ua_tag, ua_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

#ifdef DEBUG
    PMPI_Comm_size(ua_win->ua_comm, &ua_nprocs);
    PMPI_Comm_rank(ua_win->ua_comm, &ua_rank);
    MPIASP_DBG_PRINT("[%d] Created ua_comm, %d/%d\n", user_rank, ua_rank, ua_nprocs);
#endif

    // -Get the rank of ASP processes in USER + ASP communicator */
    PMPI_Comm_group(ua_win->ua_comm, &ua_win->ua_group);
    mpi_errno = PMPI_Group_translate_ranks(MPIASP_GROUP_WORLD,
                                           MPIASP_NUM_ASP_IN_LOCAL,
                                           &MPIASP_RANK_IN_COMM_WORLD, ua_win->ua_group,
                                           &ua_win->asp_ranks_in_ua[user_rank]);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                               ua_win->asp_ranks_in_ua, 1, MPI_INT, user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

#ifdef DEBUG
    for (i = 0; i < user_nprocs; i++) {
        MPIASP_DBG_PRINT("[%d]    asp_ranks_in_ua[%d] = %d\n", user_rank, i,
                         ua_win->asp_ranks_in_ua[i]);
    }
#endif

    // -Create window
    // - Internal window for accessing to helpers
    mpi_errno = PMPI_Win_create(ua_win->base, size, disp_unit, info,
                                ua_win->ua_comm, &ua_win->ua_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    // - Only expose user window in order to hide helpers in all non-wrapped window functions
    mpi_errno = PMPI_Win_create(ua_win->base, size, disp_unit, info,
                                ua_win->user_comm, &ua_win->win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    MPIASP_DBG_PRINT("[%d] Created window 0x%x, ua_window 0x%x\n",
                     user_rank, ua_win->win, ua_win->ua_win);

    *win = ua_win->win;
    *base_pp = ua_win->base;

    if (user_local_rank == 0) {
        // Receive the handle of ASP win
        mpi_errno = PMPI_Recv(&ua_win->asp_win_handle, 1, MPI_INT,
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
    if (ua_win->ua_win)
        PMPI_Win_free(&ua_win->ua_win);

    if (ua_win->local_ua_comm && ua_win->local_ua_comm != MPIASP_COMM_LOCAL)
        PMPI_Comm_free(&ua_win->local_ua_comm);
    if (ua_win->ua_comm && ua_win->ua_comm != MPI_COMM_WORLD)
        PMPI_Comm_free(&ua_win->ua_comm);
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
    if (ua_win->user_ranks_in_user_world)
        free(ua_win->user_ranks_in_user_world);
    if (ua_win->user_ranks_in_world)
        free(ua_win->user_ranks_in_world);
    if (ua_win->asp_ranks_in_ua)
        free(ua_win->asp_ranks_in_ua);
    if (ua_win)
        free(ua_win);

    *win = MPI_WIN_NULL;
    *base_pp = NULL;

    goto fn_exit;
}

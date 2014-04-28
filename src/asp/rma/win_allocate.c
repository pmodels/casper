#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "asp.h"

#undef FUNCNAME
#define FUNCNAME ASP_Win_allocate

/**
 * TODO: should implement table[user_win_handle : asp_win object]
 */
ASP_Win *asp_win_table[2];

static int create_ua_comm(int user_local_root, int user_tag, MPI_Comm *ua_comm) {
    int mpi_errno = MPI_SUCCESS;
    int world_nprocs, world_rank;
    int *ua_ranks_in_world = NULL;
    MPI_Group ua_group = MPI_GROUP_NULL, world_group = MPI_GROUP_NULL;
    MPI_Status status;
    int dst, num_ua_ranks;

    PMPI_Comm_size(MPI_COMM_WORLD, &world_nprocs);
    PMPI_Comm_group(MPI_COMM_WORLD, &world_group);

    ua_ranks_in_world = calloc(world_nprocs, sizeof(int));

    // --Receive the number of UA ranks
    mpi_errno = PMPI_Recv(&num_ua_ranks, 1, MPI_INT,
            user_local_root, user_tag, MPI_COMM_WORLD, &status);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    // --Receive UA ranks
    mpi_errno = PMPI_Recv(ua_ranks_in_world, num_ua_ranks, MPI_INT,
            user_local_root, user_tag, MPI_COMM_WORLD, &status);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

#ifdef DEBUG
    char outputs[256];
    char *outputs_ptr = &outputs[0];
    memset(outputs, 0, sizeof(outputs));
    for (dst = 0; dst < num_ua_ranks; dst++) {
        sprintf(outputs_ptr, "%2d ", ua_ranks_in_world[dst]);
        outputs_ptr += 3;
    }
    ASP_DBG_PRINT(" Received user ranks in world: %s\n", outputs);
#endif

    // -Create communicator
    PMPI_Group_incl(world_group, num_ua_ranks, ua_ranks_in_world, &ua_group);
    mpi_errno = PMPI_Comm_create_group(MPI_COMM_WORLD, ua_group, 0, ua_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    fn_exit:
    if (ua_ranks_in_world)
        free(ua_ranks_in_world);
    if (world_group != MPI_GROUP_NULL)
        PMPI_Group_free(&world_group);
    if (ua_group != MPI_GROUP_NULL)
        PMPI_Group_free(&ua_group);

    return mpi_errno;

    fn_fail:
    if (*ua_comm)
        PMPI_Comm_free(ua_comm);
    goto fn_exit;
}

static int create_ua_local_comm(int user_local_root, int user_local_nprocs,
        int user_tag, MPI_Comm *ua_local_comm) {
    int mpi_errno = MPI_SUCCESS;
    int *ua_ranks_in_local = NULL;
    MPI_Group local_group = MPI_GROUP_NULL, local_ua_group = MPI_GROUP_NULL;
    MPI_Status status;

    PMPI_Comm_group(MPIASP_COMM_LOCAL, &local_group);

    // -Receive rank of local USER processes in COMM_LOCAL
    ua_ranks_in_local = calloc(user_local_nprocs + MPIASP_NUM_ASP_IN_LOCAL,
            sizeof(int));
    mpi_errno = PMPI_Recv(ua_ranks_in_local, user_local_nprocs, MPI_INT,
            user_local_root, user_tag, MPIASP_COMM_LOCAL, &status);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    ua_ranks_in_local[user_local_nprocs] = MPIASP_RANK_IN_COMM_LOCAL;

    // -Create communicator
    PMPI_Group_incl(local_group, user_local_nprocs + MPIASP_NUM_ASP_IN_LOCAL,
            ua_ranks_in_local, &local_ua_group);
    mpi_errno = PMPI_Comm_create_group(MPIASP_COMM_LOCAL, local_ua_group, 0,
            ua_local_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    fn_exit:
    if (ua_ranks_in_local)
        free(ua_ranks_in_local);
    if (local_group != MPI_GROUP_NULL)
        PMPI_Group_free(&local_group);
    if (local_ua_group != MPI_GROUP_NULL)
        PMPI_Group_free(&local_ua_group);

    return mpi_errno;

    fn_fail:
    if (*ua_local_comm)
        PMPI_Comm_free(ua_local_comm);
    goto fn_exit;
}

int ASP_Win_allocate(int user_local_root, int user_local_nprocs, int user_tag) {
    int mpi_errno = MPI_SUCCESS;
    MPI_Status status;
    int dst, local_ua_rank, local_ua_nprocs;
    MPI_Aint r_size, size;
    int r_disp_unit, req_idx;
    int ua_nprocs, ua_rank;
    ASP_Win *win;
    void **user_bases = NULL;
    int *ua_ranks_in_world = NULL;

    int func_params[1];
    int is_user_world;
    MPI_Request *reqs = NULL;
    MPI_Status *stats = NULL;

    win = calloc(1, sizeof(ASP_Win));
    win->user_base_addrs_in_local = calloc(user_local_nprocs, sizeof(MPI_Aint));
    ua_ranks_in_world = calloc(user_local_nprocs + 1, sizeof(int));
    user_bases = calloc(user_local_nprocs, sizeof(void*));

    /* Gather parameters from user processes, because each process may
     * specify different size, disp_unit */
    mpi_errno = MPIASP_Func_get_param((char*) func_params, sizeof(func_params),
            user_local_root, user_tag);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    is_user_world = func_params[0];

    ASP_DBG_PRINT(
            " Received parameters from %d: is_user_world %d\n",
            user_local_root, is_user_world);

    /* Allocate a shared window with local USER processes */
    ASP_DBG_PRINT(" ---- Start allocating shared window\n");

    // -Get communicator including local USER processes and local ASP
    if (is_user_world) {
        win->local_ua_comm = MPIASP_COMM_LOCAL;
    }
    else {
        mpi_errno = create_ua_local_comm(user_local_root, user_local_nprocs,
                user_tag, &win->local_ua_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    // -Allocate shared window in CHAR type
    // (No local buffer, only need shared buffer on user processes)
    mpi_errno = PMPI_Win_allocate_shared(0, 1, MPI_INFO_NULL,
            win->local_ua_comm, &win->base, &win->local_ua_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    ASP_DBG_PRINT(" Created local_ua_win, base=%p\n", win->base);

    // -Query address of user buffers and send to USER processes
#ifdef DEBUG
    PMPI_Comm_rank(win->local_ua_comm, &local_ua_rank);
    PMPI_Comm_size(win->local_ua_comm, &local_ua_nprocs);
    for (dst = 0; dst < local_ua_nprocs; dst++) {
        mpi_errno = PMPI_Win_shared_query(win->local_ua_win, dst, &r_size,
                &r_disp_unit, &user_bases[dst]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        PMPI_Get_address(user_bases[dst],
                &win->user_base_addrs_in_local[dst]);
        ASP_DBG_PRINT(
                "   shared base[%d]=%p, addr 0x%lx, offset 0x%lx"
                ", r_size %ld, r_unit %d\n"
                , dst, user_bases[dst], win->user_base_addrs_in_local[dst],
                (unsigned long)(user_bases[dst] - win->base), r_size, r_disp_unit);

        size += r_size; // size in byte
    }
#endif

    /* Create a window including all USER processes and ASP processes */
    ASP_DBG_PRINT(" ---- Start create UA window\n");

    // -Get the communicator including all USER processes + ASP processes
    if (is_user_world) {
        win->ua_comm = MPI_COMM_WORLD;
    } else {
        mpi_errno = create_ua_comm(user_local_root, user_tag, &win->ua_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    PMPI_Comm_size(win->ua_comm, &ua_nprocs);
    PMPI_Comm_rank(win->ua_comm, &ua_rank);
    ASP_DBG_PRINT(" Created ua_comm, ua_rank %d, ua_nprocs %d\n",
            ua_rank, ua_nprocs);

    // -Create win in CHAR type, size = total size of shared buffers
    mpi_errno = PMPI_Win_create(win->base, size, 1, MPI_INFO_NULL, win->ua_comm,
            &win->win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    mpi_errno = put_asp_win((int)win->win, win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    ASP_DBG_PRINT(" Created ASP window 0x%x\n", win->win);

    // Notify user root the handle of ASP win
    mpi_errno = PMPI_Send(&win->win, 1, MPI_INT,
            user_local_root, user_tag, MPIASP_COMM_LOCAL);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    fn_exit:

    if (ua_ranks_in_world)
        free(ua_ranks_in_world);
    if (user_bases)
        free(user_bases);
    if (reqs)
        free(reqs);
    if (stats)
        free(stats);

    return mpi_errno;

    fn_fail:

    if (win->local_ua_win)
        PMPI_Win_free(&win->local_ua_win);
    if (win->win)
        PMPI_Win_free(&win->win);

    if (win->local_ua_comm && win->local_ua_comm != MPIASP_COMM_LOCAL)
        PMPI_Comm_free(&win->local_ua_comm);
    if (win->ua_comm && win->ua_comm != MPI_COMM_WORLD)
        PMPI_Comm_free(&win->ua_comm);

    if (win->user_base_addrs_in_local)
        free(win->user_base_addrs_in_local);
    if (win)
        free(win);

    goto fn_exit;
}

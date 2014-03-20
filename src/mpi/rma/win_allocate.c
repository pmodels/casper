#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

/**
 * TODO: should implement table[win_handle : ua_win object]
 */
MPIASP_Win *ua_win_table[2];

int MPI_Win_allocate(MPI_Aint size, int disp_unit, MPI_Info info,
        MPI_Comm user_comm, void *baseptr, MPI_Win *win) {
    static const char FCNAME[] = "MPIASP_Win_allocate";
    int mpi_errno = MPI_SUCCESS;
    MPI_Group ua_group, shrd_group, world_group;
    int shrd_ranks[2], *ua_ranks_in_world;
    int dst;
    int ua_nprocs, ua_rank, user_nprocs, user_rank;
    MPIASP_Win *ua_win;
    int ua_tag;
    int func_params[2];
    void **base_pp = (void **) baseptr;

    MPIASP_DBG_PRINT_FCNAME();

    ua_tag = MPIASP_Tag_format((int)user_comm);
    if(ua_tag < 0){
        goto fn_fail;
    }

    PMPI_Comm_size(user_comm, &user_nprocs);
    PMPI_Comm_rank(user_comm, &user_rank);
    PMPI_Comm_group(MPI_COMM_WORLD, &world_group);

    if (user_rank == 0) {
        MPIASP_Func_start(MPIASP_FUNC_WIN_ALLOCATE, user_nprocs, ua_tag);
    }

    ua_win = calloc(1, sizeof(MPIASP_Win));
    ua_win->base_asp_addrs = calloc(user_nprocs, sizeof(MPI_Aint));
    ua_win->base_addrs = calloc(user_nprocs, sizeof(MPI_Aint));
    ua_win->disp_units = calloc(user_nprocs, sizeof(int));
    ua_win->sizes = calloc(user_nprocs, sizeof(MPI_Aint));
    ua_ranks_in_world = calloc(user_nprocs + 1, sizeof(int));
    ua_win->user_comm = user_comm;

    /*
     * Send parameters to ASP
     */
    if (user_rank == 0) {
        func_params[0] = size;
        func_params[1] = disp_unit;
        mpi_errno = PMPI_Send(func_params, 2, MPI_INT,
                MPIASP_RANK_IN_COMM_WORLD, ua_tag, MPI_COMM_WORLD);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /*
     * Get the communicator including all USER processes + ASP
     */
    // -Gather rank of user processes in COMM_WORLD
    PMPI_Comm_rank(MPI_COMM_WORLD, &ua_ranks_in_world[user_rank]);
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
            ua_ranks_in_world, 1, MPI_INT, user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    ua_ranks_in_world[user_nprocs] = MPIASP_RANK_IN_COMM_WORLD;

    // -Send user world ranks to ASP
    if (user_rank == 0) {
        mpi_errno = PMPI_Send(ua_ranks_in_world, user_nprocs, MPI_INT,
                MPIASP_RANK_IN_COMM_WORLD, ua_tag, MPI_COMM_WORLD);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    // -Create communicator
    PMPI_Group_incl(world_group, user_nprocs + 1, ua_ranks_in_world, &ua_group);
    mpi_errno = PMPI_Comm_create_group(MPI_COMM_WORLD, ua_group, 0,
            &ua_win->ua_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    PMPI_Comm_size(ua_win->ua_comm, &ua_nprocs);
    PMPI_Comm_rank(ua_win->ua_comm, &ua_rank);

    MPIASP_DBG_PRINT(
            "[%d] Created ua_comm, ua_rank %d, ua_nprocs %d\n", user_rank, ua_rank, ua_nprocs);

    /*
     * Allocate a shared window with ASP
     */
    // -Create the communicator only including local process and ASP
    shrd_ranks[0] = ua_ranks_in_world[user_rank];
    shrd_ranks[1] = MPIASP_RANK_IN_COMM_WORLD;

    PMPI_Group_incl(world_group, 2, shrd_ranks, &shrd_group);
    mpi_errno = PMPI_Comm_create_group(MPI_COMM_WORLD, shrd_group, 0,
            &ua_win->shrd_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    // -Allocate shared window
    mpi_errno = PMPI_Win_allocate_shared(size, disp_unit, info,
            ua_win->shrd_comm, &ua_win->base, &ua_win->shrd_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    PMPI_Get_address(ua_win->base, &ua_win->base_addrs[user_rank]);

    MPIASP_DBG_PRINT(
            "[%d]Created local shared winbuf = %p\n", user_rank, ua_win->base);

    /*
     * Receive the address of all the shared user buffers on ASP
     */
    mpi_errno = PMPI_Bcast(ua_win->base_asp_addrs, user_nprocs, MPI_AINT,
            MPIASP_RANK_IN_COMM_WORLD, ua_win->ua_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

#ifdef DEBUG
    if (user_rank == 0) {
        for (dst = 0; dst < user_nprocs; dst++) {
            MPIASP_DBG_PRINT(
                    "[%d] base_asp_addrs[%d] = 0x%lx\n", user_rank, dst, ua_win->base_asp_addrs[dst]);
        }
    }
#endif

    /*
     * Gather the base addresses on all user processes
     */
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
            ua_win->base_addrs, 1, MPI_AINT, user_comm);

#ifdef DEBUG
    if (user_rank == 0) {
        for (dst = 0; dst < user_nprocs; dst++) {
            MPIASP_DBG_PRINT(
                    "[%d] base_addrs[%d] = 0x%lx\n", user_rank, dst, ua_win->base_addrs[dst]);
        }
    }
#endif

    /**
     * TODO: How to set disp_unit and size in ua_win
     * PMPI_Win_create_dynamic creates window with base=MPI_BOTTOM, size=0, disp_unit=1
     */
    /*
     * Gather the disp_unit and size on all user processes
     */
    ua_win->disp_units[user_rank] = disp_unit;
    ua_win->sizes[user_rank] = size;
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
            ua_win->disp_units, 1, MPI_INT, user_comm);
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
            ua_win->sizes, 1, MPI_LONG, user_comm);

    /*
     * Get the rank of ASP in user+asp communicator
     */
    int rank1[1] = { MPIASP_RANK_IN_COMM_WORLD };
    int rank2[1];
    mpi_errno = PMPI_Group_translate_ranks(world_group, 1, rank1, ua_group,
            rank2);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    ua_win->asp_rank = rank2[0];
    MPIASP_DBG_PRINT( "[%d] asp_rank = %d\n", user_rank, ua_win->asp_rank);

    /*
     * Create a window including user processes and ASP;
     * Attach the local shared buffer as;
     * the same behavior as MPI_Win_create does, but create_dynamic need a collective call
     */
    mpi_errno = PMPI_Win_create_dynamic(info, ua_win->ua_comm, &ua_win->win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    mpi_errno = PMPI_Win_attach(ua_win->win, ua_win->base, size);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    *win = ua_win->win;
    *base_pp = ua_win->base;

    put_ua_win(*win, ua_win);

    fn_exit:

    PMPI_Group_free(&world_group);
    PMPI_Group_free(&ua_group);
    PMPI_Group_free(&shrd_group);

    if (ua_ranks_in_world)
        free(ua_ranks_in_world);

    return mpi_errno;

    fn_fail:

    if (ua_win->shrd_win)
        PMPI_Win_free(&ua_win->shrd_win);
    if (ua_win->win)
        PMPI_Win_free(&ua_win->win);

    if (ua_win->shrd_comm)
        PMPI_Comm_free(&ua_win->shrd_comm);
    if (ua_win->ua_comm)
        PMPI_Comm_free(&ua_win->ua_comm);

    if (ua_win->base_asp_addrs)
        free(ua_win->base_asp_addrs);
    if (ua_win->base_addrs)
        free(ua_win->base_addrs);
    if (ua_win->disp_units)
        free(ua_win->disp_units);
    if (ua_win->sizes)
        free(ua_win->sizes);
    if (ua_win)
        free(ua_win);

    goto fn_exit;
}

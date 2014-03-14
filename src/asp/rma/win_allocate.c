#include <stdio.h>
#include <stdlib.h>
#include "asp.h"

#undef FUNCNAME
#define FUNCNAME ASP_Win_allocate

/**
 * TODO: should implement table[user_win_handle : asp_win object]
 */
ASP_Win *asp_win_table[2];

inline ASP_Win* get_asp_win(int handle) {
    return asp_win_table[0];
}
inline void put_asp_win(int handle, ASP_Win* win) {
    asp_win_table[0] = win;
}

int ASP_Win_allocate(int user_root, int user_nprocs, int user_tag) {
    int mpi_errno = MPI_SUCCESS;
    MPI_Status status;
    MPI_Group world_group, ua_group, shrd_group;
    int shrd_ranks[2], *ua_ranks_in_world = NULL;
    int dst;
    MPI_Aint r_size;
    int r_disp_unit;
    int ua_nprocs, ua_rank;
    void *winbuf;
    ASP_Win *win;
    void **shrd_winbufs = NULL;

    int func_params[2];
    int size, disp_unit;

    PMPI_Comm_group(MPI_COMM_WORLD, &world_group);

    win = calloc(1, sizeof(ASP_Win));
    win->all_shrd_comms = calloc(user_nprocs, sizeof(MPI_Comm));
    win->all_shrd_wins = calloc(user_nprocs, sizeof(MPI_Win));
    win->all_shrd_base_addrs = calloc(user_nprocs, sizeof(MPI_Aint));
    ua_ranks_in_world = calloc(user_nprocs + 1, sizeof(int));
    shrd_winbufs = calloc(user_nprocs, sizeof(void*));

    /*
     * Receive parameters from user root
     */
    mpi_errno = PMPI_Recv(func_params, 2, MPI_INT, user_root, user_tag,
            MPI_COMM_WORLD, &status);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    size = func_params[0];
    disp_unit = func_params[1];

    MPIASP_DBG_PRINT(
            "[ASP] Received parameters from %d: size %d, disp_unit %d\n", user_root, size, disp_unit);

    /*
     * Get the communicator including all user processes + ASP
     */
    // -Receive the rank of user processes
    mpi_errno = PMPI_Recv(ua_ranks_in_world, user_nprocs, MPI_INT, user_root,
            user_tag, MPI_COMM_WORLD, &status);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    ua_ranks_in_world[user_nprocs] = MPIASP_RANK_IN_COMM_WORLD;

#ifdef DEBUG
    char outputs[256];
    char *outputs_ptr = &outputs[0];
    memset(outputs, 0, sizeof(outputs));
    for (dst = 0; dst < user_nprocs; dst++) {
        sprintf(outputs_ptr, "%2d ", ua_ranks_in_world[dst]);
        outputs += 3;
    }
    MPIASP_DBG_PRINT("[ASP] Received user ranks in world: %s\n", outputs);
#endif

    // -Create communicator
    PMPI_Group_incl(world_group, user_nprocs + 1, ua_ranks_in_world, &ua_group);
    mpi_errno = PMPI_Comm_create_group(MPI_COMM_WORLD, ua_group, 0,
            &win->ua_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    PMPI_Comm_size(win->ua_comm, &ua_nprocs);
    PMPI_Comm_rank(win->ua_comm, &ua_rank);

    MPIASP_DBG_PRINT(
            "[ASP] Created ua_comm, ua_rank %d, ua_nprocs\n", ua_rank, ua_nprocs);

    /*
     * Allocate shared buffers with all the user processes
     */
    for (dst = 0; dst < user_nprocs; dst++) {

        // -Create the communicator only including a user process and ASP
        shrd_ranks[0] = dst;
        shrd_ranks[1] = rank;

        PMPI_Group_incl(world_group, 2, shrd_ranks, &shrd_group);
        mpi_errno = PMPI_Comm_create_group(MPI_COMM_WORLD, shrd_group, 0,
                &win->all_shrd_comms[dst]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        // -Allocate shared window
        // No local buffer, only need shared buffer on user processes
        mpi_errno = PMPI_Win_allocate_shared(0, disp_unit, NULL, win->all_shrd_comms[dst],
                &winbuf, &win->all_shrd_wins[dst]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        // the rank of an user process in its shared communicator is always 0
        mpi_errno = PMPI_Win_shared_query(win->all_shrd_wins[dst], 0, &r_size, &r_disp_unit,
                &shrd_winbufs[dst]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        PMPI_Get_address(shrd_winbufs[dst], &win->all_shrd_base_addrs[dst]);

        PMPI_Group_free(&shrd_group);

        MPIASP_DBG_PRINT(
                "[ASP] Created shared base[%d]=%p, addr 0x%lx\n", dst, shrd_winbufs[dst], win->all_shrd_base_addrs[dst]);
    }

    // -Notify the address of all the shared user buffers on ASP
    mpi_errno = PMPI_Bcast(win->all_shrd_base_addrs, user_nprocs, MPI_AINT, ua_rank,
            win->ua_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /*
     * Create a window including user processes and ASP and attach all the shared buffers.
     */
    PMPI_Win_create_dynamic(NULL, win->ua_comm, &win->win);
    for (dst = 0; dst < user_nprocs; dst++) {
        PMPI_Win_attach(win->win, shrd_winbufs[dst], size);
    }

    put_asp_win(0, win);

    MPIASP_DBG_PRINT( "[ASP] Created ASP window 0x%lx\n", win->win);

    fn_exit:

    if (world_group)
        PMPI_Group_free(&world_group);
    if (ua_group)
        PMPI_Group_free(&ua_group);

    if (ua_ranks_in_world)
        free(ua_ranks_in_world);
    if (shrd_winbufs)
        free(shrd_winbufs);

    return mpi_errno;

    fn_fail:

    for (dst = 0; dst < user_nprocs; dst++) {
        if (win->all_shrd_wins[dst])
            PMPI_Win_free(win->all_shrd_wins[dst]);
        if (win->all_shrd_comms[dst])
            PMPI_Comm_free(win->all_shrd_comms[dst]);
    }
    if (win->win > 0)
        PMPI_Win_free(win->win);
    if (win->ua_comm)
        PMPI_Comm_free(win->ua_comm);

    if (win->all_shrd_base_addrs)
        free(win->all_shrd_base_addrs);
    if (win->all_shrd_comms)
        free(win->all_shrd_comms);
    if (win->all_shrd_wins)
        free(win->all_shrd_wins);
    if (win)
        free(win);

    goto fn_exit;
}

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cspg.h"

#undef FUNCNAME
#define FUNCNAME CSP_G_win_allocate

static int create_ug_comm(int user_nprocs, int *user_ranks_in_world, int num_ghosts,
                          int *gp_ranks_in_world, CSP_G_win * win)
{
    int mpi_errno = MPI_SUCCESS;
    int world_nprocs;
    int *ug_ranks_in_world = NULL;
    MPI_Group ug_group = MPI_GROUP_NULL;
    int num_ug_ranks;

    PMPI_Comm_size(MPI_COMM_WORLD, &world_nprocs);

    /* maximum amount equals to world size */
    ug_ranks_in_world = calloc(world_nprocs, sizeof(int));
    if (ug_ranks_in_world == NULL)
        goto fn_fail;

    /* -Create ug communicator including all USER processes and Ghost processes. */
    num_ug_ranks = user_nprocs + num_ghosts;
    CSP_G_assert(num_ug_ranks <= world_nprocs);
    memcpy(ug_ranks_in_world, gp_ranks_in_world, num_ghosts * sizeof(int));
    memcpy(&ug_ranks_in_world[num_ghosts], user_ranks_in_world, user_nprocs * sizeof(int));

#ifdef CSP_G_DEBUG
    int i;
    CSP_G_DBG_PRINT("ug_ranks_in_world: (nh %d, nu %d)\n", num_ghosts, user_nprocs);
    for (i = 0; i < num_ug_ranks; i++) {
        CSP_G_DBG_PRINT("[%d] %d \n", i, ug_ranks_in_world[i]);
    }
#endif

    /* -Create ug communicator. */
    PMPI_Group_incl(CSP_GROUP_WORLD, num_ug_ranks, ug_ranks_in_world, &ug_group);
    mpi_errno = PMPI_Comm_create_group(MPI_COMM_WORLD, ug_group, 0, &win->ug_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    if (ug_ranks_in_world)
        free(ug_ranks_in_world);
    if (ug_group != MPI_GROUP_NULL)
        PMPI_Group_free(&ug_group);

    return mpi_errno;

  fn_fail:
    if (win->ug_comm != MPI_COMM_NULL) {
        PMPI_Comm_free(&win->ug_comm);
        win->ug_comm = MPI_COMM_NULL;
    }
    goto fn_exit;
}

static int create_communicators(int user_nprocs, CSP_G_win * win)
{
    int mpi_errno = MPI_SUCCESS;
    int *func_params = NULL, func_param_size = 0;
    int is_user_world;
    int *user_ranks_in_world = NULL;
    int *gp_ranks_in_world = NULL, num_ghosts = 0, max_num_ghosts;

    user_ranks_in_world = calloc(user_nprocs, sizeof(int));
    max_num_ghosts = CSP_ENV.num_g * CSP_NUM_NODES;
    gp_ranks_in_world = calloc(max_num_ghosts, sizeof(int));
    func_param_size = user_nprocs + max_num_ghosts + 2;
    func_params = calloc(func_param_size, sizeof(int));

    mpi_errno = CSP_G_func_get_param((char *) func_params, sizeof(int) * func_param_size,
                                     win->ur_g_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Get parameters from local User_root
     *  [0]: is_comm_user_world */
    is_user_world = func_params[0];

    if (is_user_world) {
        CSP_G_DBG_PRINT(" Received parameters: is_user_world %d\n", is_user_world);

        /* Create communicators
         *  local_ug_comm: including local USER and Ghost processes
         *  ug_comm: including all USER and Ghost processes
         */
        win->local_ug_comm = CSP_COMM_LOCAL;
        win->ug_comm = MPI_COMM_WORLD;
    }
    else {

        /*  [2:N+1]: user ranks in comm_world
         *  [N+2:]: ghost ranks in comm_world
         */
        int pidx;
        num_ghosts = func_params[1];
        pidx = 2;
        memcpy(user_ranks_in_world, &func_params[pidx], sizeof(int) * user_nprocs);
        pidx += user_nprocs;
        memcpy(gp_ranks_in_world, &func_params[pidx], sizeof(int) * num_ghosts);

        CSP_G_DBG_PRINT(" Received parameters: is_user_world %d, num_ghosts %d\n",
                        is_user_world, num_ghosts);

        /* Create communicators
         *  ug_comm: including all USER and Ghost processes
         *  local_ug_comm: including local USER and Ghost processes
         */
        mpi_errno = create_ug_comm(user_nprocs, user_ranks_in_world, num_ghosts,
                                   gp_ranks_in_world, win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

#ifdef DEBUG
        {
            int ug_rank, ug_nprocs;
            PMPI_Comm_rank(win->ug_comm, &ug_rank);
            PMPI_Comm_size(win->ug_comm, &ug_nprocs);
            CSP_G_DBG_PRINT("created ug_comm, my rank %d/%d\n", ug_rank, ug_nprocs);
        }
#endif
        mpi_errno = PMPI_Comm_split_type(win->ug_comm, MPI_COMM_TYPE_SHARED, 0,
                                         MPI_INFO_NULL, &win->local_ug_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

#ifdef DEBUG
        {
            int ug_rank, ug_nprocs;
            PMPI_Comm_rank(win->local_ug_comm, &ug_rank);
            PMPI_Comm_size(win->local_ug_comm, &ug_nprocs);
            CSP_G_DBG_PRINT("created local_ug_comm, my rank %d/%d\n", ug_rank, ug_nprocs);
        }
#endif
    }

  fn_exit:
    if (func_params)
        free(func_params);
    if (user_ranks_in_world)
        free(user_ranks_in_world);
    if (gp_ranks_in_world)
        free(gp_ranks_in_world);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int create_lock_windows(MPI_Aint size, CSP_G_win * win)
{
    int mpi_errno = MPI_SUCCESS;
    int i;

    /* Need multiple windows for single lock synchronization */
    if (win->info_args.epoch_type & CSP_EPOCH_LOCK) {
        win->num_ug_wins = win->max_local_user_nprocs;
    }
    /* Need a single window for lock_all only synchronization */
    else if (win->info_args.epoch_type & CSP_EPOCH_LOCK_ALL) {
        win->num_ug_wins = 1;
    }

    win->ug_wins = calloc(win->num_ug_wins, sizeof(MPI_Win));
    for (i = 0; i < win->num_ug_wins; i++) {
        mpi_errno = PMPI_Win_create(win->base, size, 1, MPI_INFO_NULL,
                                    win->ug_comm, &win->ug_wins[i]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        CSP_G_DBG_PRINT(" Created ug windows[%d] 0x%x\n", i, win->ug_wins[i]);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}


int CSP_G_win_allocate(int user_local_root, int user_nprocs)
{
    int mpi_errno = MPI_SUCCESS;
    int dst, local_ug_rank, local_ug_nprocs;
    MPI_Aint r_size, size = 0;
    int r_disp_unit;
    int ug_nprocs, ug_rank;
    CSP_G_win *win;
    void **user_bases = NULL;
    int csp_buf_size = CSP_GP_SHARED_SG_SIZE;

    win = calloc(1, sizeof(CSP_G_win));

    /* Create user root + ghosts communicator for
     * internal information exchange between users and ghosts. */
    mpi_errno = CSP_G_func_new_ur_g_comm(user_local_root, &win->ur_g_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Create communicators
     *  ug_comm: including all USER and Ghost processes
     *  local_ug_comm: including local USER and Ghost processes
     */
    create_communicators(user_nprocs, win);

    PMPI_Comm_rank(win->local_ug_comm, &local_ug_rank);
    PMPI_Comm_size(win->local_ug_comm, &local_ug_nprocs);
    PMPI_Comm_size(win->ug_comm, &ug_nprocs);
    PMPI_Comm_rank(win->ug_comm, &ug_rank);
    CSP_G_DBG_PRINT(" Created ug_comm: %d/%d, local_ug_comm: %d/%d\n",
                    ug_rank, ug_nprocs, local_ug_rank, local_ug_nprocs);

    /* Allocate a shared window with local USER processes */

    if (local_ug_rank == 0) {
#ifdef CSP_ENABLE_GRANT_LOCK_HIDDEN_BYTE
        csp_buf_size = max(csp_buf_size, sizeof(CSP_GRANT_LOCK_DATATYPE));
#endif
        csp_buf_size = max(csp_buf_size, sizeof(int) * user_nprocs);
    }

    /* -Allocate shared window in CHAR type
     * (No local buffer, only need shared buffer on user processes) */
    mpi_errno = PMPI_Win_allocate_shared(csp_buf_size, 1, MPI_INFO_NULL,
                                         win->local_ug_comm, &win->base, &win->local_ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    CSP_G_DBG_PRINT(" Created local_ug_win, base=%p, size=%d\n", win->base, csp_buf_size);

    /* -Query address of user buffers and send to USER processes */
    user_bases = calloc(local_ug_nprocs, sizeof(void *));
    win->user_base_addrs_in_local = calloc(local_ug_nprocs, sizeof(MPI_Aint));

    for (dst = 0; dst < local_ug_nprocs; dst++) {
        mpi_errno = PMPI_Win_shared_query(win->local_ug_win, dst, &r_size,
                                          &r_disp_unit, &user_bases[dst]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        PMPI_Get_address(user_bases[dst], &win->user_base_addrs_in_local[dst]);
        CSP_G_DBG_PRINT("   shared base[%d]=%p, addr 0x%lx, offset 0x%lx"
                        ", r_size %ld, r_unit %d\n", dst, user_bases[dst],
                        win->user_base_addrs_in_local[dst],
                        (unsigned long) (user_bases[dst] - win->base), r_size, r_disp_unit);

        size += r_size; /* size in byte */
    }

    /* All ghosts create window starting from the baseptr of ghost 0, so users
     * can use the same offset for all ghosts*/

    /* FIXME: if size=0 and ghost rank > 0, base may be returned as 0x0.
     * Is it implementation specific ? What is the uniform solution ?
     * It is not wrong that simply use base[0] for creating window, because it is accessible. */
    win->base = user_bases[0];

    /* Create ug windows including all User and Ghost processes.
     * Every User process has a window used for permission check and accessing Ghosts.
     * User processes in different nodes can share a window.
     *  i.e., win[x] can be shared by processes whose local rank is x.
     */
    int func_params[2];
    mpi_errno = CSP_G_func_get_param((char *) func_params, sizeof(func_params), win->ur_g_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    win->max_local_user_nprocs = func_params[0];
    win->info_args.epoch_type = func_params[1];
    CSP_G_DBG_PRINT(" Received parameters: max_local_user_nprocs = %d, epoch_type=%d\n",
                    win->max_local_user_nprocs, win->info_args.epoch_type);

    /* - Create lock/lockall windows */
    if ((win->info_args.epoch_type & CSP_EPOCH_LOCK) ||
        (win->info_args.epoch_type & CSP_EPOCH_LOCK_ALL)) {

        mpi_errno = create_lock_windows(size, win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* - Create global active window */
    if ((win->info_args.epoch_type & CSP_EPOCH_FENCE) ||
        (win->info_args.epoch_type & CSP_EPOCH_PSCW)) {
        mpi_errno = PMPI_Win_create(win->base, size, 1, MPI_INFO_NULL, win->ug_comm,
                                    &win->active_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
        CSP_G_DBG_PRINT(" Created active windows 0x%x\n", win->active_win);
    }

    win->csp_g_win_handle = (unsigned long) win;

    /* Notify user root the handle of ghost win. User root is always rank num_g in
     * user root + ghosts communicator */
    mpi_errno = PMPI_Gather(&win->csp_g_win_handle, 1, MPI_UNSIGNED_LONG, NULL,
                            0, MPI_UNSIGNED_LONG, CSP_ENV.num_g, win->ur_g_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    CSP_G_DBG_PRINT(" Define csp_g_win_handle=0x%lx\n", win->csp_g_win_handle);

  fn_exit:

    if (user_bases)
        free(user_bases);

    return mpi_errno;

  fn_fail:
    fprintf(stderr, "error happened in %s, abort\n", __FUNCTION__);
    /* cannot release global comm/win/group */

    if (win->user_base_addrs_in_local)
        free(win->user_base_addrs_in_local);
    if (win->ug_wins)
        free(win->ug_wins);
    if (win)
        free(win);

    PMPI_Abort(MPI_COMM_WORLD, 0);

    goto fn_exit;
}

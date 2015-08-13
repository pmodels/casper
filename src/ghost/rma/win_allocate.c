/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cspg.h"
#include "info.h"

#undef FUNCNAME
#define FUNCNAME CSPG_win_allocate

/* Receive parameters from user root via local communicator (blocking call). */
static inline int recv_ghost_cmd_param(void *params, size_t size, CSPG_win * win)
{
    return PMPI_Recv(params, size, MPI_CHAR, win->user_local_root, CSP_CMD_PARAM_TAG,
                     CSP_COMM_LOCAL, MPI_STATUS_IGNORE);
}

/* Send parameters to user root via local communicator (blocking call). */
static inline int send_ghost_cmd_param(void *params, size_t size, CSPG_win * win)
{
    return PMPI_Send(params, size, MPI_CHAR, win->user_local_root, CSP_CMD_PARAM_TAG,
                     CSP_COMM_LOCAL);
}

static int init_ghost_win(CSP_cmd_winalloc_pkt_t * winalloc_pkt, CSPG_win * win,
                          MPI_Info * user_info)
{
    int mpi_errno = MPI_SUCCESS;
    int info_npairs = 0;
    CSP_info_keyval_t *info_keyvals = NULL;

    win->max_local_user_nprocs = winalloc_pkt->max_local_user_nprocs;
    win->info_args.epoch_type = winalloc_pkt->epoch_type;
    win->is_u_world = winalloc_pkt->is_u_world;
    win->user_nprocs = winalloc_pkt->user_nprocs;
    win->user_local_root = winalloc_pkt->user_local_root;
    info_npairs = winalloc_pkt->info_npairs;

    CSPG_DBG_PRINT(" Received command from %d: max_local_user_nprocs = %d, epoch_type=%d, "
                   "is_u_world=%d, user_nprocs=%d, info npairs=%d\n", win->user_local_root,
                   win->max_local_user_nprocs, win->info_args.epoch_type, win->is_u_world,
                   win->user_nprocs, info_npairs);

    /* Receive window info */
    if (info_npairs > 0) {
        info_keyvals = CSP_calloc(info_npairs, sizeof(CSP_info_keyval_t));
        mpi_errno = recv_ghost_cmd_param(info_keyvals, sizeof(CSP_info_keyval_t) * info_npairs,
                                         win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
        CSPG_DBG_PRINT(" Received parameters: info\n");

        mpi_errno = CSP_info_serialize(info_keyvals, info_npairs, user_info);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

  fn_exit:
    if (info_keyvals)
        free(info_keyvals);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int create_ug_comm(int user_nprocs, int *user_ranks_in_world, int num_ghosts,
                          int *gp_ranks_in_world, CSPG_win * win)
{
    int mpi_errno = MPI_SUCCESS;
    int world_nprocs;
    int *ug_ranks_in_world = NULL;
    MPI_Group ug_group = MPI_GROUP_NULL;
    int num_ug_ranks;

    PMPI_Comm_size(MPI_COMM_WORLD, &world_nprocs);

    /* maximum amount equals to world size */
    ug_ranks_in_world = CSP_calloc(world_nprocs, sizeof(int));
    if (ug_ranks_in_world == NULL)
        goto fn_fail;

    /* -Create ug communicator including all USER processes and Ghost processes. */
    num_ug_ranks = user_nprocs + num_ghosts;
    CSPG_assert(num_ug_ranks <= world_nprocs);
    memcpy(ug_ranks_in_world, gp_ranks_in_world, num_ghosts * sizeof(int));
    memcpy(&ug_ranks_in_world[num_ghosts], user_ranks_in_world, user_nprocs * sizeof(int));

#ifdef CSPG_DEBUG
    int i;
    CSPG_DBG_PRINT("ug_ranks_in_world: (nh %d, nu %d)\n", num_ghosts, user_nprocs);
    for (i = 0; i < num_ug_ranks; i++) {
        CSPG_DBG_PRINT("[%d] %d \n", i, ug_ranks_in_world[i]);
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

static int create_communicators(CSPG_win * win)
{
    int mpi_errno = MPI_SUCCESS;
    int *cmd_params = NULL;

    if (win->is_u_world) {
        /* Fast path of communicator creation for window with user world communicator.
         *  local_ug_comm: including local USER and Ghost processes
         *  ug_comm: including all USER and Ghost processes
         */
        win->local_ug_comm = CSP_COMM_LOCAL;
        win->ug_comm = MPI_COMM_WORLD;
    }
    else {
        /* Receive parameters from local user root
         *  [0]: num_ghosts
         *  [1:N]: user ranks in comm_world
         *  [N+1:]: ghost ranks in comm_world
         */
        int *user_ranks_in_world = NULL, *gp_ranks_in_world = NULL;
        int num_ghosts = 0;
        int cmd_param_size = win->user_nprocs + CSP_ENV.num_g * CSP_NUM_NODES + 1;

        cmd_params = CSP_calloc(cmd_param_size, sizeof(int));
        mpi_errno = recv_ghost_cmd_param((char *) cmd_params, sizeof(int) * cmd_param_size, win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        num_ghosts = cmd_params[0];
        user_ranks_in_world = &cmd_params[1];
        gp_ranks_in_world = &cmd_params[win->user_nprocs + 1];
        CSPG_DBG_PRINT(" Received parameters: num_ghosts %d\n", num_ghosts);

        /* General communicator creation.
         *  local_ug_comm: including local USER and Ghost processes
         *  ug_comm: including all USER and Ghost processes
         */
        mpi_errno = create_ug_comm(win->user_nprocs, user_ranks_in_world, num_ghosts,
                                   gp_ranks_in_world, win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

#ifdef CSP_DEBUG
        {
            int ug_rank, ug_nprocs;
            PMPI_Comm_rank(win->ug_comm, &ug_rank);
            PMPI_Comm_size(win->ug_comm, &ug_nprocs);
            CSPG_DBG_PRINT("created ug_comm, my rank %d/%d\n", ug_rank, ug_nprocs);
        }
#endif
        mpi_errno = PMPI_Comm_split_type(win->ug_comm, MPI_COMM_TYPE_SHARED, 0,
                                         MPI_INFO_NULL, &win->local_ug_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

#ifdef CSP_DEBUG
        {
            int ug_rank, ug_nprocs;
            PMPI_Comm_rank(win->local_ug_comm, &ug_rank);
            PMPI_Comm_size(win->local_ug_comm, &ug_nprocs);
            CSPG_DBG_PRINT("created local_ug_comm, my rank %d/%d\n", ug_rank, ug_nprocs);
        }
#endif
    }

  fn_exit:
    if (cmd_params)
        free(cmd_params);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int create_lock_windows(MPI_Aint size, MPI_Info user_info, CSPG_win * win)
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

    win->ug_wins = CSP_calloc(win->num_ug_wins, sizeof(MPI_Win));
    for (i = 0; i < win->num_ug_wins; i++) {
        mpi_errno = PMPI_Win_create(win->base, size, 1, user_info, win->ug_comm, &win->ug_wins[i]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        CSPG_DBG_PRINT(" Created ug windows[%d] 0x%x\n", i, win->ug_wins[i]);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}


int CSPG_win_allocate(CSP_cmd_pkt_t * pkt, int *exit_flag)
{
    int mpi_errno = MPI_SUCCESS;
    int dst, local_ug_rank, local_ug_nprocs;
    MPI_Aint r_size, size = 0;
    int r_disp_unit;
    int ug_nprocs, ug_rank;
    CSPG_win *win = NULL;
    void **user_bases = NULL;
    MPI_Aint csp_buf_size = CSP_GP_SHARED_SG_SIZE;
    MPI_Info user_info = MPI_INFO_NULL;
    MPI_Info shared_info = MPI_INFO_NULL;
    CSP_cmd_winalloc_pkt_t *winalloc_pkt = &pkt->winalloc;

    win = CSP_calloc(1, sizeof(CSPG_win));

    (*exit_flag) = 0;

    /* Every ghost initialize ghost window by using received parameters. */
    mpi_errno = init_ghost_win(winalloc_pkt, win, &user_info);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Create communicators
     *  ug_comm: including all USER and Ghost processes
     *  local_ug_comm: including local USER and Ghost processes
     */
    create_communicators(win);

    PMPI_Comm_rank(win->local_ug_comm, &local_ug_rank);
    PMPI_Comm_size(win->local_ug_comm, &local_ug_nprocs);
    PMPI_Comm_size(win->ug_comm, &ug_nprocs);
    PMPI_Comm_rank(win->ug_comm, &ug_rank);
    CSPG_DBG_PRINT(" Created ug_comm: %d/%d, local_ug_comm: %d/%d\n",
                   ug_rank, ug_nprocs, local_ug_rank, local_ug_nprocs);

    /* Allocate a shared window with local USER processes */

    /* -Calculate the window size of ghost 0, because it contains extra space
     *  for sync. */
    if (local_ug_rank == 0) {
#ifdef CSP_ENABLE_GRANT_LOCK_HIDDEN_BYTE
        csp_buf_size = CSP_max(csp_buf_size, sizeof(CSP_GRANT_LOCK_DATATYPE));
#endif
    }

    /* -Always set alloc_shm to true, same_size to false for the shared internal window.
     *
     *  We only pass user specified alloc_shm to win_create windows.
     *  - If alloc_shm is true, MPI implementation can still provide shm optimization;
     *  - If alloc_shm is false, those win_create windows are just handled as normal windows in MPI. */
    if (user_info != MPI_INFO_NULL) {
        mpi_errno = PMPI_Info_dup(user_info, &shared_info);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
        mpi_errno = PMPI_Info_set(shared_info, "alloc_shm", "true");
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
        mpi_errno = PMPI_Info_set(shared_info, "same_size", "false");
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* -Allocate shared window in CHAR type
     * (No local buffer, only need shared buffer on user processes) */
    mpi_errno = PMPI_Win_allocate_shared(csp_buf_size, 1, shared_info,
                                         win->local_ug_comm, &win->base, &win->local_ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    CSPG_DBG_PRINT(" Created local_ug_win, base=%p, size=%ld\n", win->base, csp_buf_size);

    /* -Query address of user buffers and send to USER processes */
    user_bases = CSP_calloc(local_ug_nprocs, sizeof(void *));
    win->user_base_addrs_in_local = CSP_calloc(local_ug_nprocs, sizeof(MPI_Aint));

    for (dst = 0; dst < local_ug_nprocs; dst++) {
        mpi_errno = PMPI_Win_shared_query(win->local_ug_win, dst, &r_size,
                                          &r_disp_unit, &user_bases[dst]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        PMPI_Get_address(user_bases[dst], &win->user_base_addrs_in_local[dst]);
        CSPG_DBG_PRINT("   shared base[%d]=%p, addr 0x%lx, offset 0x%lx"
                       ", r_size %ld, r_unit %d\n", dst, user_bases[dst],
                       win->user_base_addrs_in_local[dst],
                       (unsigned long) ((char *) user_bases[dst] - (char *) win->base), r_size,
                       r_disp_unit);

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

    /* - Create lock/lockall windows */
    if ((win->info_args.epoch_type & CSP_EPOCH_LOCK) ||
        (win->info_args.epoch_type & CSP_EPOCH_LOCK_ALL)) {

        mpi_errno = create_lock_windows(size, user_info, win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* - Create global active window */
    if ((win->info_args.epoch_type & CSP_EPOCH_FENCE) ||
        (win->info_args.epoch_type & CSP_EPOCH_PSCW)) {
        mpi_errno = PMPI_Win_create(win->base, size, 1, user_info, win->ug_comm, &win->active_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
        CSPG_DBG_PRINT(" Created active windows 0x%x\n", win->active_win);
    }

    win->csp_g_win_handle = (unsigned long) win;

    /* Notify user root the handle of ghost win. */
    mpi_errno = send_ghost_cmd_param(&win->csp_g_win_handle, sizeof(unsigned long), win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    CSPG_DBG_PRINT(" Define csp_g_win_handle=0x%lx\n", win->csp_g_win_handle);

  fn_exit:
    if (user_info && user_info != MPI_INFO_NULL)
        PMPI_Info_free(&user_info);
    if (shared_info && shared_info != MPI_INFO_NULL)
        PMPI_Info_free(&shared_info);
    if (user_bases)
        free(user_bases);

    return mpi_errno;

  fn_fail:
    CSPG_ERR_PRINT("error happened in %s, abort\n", __FUNCTION__);
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

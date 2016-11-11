/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cspg.h"

/* Receive parameters from user root via local communicator (blocking call). */
static inline int recv_ghost_cmd_param(void *params, size_t size, CSPG_win_t * win)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_CALLMPI(NOSTMT, PMPI_Recv(params, size, MPI_CHAR, win->user_local_root,
                                  CSP_CWP_PARAM_TAG, CSP_PROC.local_comm, MPI_STATUS_IGNORE));
    return mpi_errno;
}

/* Send parameters to user root via local communicator (blocking call). */
static inline int send_ghost_cmd_param(void *params, size_t size, CSPG_win_t * win)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_CALLMPI(NOSTMT, PMPI_Send(params, size, MPI_CHAR, win->user_local_root,
                                  CSP_CWP_PARAM_TAG, CSP_PROC.local_comm));
    return mpi_errno;
}

static int init_ghost_win(CSP_cwp_fnc_winalloc_pkt_t * winalloc_pkt, CSPG_win_t * win,
                          MPI_Info * user_info)
{
    int mpi_errno = MPI_SUCCESS;
    int info_npairs = 0;
    CSP_info_keyval_t *info_keyvals = NULL;

    win->max_local_user_nprocs = winalloc_pkt->max_local_user_nprocs;
    win->info_args.epochs_used = winalloc_pkt->epochs_used;
    win->is_u_world = winalloc_pkt->is_u_world;
    win->user_nprocs = winalloc_pkt->user_nprocs;
    win->user_local_root = winalloc_pkt->user_local_root;
    info_npairs = winalloc_pkt->info_npairs;

    CSPG_DBG_PRINT(" Received command from %d: max_local_user_nprocs = %d, epochs_used=%d, "
                   "is_u_world=%d, user_nprocs=%d, info npairs=%d\n", win->user_local_root,
                   win->max_local_user_nprocs, win->info_args.epochs_used, win->is_u_world,
                   win->user_nprocs, info_npairs);

    /* Receive window info */
    if (info_npairs > 0) {
        info_keyvals = CSP_calloc(info_npairs, sizeof(CSP_info_keyval_t));
        mpi_errno = recv_ghost_cmd_param(info_keyvals, sizeof(CSP_info_keyval_t) * info_npairs,
                                         win);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
        CSPG_DBG_PRINT(" Received parameters: info\n");

        mpi_errno = CSP_info_serialize(info_keyvals, info_npairs, user_info);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

  fn_exit:
    if (info_keyvals)
        free(info_keyvals);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int create_ug_comm(int user_nprocs, int *user_ranks_in_world, int num_ghosts,
                          int *gp_ranks_in_world, CSPG_win_t * win)
{
    int mpi_errno = MPI_SUCCESS;
    int world_nprocs;
    int *ug_ranks_in_world = NULL;
    MPI_Group ug_group = MPI_GROUP_NULL;
    int num_ug_ranks;

    CSP_CALLMPI(JUMP, PMPI_Comm_size(MPI_COMM_WORLD, &world_nprocs));

    /* maximum amount equals to world size */
    ug_ranks_in_world = CSP_calloc(world_nprocs, sizeof(int));
    if (ug_ranks_in_world == NULL)
        goto fn_fail;

    /* -Create ug communicator including all USER processes and Ghost processes. */
    num_ug_ranks = user_nprocs + num_ghosts;
    CSP_ASSERT(num_ug_ranks <= world_nprocs);
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
    CSP_CALLMPI(JUMP, PMPI_Group_incl(CSP_PROC.wgroup, num_ug_ranks, ug_ranks_in_world, &ug_group));
    CSP_CALLMPI(JUMP, PMPI_Comm_create_group(MPI_COMM_WORLD, ug_group, 0, &win->ug_comm));

  fn_exit:
    if (ug_ranks_in_world)
        free(ug_ranks_in_world);
    if (ug_group != MPI_GROUP_NULL)
        CSP_CALLMPI_EXIT(PMPI_Group_free(&ug_group));

    return mpi_errno;

  fn_fail:
    if (win->ug_comm != MPI_COMM_NULL) {
        CSP_CALLMPI_EXIT(PMPI_Comm_free(&win->ug_comm));
        win->ug_comm = MPI_COMM_NULL;
    }
    goto fn_exit;
}

static int create_communicators(CSPG_win_t * win)
{
    int mpi_errno = MPI_SUCCESS;
    int *cmd_params = NULL;

    if (win->is_u_world) {
        /* Fast path of communicator creation for window with user world communicator.
         *  local_ug_comm: including local USER and Ghost processes
         *  ug_comm: including all USER and Ghost processes
         */
        win->local_ug_comm = CSP_PROC.local_comm;
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
        int cmd_param_size = win->user_nprocs + CSP_ENV.num_g * CSP_PROC.num_nodes + 1;

        cmd_params = CSP_calloc(cmd_param_size, sizeof(int));
        mpi_errno = recv_ghost_cmd_param((char *) cmd_params, sizeof(int) * cmd_param_size, win);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);

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
        CSP_CHKMPIFAIL_JUMP(mpi_errno);

#ifdef CSP_DEBUG
        {
            int ug_rank, ug_nprocs;
            CSP_CALLMPI(JUMP, PMPI_Comm_rank(win->ug_comm, &ug_rank));
            CSP_CALLMPI(JUMP, PMPI_Comm_size(win->ug_comm, &ug_nprocs));
            CSPG_DBG_PRINT("created ug_comm, my rank %d/%d\n", ug_rank, ug_nprocs);
        }
#endif
        CSP_CALLMPI(JUMP, PMPI_Comm_split_type(win->ug_comm, MPI_COMM_TYPE_SHARED, 0,
                                               MPI_INFO_NULL, &win->local_ug_comm));

#ifdef CSP_DEBUG
        {
            int ug_rank, ug_nprocs;
            CSP_CALLMPI(JUMP, PMPI_Comm_rank(win->local_ug_comm, &ug_rank));
            CSP_CALLMPI(JUMP, PMPI_Comm_size(win->local_ug_comm, &ug_nprocs));
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

/* Allocate shared window with local GHOSTs and USERs.
 * Return the size of whole shared memory buffer across processes, and
 * the shared window is stored as a CSPG_win_t struct member.*/
static int alloc_shared_window(MPI_Info user_info, MPI_Aint * size, CSPG_win_t * win)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint csp_buf_size = CSP_GP_SHARED_SG_SIZE;
    int dst, local_ug_rank, local_ug_nprocs;
    MPI_Info shared_info = MPI_INFO_NULL;
    int r_disp_unit;
    MPI_Aint r_size;
    void **user_bases = NULL;
    int is_first_nonzero = 1;

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(win->local_ug_comm, &local_ug_rank));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(win->local_ug_comm, &local_ug_nprocs));

    /* -Calculate the window size of ghost 0, because it contains extra space
     *  for sync. */
    if (local_ug_rank == 0) {
#ifdef CSP_ENABLE_GRANT_LOCK_HIDDEN_BYTE
        csp_buf_size = CSP_MAX(csp_buf_size, sizeof(CSP_GRANT_LOCK_DATATYPE));
#endif
    }

    /* -Always set alloc_shm to true, same_size to false, noncontig to false
     *  for the shared internal window.
     *
     *  We only pass user specified alloc_shm to win_create windows.
     *  - If alloc_shm is true, MPI implementation can still provide shm optimization;
     *  - If alloc_shm is false, those win_create windows are just handled as normal windows in MPI.*/
    if (user_info != MPI_INFO_NULL) {
        CSP_CALLMPI(JUMP, PMPI_Info_dup(user_info, &shared_info));
        CSP_CALLMPI(JUMP, PMPI_Info_set(shared_info, "alloc_shm", "true"));
        CSP_CALLMPI(JUMP, PMPI_Info_set(shared_info, "same_size", "false"));
        CSP_CALLMPI(JUMP, PMPI_Info_set(shared_info, "alloc_shared_noncontig", "false"));
    }

    /* -Allocate shared window in CHAR type
     * (No local buffer, only need shared buffer on user processes) */
    CSP_CALLMPI(JUMP, PMPI_Win_allocate_shared(csp_buf_size, 1, shared_info,
                                               win->local_ug_comm, &win->base, &win->local_ug_win));

    /* -Query address of user buffers and send to USER processes */
    user_bases = CSP_calloc(local_ug_nprocs, sizeof(void *));

    for (dst = 0; dst < local_ug_nprocs; dst++) {
        CSP_CALLMPI(JUMP, PMPI_Win_shared_query(win->local_ug_win, dst, &r_size,
                                                &r_disp_unit, &user_bases[dst]));

        CSPG_DBG_PRINT("   shared base[%d]=%p, offset 0x%lx"
                       ", r_size %ld, r_unit %d\n", dst, user_bases[dst],
                       (unsigned long) ((char *) user_bases[dst] - (char *) win->base), r_size,
                       r_disp_unit);

        /* ISSUE: NULL base may be returned if that process passed size=0 in
         * win_allocate_shared (e.g., the ghost process's). If just using the
         * NULL base with non-zero size in later win_create, MPI error happens.
         *
         * SOLUTION: using the first non-zero remote region's start address as the
         * base for win_create. (It is a portable solution because standard
         * guarantees win_allocate_shared always allocates contiguous memory
         * unless `alloc_shared_noncontig` passed).
         *
         * NOTE: Since all ghosts create window starting from the first non-zero
         * region, users can use the same offset for all ghosts */
        if (r_size > 0 && is_first_nonzero) {
            win->base = user_bases[dst];
            is_first_nonzero = 0;
        }

        (*size) += r_size;      /* size in byte */
    }

    CSPG_DBG_PRINT(" Created shared window, base=%p, size=%ld\n", win->base, (*size));

  fn_exit:
    if (shared_info && shared_info != MPI_INFO_NULL)
        CSP_CALLMPI_EXIT(PMPI_Info_free(&shared_info));
    if (user_bases)
        free(user_bases);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int create_lock_windows(MPI_Aint size, MPI_Info user_info, CSPG_win_t * win)
{
    int mpi_errno = MPI_SUCCESS;
    int i;

    /* Need multiple windows for single lock synchronization */
    win->num_ug_wins = win->max_local_user_nprocs;
    win->ug_wins = CSP_calloc(win->num_ug_wins, sizeof(MPI_Win));
    for (i = 0; i < win->num_ug_wins; i++) {
        CSP_CALLMPI(JUMP, PMPI_Win_create(win->base, size, 1, user_info,
                                          win->ug_comm, &win->ug_wins[i]));

        CSPG_DBG_PRINT(" Created ug windows[%d] 0x%x\n", i, win->ug_wins[i]);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

/* Common internal implementation of win_allocate handlers.*/
static int win_allocate_impl(CSP_cwp_fnc_winalloc_pkt_t * winalloc_pkt)
{
    int mpi_errno = MPI_SUCCESS;
    int local_ug_rank, local_ug_nprocs;
    MPI_Aint size = 0;
    int ug_nprocs, ug_rank;
    CSPG_win_t *win = NULL;
    MPI_Info user_info = MPI_INFO_NULL;

    win = CSP_calloc(1, sizeof(CSPG_win_t));

    /* Every ghost initialize ghost window by using received parameters. */
    mpi_errno = init_ghost_win(winalloc_pkt, win, &user_info);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* Create communicators
     *  ug_comm: including all USER and Ghost processes
     *  local_ug_comm: including local USER and Ghost processes
     */
    create_communicators(win);

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(win->local_ug_comm, &local_ug_rank));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(win->local_ug_comm, &local_ug_nprocs));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(win->ug_comm, &ug_nprocs));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(win->ug_comm, &ug_rank));
    CSPG_DBG_PRINT(" Created ug_comm: %d/%d, local_ug_comm: %d/%d\n",
                   ug_rank, ug_nprocs, local_ug_rank, local_ug_nprocs);

    /* Allocate local shared window */
    mpi_errno = alloc_shared_window(user_info, &size, win);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* Create ug windows including all User and Ghost processes.
     * Every User process has a window used for permission check and accessing Ghosts.
     * User processes in different nodes can share a window.
     *  i.e., win[x] can be shared by processes whose local rank is x.
     */

    /* - Create lock N-windows */
    if ((win->info_args.epochs_used & CSP_EPOCH_LOCK)) {

        mpi_errno = create_lock_windows(size, user_info, win);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

    /* - Create global window when fence|pscw are specified,
     *   or only lock_all is specified.*/
    if ((win->info_args.epochs_used & CSP_EPOCH_FENCE) ||
        (win->info_args.epochs_used & CSP_EPOCH_PSCW) ||
        (win->info_args.epochs_used == CSP_EPOCH_LOCK_ALL)) {
        CSP_CALLMPI(JUMP, PMPI_Win_create(win->base, size, 1, user_info,
                                          win->ug_comm, &win->global_win));
        CSPG_DBG_PRINT(" Created global windows 0x%x\n", win->global_win);
    }

    win->csp_g_win_handle = (unsigned long) win;

    /* Notify user root the handle of ghost win. */
    mpi_errno = send_ghost_cmd_param(&win->csp_g_win_handle, sizeof(unsigned long), win);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);
    CSPG_DBG_PRINT(" Define csp_g_win_handle=0x%lx\n", win->csp_g_win_handle);

  fn_exit:
    if (user_info && user_info != MPI_INFO_NULL)
        CSP_CALLMPI_EXIT(PMPI_Info_free(&user_info));
    return mpi_errno;

  fn_fail:
    /* cannot release global comm/win/group */
    if (win->ug_wins)
        free(win->ug_wins);
    if (win)
        free(win);
    goto fn_exit;
}

int CSPG_win_allocate_cwp_root_handler(CSP_cwp_pkt_t * pkt,
                                       int user_local_rank CSP_ATTRIBUTE((unused)))
{
    int mpi_errno = MPI_SUCCESS;
    CSP_cwp_fnc_winalloc_pkt_t *winalloc_pkt = &pkt->u.fnc_winalloc;

    /* broadcast to other local ghosts */
    mpi_errno = CSPG_cwp_bcast(pkt);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    mpi_errno = win_allocate_impl(winalloc_pkt);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    /* Release local lock after a locked command finished. */
    mpi_errno = CSPG_mlock_release();
    return mpi_errno;

  fn_fail:
    /* Error is handled in CSPG_main. */
    goto fn_exit;
}

int CSPG_win_allocate_cwp_handler(CSP_cwp_pkt_t * pkt)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_cwp_fnc_winalloc_pkt_t *winalloc_pkt = &pkt->u.fnc_winalloc;

    mpi_errno = win_allocate_impl(winalloc_pkt);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    return mpi_errno;

  fn_fail:
    /* Error is handled in CSPG_main. */
    goto fn_exit;
}

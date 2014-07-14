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

static int create_ua_comm(int user_nprocs, int *user_ranks_in_world, int num_helpers,
                          int *helper_ranks_in_world, ASP_Win * win)
{
    int mpi_errno = MPI_SUCCESS;
    int world_nprocs, user_world_rank, tmp_world_rank;
    int *ua_ranks_in_world = NULL;
    MPI_Group ua_group = MPI_GROUP_NULL;
    MPI_Status status;
    int dst, num_ua_ranks;

    PMPI_Comm_size(MPI_COMM_WORLD, &world_nprocs);

    /* maximum amount equals to world size */
    ua_ranks_in_world = calloc(world_nprocs, sizeof(int));
    if (ua_ranks_in_world == NULL)
        goto fn_fail;

    /* -Create ua communicator including all USER processes and Helper processes. */
    num_ua_ranks = user_nprocs + num_helpers;
    ASP_Assert(num_ua_ranks <= world_nprocs);
    memcpy(ua_ranks_in_world, user_ranks_in_world, user_nprocs * sizeof(int));
    memcpy(&ua_ranks_in_world[user_nprocs], helper_ranks_in_world, num_helpers * sizeof(int));

    /* -Create ua communicator. */
    PMPI_Group_incl(MPIASP_GROUP_WORLD, num_ua_ranks, ua_ranks_in_world, &ua_group);
    mpi_errno = PMPI_Comm_create_group(MPI_COMM_WORLD, ua_group, 0, &win->ua_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    if (ua_ranks_in_world)
        free(ua_ranks_in_world);
    if (ua_group != MPI_GROUP_NULL)
        PMPI_Group_free(&ua_group);

    return mpi_errno;

  fn_fail:
    if (win->ua_comm != MPI_COMM_NULL) {
        PMPI_Comm_free(&win->ua_comm);
        win->ua_comm = MPI_COMM_NULL;
    }
    goto fn_exit;
}

static int create_communicators(int user_local_root, int user_nprocs, int user_local_nprocs,
                                int user_tag, ASP_Win * win)
{
    int mpi_errno = MPI_SUCCESS;
    int *func_params = NULL, func_param_size = 0;
    int is_user_world;
    int *user_ranks_in_world = NULL;
    int *helper_ranks_in_world = NULL, num_helpers = 0, max_num_helpers;

    user_ranks_in_world = calloc(user_nprocs, sizeof(int));
    helper_ranks_in_world = calloc(max_num_helpers, sizeof(int));
    max_num_helpers = MPIASP_NUM_ASP_IN_LOCAL * MPIASP_NUM_NODES;
    func_param_size = user_nprocs + max_num_helpers + 3;
    func_params = calloc(func_param_size, sizeof(int));

    mpi_errno = MPIASP_Func_get_param((char *) func_params, sizeof(int) * func_param_size,
                                      user_local_root, user_tag);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Get parameters from local User_root
     *  [0]: is_comm_user_world
     *  [1]: max_local_user_nprocs */
    is_user_world = func_params[0];
    win->max_local_user_nprocs = func_params[1];

    if (is_user_world) {
        ASP_DBG_PRINT(" Received parameters from %d: is_user_world %d\n",
                      user_local_root, is_user_world);

        /* Create communicators
         *  local_ua_comm: including local USER and Helper processes
         *  ua_comm: including all USER and Helper processes
         */
        win->local_ua_comm = MPIASP_COMM_LOCAL;
        win->ua_comm = MPI_COMM_WORLD;
    }
    else {

        /*  [2]: num_helpers
         *  [3:N+2]: user ranks in comm_world
         *  [N+3:]: helper ranks in comm_world
         */
        int pidx;
        num_helpers = func_params[2];
        pidx = 3;
        memcpy(user_ranks_in_world, &func_params[pidx], sizeof(int) * user_nprocs);
        pidx += user_nprocs;
        memcpy(helper_ranks_in_world, &func_params[pidx], sizeof(int) * num_helpers);

        ASP_DBG_PRINT(" Received parameters from %d: is_user_world %d, num_helpers %d\n",
                      user_local_root, is_user_world, num_helpers);

        /* Create communicators
         *  ua_comm: including all USER and Helper processes
         *  local_ua_comm: including local USER and Helper processes
         */
        mpi_errno = create_ua_comm(user_nprocs, user_ranks_in_world, num_helpers,
                                   helper_ranks_in_world, win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
        mpi_errno = PMPI_Comm_split_type(win->ua_comm, MPI_COMM_TYPE_SHARED, 0,
                                         MPI_INFO_NULL, &win->local_ua_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

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

int ASP_Win_allocate(int user_local_root, int user_nprocs, int user_local_nprocs, int user_tag)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Status status;
    int dst, local_ua_rank, local_ua_nprocs;
    MPI_Aint r_size, size = 0;
    int r_disp_unit, req_idx;
    int ua_nprocs, ua_rank;
    ASP_Win *win;
    void **user_bases = NULL;
    int i;
    int mtcore_buf_size = 0;

    win = calloc(1, sizeof(ASP_Win));

    /* Create communicators
     *  ua_comm: including all USER and Helper processes
     *  local_ua_comm: including local USER and Helper processes
     */
    create_communicators(user_local_root, user_nprocs, user_local_nprocs, user_tag, win);

    PMPI_Comm_rank(win->local_ua_comm, &local_ua_rank);
    PMPI_Comm_size(win->local_ua_comm, &local_ua_nprocs);

    PMPI_Comm_size(win->ua_comm, &ua_nprocs);
    PMPI_Comm_rank(win->ua_comm, &ua_rank);
    ASP_DBG_PRINT(" Created ua_comm: %d/%d, local_ua_comm: %d/%d\n",
                  ua_rank, ua_nprocs, local_ua_rank, local_ua_rank);

    /* Allocate a shared window with local USER processes */

    /* -Allocate shared window in CHAR type
     * (No local buffer, only need shared buffer on user processes)
     */
#ifdef MTCORE_ENABLE_GRANT_LOCK_HIDDEN_BYTE
    /* Additional byte for granting locks on Helper 0 */
    if (local_ua_rank == 0) {
        mtcore_buf_size += sizeof(MTCORE_GRANT_LOCK_DATATYPE);
    }
#endif
    mpi_errno = PMPI_Win_allocate_shared(mtcore_buf_size, 1, MPI_INFO_NULL,
                                         win->local_ua_comm, &win->base, &win->local_ua_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    ASP_DBG_PRINT(" Created local_ua_win, base=%p\n", win->base);

    /* -Query address of user buffers and send to USER processes */
    user_bases = calloc(local_ua_nprocs, sizeof(void *));
    win->user_base_addrs_in_local = calloc(local_ua_nprocs, sizeof(MPI_Aint));

    for (dst = 0; dst < local_ua_nprocs; dst++) {
        mpi_errno = PMPI_Win_shared_query(win->local_ua_win, dst, &r_size,
                                          &r_disp_unit, &user_bases[dst]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        PMPI_Get_address(user_bases[dst], &win->user_base_addrs_in_local[dst]);
        ASP_DBG_PRINT("   shared base[%d]=%p, addr 0x%lx, offset 0x%lx"
                      ", r_size %ld, r_unit %d\n", dst, user_bases[dst],
                      win->user_base_addrs_in_local[dst],
                      (unsigned long) (user_bases[dst] - win->base), r_size, r_disp_unit);

        size += r_size; /* size in byte */
    }

    /* Create ua windows including all User and Helper processes.
     * Every User process has a window used for permission check and accessing Helpers.
     * User processes in different nodes can share a window.
     *  i.e., win[x] can be shared by processes whose local rank is x.
     */
    win->ua_wins = calloc(win->max_local_user_nprocs, sizeof(MPI_Win));
    for (i = 0; i < win->max_local_user_nprocs; i++) {
        mpi_errno = PMPI_Win_create(win->base, size, 1, MPI_INFO_NULL, win->ua_comm,
                                    &win->ua_wins[i]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        ASP_DBG_PRINT(" Created ua windows[%d] 0x%x\n", i, win->ua_wins[i]);
    }

    win->asp_win_handle = (unsigned long) win->ua_wins;
    mpi_errno = put_asp_win(win->asp_win_handle, win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Notify user root the handle of ASP win */
    mpi_errno =
        PMPI_Send(&win->asp_win_handle, 1, MPI_INT, user_local_root, user_tag, MPIASP_COMM_LOCAL);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:

    if (user_bases)
        free(user_bases);

    return mpi_errno;

  fn_fail:

    if (win->local_ua_win)
        PMPI_Win_free(&win->local_ua_win);
    if (win->ua_wins) {
        for (i = 0; i < win->max_local_user_nprocs; i++) {
            if (win->ua_wins)
                PMPI_Win_free(&win->ua_wins[i]);
        }
    }

    if (win->local_ua_comm && win->local_ua_comm != MPIASP_COMM_LOCAL)
        PMPI_Comm_free(&win->local_ua_comm);
    if (win->ua_comm && win->ua_comm != MPI_COMM_WORLD)
        PMPI_Comm_free(&win->ua_comm);

    if (win->user_base_addrs_in_local)
        free(win->user_base_addrs_in_local);
    if (win->ua_wins)
        free(win->ua_wins);
    if (win)
        free(win);

    goto fn_exit;
}

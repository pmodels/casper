#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mtcore_helper.h"

#undef FUNCNAME
#define FUNCNAME MTCORE_H_win_allocate

static int create_uh_comm(int user_nprocs, int *user_ranks_in_world, int num_helpers,
                          int *helper_ranks_in_world, MTCORE_H_win * win)
{
    int mpi_errno = MPI_SUCCESS;
    int world_nprocs, user_world_rank, tmp_world_rank;
    int *uh_ranks_in_world = NULL;
    MPI_Group uh_group = MPI_GROUP_NULL;
    MPI_Status status;
    int dst, num_uh_ranks;

    PMPI_Comm_size(MPI_COMM_WORLD, &world_nprocs);

    /* maximum amount equals to world size */
    uh_ranks_in_world = calloc(world_nprocs, sizeof(int));
    if (uh_ranks_in_world == NULL)
        goto fn_fail;

    /* -Create uh communicator including all USER processes and Helper processes. */
    num_uh_ranks = user_nprocs + num_helpers;
    MTCORE_H_assert(num_uh_ranks <= world_nprocs);
    memcpy(uh_ranks_in_world, user_ranks_in_world, user_nprocs * sizeof(int));
    memcpy(&uh_ranks_in_world[user_nprocs], helper_ranks_in_world, num_helpers * sizeof(int));

    /* -Create uh communicator. */
    PMPI_Group_incl(MTCORE_GROUP_WORLD, num_uh_ranks, uh_ranks_in_world, &uh_group);
    mpi_errno = PMPI_Comm_create_group(MPI_COMM_WORLD, uh_group, 0, &win->uh_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    if (uh_ranks_in_world)
        free(uh_ranks_in_world);
    if (uh_group != MPI_GROUP_NULL)
        PMPI_Group_free(&uh_group);

    return mpi_errno;

  fn_fail:
    if (win->uh_comm != MPI_COMM_NULL) {
        PMPI_Comm_free(&win->uh_comm);
        win->uh_comm = MPI_COMM_NULL;
    }
    goto fn_exit;
}

static int create_communicators(int user_nprocs, int user_local_nprocs, MTCORE_H_win * win)
{
    int mpi_errno = MPI_SUCCESS;
    int *func_params = NULL, func_param_size = 0;
    int is_user_world;
    int *user_ranks_in_world = NULL;
    int *helper_ranks_in_world = NULL, num_helpers = 0, max_num_helpers;

    user_ranks_in_world = calloc(user_nprocs, sizeof(int));
    max_num_helpers = MTCORE_NUM_H * MTCORE_NUM_NODES;
    helper_ranks_in_world = calloc(max_num_helpers, sizeof(int));
    func_param_size = user_nprocs + max_num_helpers + 3;
    func_params = calloc(func_param_size, sizeof(int));

    mpi_errno = MTCORE_H_func_get_param((char *) func_params, sizeof(int) * func_param_size,
                                        win->ur_h_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Get parameters from local User_root
     *  [0]: is_comm_user_world
     *  [1]: max_local_user_nprocs */
    is_user_world = func_params[0];
    win->max_local_user_nprocs = func_params[1];

    if (is_user_world) {
        MTCORE_H_DBG_PRINT(" Received parameters: is_user_world %d\n", is_user_world);

        /* Create communicators
         *  local_uh_comm: including local USER and Helper processes
         *  uh_comm: including all USER and Helper processes
         */
        win->local_uh_comm = MTCORE_COMM_LOCAL;
        win->uh_comm = MPI_COMM_WORLD;
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

        MTCORE_H_DBG_PRINT(" Received parameters: is_user_world %d, num_helpers %d\n",
                           is_user_world, num_helpers);

        /* Create communicators
         *  uh_comm: including all USER and Helper processes
         *  local_uh_comm: including local USER and Helper processes
         */
        mpi_errno = create_uh_comm(user_nprocs, user_ranks_in_world, num_helpers,
                                   helper_ranks_in_world, win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
        mpi_errno = PMPI_Comm_split_type(win->uh_comm, MPI_COMM_TYPE_SHARED, 0,
                                         MPI_INFO_NULL, &win->local_uh_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

  fn_exit:
    if (func_params)
        free(func_params);
    if (user_ranks_in_world)
        free(user_ranks_in_world);
    if (helper_ranks_in_world)
        free(helper_ranks_in_world);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MTCORE_H_win_allocate(int user_local_root, int user_nprocs, int user_local_nprocs)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Status status;
    int dst, local_uh_rank, local_uh_nprocs;
    MPI_Aint r_size, size = 0;
    int r_disp_unit, req_idx;
    int uh_nprocs, uh_rank;
    MTCORE_H_win *win;
    void **user_bases = NULL;
    int i;
    int mtcore_buf_size = MTCORE_HELPER_SHARED_SG_SIZE;

    win = calloc(1, sizeof(MTCORE_H_win));

    /* Create user root + helpers communicator for
     * internal information exchange between users and helpers. */
    mpi_errno = MTCORE_H_func_new_ur_h_comm(user_local_root, &win->ur_h_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Create communicators
     *  uh_comm: including all USER and Helper processes
     *  local_uh_comm: including local USER and Helper processes
     */
    create_communicators(user_nprocs, user_local_nprocs, win);

    PMPI_Comm_rank(win->local_uh_comm, &local_uh_rank);
    PMPI_Comm_size(win->local_uh_comm, &local_uh_nprocs);
    PMPI_Comm_size(win->uh_comm, &uh_nprocs);
    PMPI_Comm_rank(win->uh_comm, &uh_rank);
    MTCORE_H_DBG_PRINT(" Created uh_comm: %d/%d, local_uh_comm: %d/%d\n",
                       uh_rank, uh_nprocs, local_uh_rank, local_uh_nprocs);

    /* Allocate a shared window with local USER processes */

    /* -Allocate shared window in CHAR type
     * (No local buffer, only need shared buffer on user processes)
     */
#ifdef MTCORE_ENABLE_GRANT_LOCK_HIDDEN_BYTE
    /* Additional byte for granting locks on Helper 0, all the other
     * helpers share the same byte */
    if (local_uh_rank == 0) {
        mtcore_buf_size = max(MTCORE_HELPER_SHARED_SG_SIZE, sizeof(MTCORE_GRANT_LOCK_DATATYPE));
    }
#endif
    mpi_errno = PMPI_Win_allocate_shared(mtcore_buf_size, 1, MPI_INFO_NULL,
                                         win->local_uh_comm, &win->base, &win->local_uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    MTCORE_H_DBG_PRINT(" Created local_uh_win, base=%p, size=%d\n", win->base, mtcore_buf_size);

    /* -Query address of user buffers and send to USER processes */
    user_bases = calloc(local_uh_nprocs, sizeof(void *));
    win->user_base_addrs_in_local = calloc(local_uh_nprocs, sizeof(MPI_Aint));

    for (dst = 0; dst < local_uh_nprocs; dst++) {
        mpi_errno = PMPI_Win_shared_query(win->local_uh_win, dst, &r_size,
                                          &r_disp_unit, &user_bases[dst]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        PMPI_Get_address(user_bases[dst], &win->user_base_addrs_in_local[dst]);
        MTCORE_H_DBG_PRINT("   shared base[%d]=%p, addr 0x%lx, offset 0x%lx"
                           ", r_size %ld, r_unit %d\n", dst, user_bases[dst],
                           win->user_base_addrs_in_local[dst],
                           (unsigned long) (user_bases[dst] - win->base), r_size, r_disp_unit);

        size += r_size; /* size in byte */
    }

    /* All helpers create window starting from the baseptr of helper 0, so users
     * can use the same offset for all helpers*/

    /* FIXME: if size=0 and helper rank > 0, base may be returned as 0x0.
     * Is it implementation specific ? What is the uniform solution ?
     * It is not wrong that simply use base[0] for creating window, because it is accessible. */
    win->base = user_bases[0];

    /* Create uh windows including all User and Helper processes.
     * Every User process has a window used for permission check and accessing Helpers.
     * User processes in different nodes can share a window.
     *  i.e., win[x] can be shared by processes whose local rank is x.
     */
    win->uh_wins = calloc(win->max_local_user_nprocs, sizeof(MPI_Win));
    for (i = 0; i < win->max_local_user_nprocs; i++) {
        mpi_errno = PMPI_Win_create(win->base, size, 1, MPI_INFO_NULL, win->uh_comm,
                                    &win->uh_wins[i]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        MTCORE_H_DBG_PRINT(" Created uh windows[%d] 0x%x\n", i, win->uh_wins[i]);
    }

    win->mtcore_h_win_handle = (unsigned long) win->uh_wins;
    mpi_errno = mtcore_put_h_win(win->mtcore_h_win_handle, win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Notify user root the handle of helper win. User root is always rank num_h in
     * user root + helpers communicator */
    mpi_errno = PMPI_Gather(&win->mtcore_h_win_handle, 1, MPI_UNSIGNED_LONG, NULL,
                            0, MPI_UNSIGNED_LONG, MTCORE_NUM_H, win->ur_h_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    MTCORE_H_DBG_PRINT(" Define mtcore_h_win_handle=0x%lx\n", win->mtcore_h_win_handle);

  fn_exit:

    if (user_bases)
        free(user_bases);

    return mpi_errno;

  fn_fail:

    if (win->local_uh_win)
        PMPI_Win_free(&win->local_uh_win);
    if (win->uh_wins) {
        for (i = 0; i < win->max_local_user_nprocs; i++) {
            if (win->uh_wins)
                PMPI_Win_free(&win->uh_wins[i]);
        }
    }

    if (win->ur_h_comm && win->ur_h_comm != MPI_COMM_NULL)
        PMPI_Comm_free(&win->ur_h_comm);
    if (win->local_uh_comm && win->local_uh_comm != MTCORE_COMM_LOCAL)
        PMPI_Comm_free(&win->local_uh_comm);
    if (win->uh_comm && win->uh_comm != MPI_COMM_WORLD)
        PMPI_Comm_free(&win->uh_comm);

    if (win->user_base_addrs_in_local)
        free(win->user_base_addrs_in_local);
    if (win->uh_wins)
        free(win->uh_wins);
    if (win)
        free(win);

    goto fn_exit;
}

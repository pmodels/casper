/*
 * win_start.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

static int fill_ranks_in_win_grp(MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int *ranks_in_start_grp = NULL;
    int i, start_grp_size;

    mpi_errno = PMPI_Group_size(uh_win->start_group, &start_grp_size);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    ranks_in_start_grp = calloc(start_grp_size, sizeof(int));
    for (i = 0; i < start_grp_size; i++) {
        ranks_in_start_grp[i] = i;
    }

    mpi_errno = PMPI_Group_translate_ranks(uh_win->start_group, start_grp_size,
                                           ranks_in_start_grp, uh_win->user_group,
                                           uh_win->start_ranks_in_win_group);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    if (ranks_in_start_grp)
        free(ranks_in_start_grp);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int wait_pscw_target_post(int target_rank, MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank;
    int *target_post_flg_ptr;

    /* every process has separate flag */
    target_post_flg_ptr = (int *) ((unsigned long) uh_win->post_flg_ptr
                                   + target_rank * sizeof(int));

    MTCORE_DBG_PRINT("wait pscw post at %p(%p + 0x%x) for target %d\n",
                     target_post_flg_ptr, uh_win->post_flg_ptr, target_rank * sizeof(int),
                     target_rank);

    /* Wait for the remote post call updating local flag. */
    while (*target_post_flg_ptr != 1) {
        mpi_errno = PMPI_Win_sync(uh_win->pscw_sync_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    *target_post_flg_ptr = 0;
    /* avoid instruction reordering */
    mpi_errno = PMPI_Win_sync(uh_win->pscw_sync_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}


static inline int MTCORE_Win_lock_self_impl(MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank;

#ifdef MTCORE_ENABLE_SYNC_ALL_OPT
    /* lockall already locked window for local target */
#else
    MTCORE_DBG_PRINT("[%d]lock self(%d, local pscw_win 0x%x)\n", user_rank,
                     uh_win->my_rank_in_uh_comm, uh_win->my_pscw_win);
    mpi_errno = PMPI_Win_lock(MPI_LOCK_SHARED, uh_win->my_rank_in_uh_comm,
                              MPI_MODE_NOCHECK, uh_win->my_pscw_win);
    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;
#endif

    uh_win->is_self_locked = 1;
    return mpi_errno;
}

static int MTCORE_Win_Pscw_lock(int target_rank, int assert, MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank;
    int k;

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);

    uh_win->targets[target_rank].remote_lock_assert = assert;
    MTCORE_DBG_PRINT("[%d]pscw_lock(%d), MPI_MODE_NOCHECK %d(assert %d)\n", user_rank,
                     target_rank, (assert & MPI_MODE_NOCHECK) != 0, assert);

#ifdef MTCORE_ENABLE_SYNC_ALL_OPT
    MTCORE_DBG_PRINT("[%d]lock_all(pscw_win 0x%x), instead of target rank %d\n",
                     user_rank, uh_win->targets[target_rank].pscw_win, target_rank);
    mpi_errno = PMPI_Win_lock_all(assert, uh_win->targets[target_rank].pscw_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
#else
    /* Lock every helper on every window.
     * Note that a helper may be used on any window of this process for runtime
     * load balancing whether it is bound to that segment or not. */
    for (k = 0; k < MTCORE_ENV.num_h; k++) {
        int target_h_rank_in_uh = uh_win->targets[target_rank].h_ranks_in_uh[k];

        MTCORE_DBG_PRINT("[%d]lock(Helper(%d), pscw_wins 0x%x), instead of "
                         "target rank %d\n", user_rank, target_h_rank_in_uh,
                         uh_win->targets[target_rank].pscw_win, target_rank);

        mpi_errno = PMPI_Win_lock(MPI_LOCK_SHARED, target_h_rank_in_uh, assert,
                                  uh_win->targets[target_rank].pscw_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
#endif

    uh_win->is_self_locked = 0;

    if (user_rank == target_rank) {
        /* During pscw epoch, it is allowed to access local target directly */

#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
        mpi_errno = MTCORE_Win_lock_self_impl(uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
#endif
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_start(MPI_Group group, int assert, MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int start_grp_size = 0;
    int i;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    if (uh_win == NULL) {
        /* normal window */
        return PMPI_Win_start(group, assert, win);
    }

    /* mtcore window starts */

    MTCORE_Assert((uh_win->info_args.epoch_type & MTCORE_EPOCH_PSCW));

    if (group == MPI_GROUP_NULL) {
        /* standard says do nothing for empty group */
        MTCORE_DBG_PRINT("Start empty group\n");
        return mpi_errno;
    }

    mpi_errno = PMPI_Group_size(group, &start_grp_size);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (start_grp_size <= 0) {
        /* standard says do nothing for empty group */
        MTCORE_DBG_PRINT("Start empty group\n");
        return mpi_errno;
    }

    uh_win->start_group = group;
    uh_win->start_ranks_in_win_group = calloc(start_grp_size, sizeof(int));
    MTCORE_DBG_PRINT("start group 0x%x, size %d\n", uh_win->start_group, start_grp_size);

    /* Both lock and start only allow no_check assert. */
    assert = (assert == MPI_MODE_NOCHECK) ? MPI_MODE_NOCHECK : 0;

    mpi_errno = fill_ranks_in_win_grp(uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    for (i = 0; i < start_grp_size; i++) {

        /* Synchronize start-post if user does not specify nocheck */
        if ((assert & MPI_MODE_NOCHECK) == 0) {
            mpi_errno = wait_pscw_target_post(uh_win->start_ranks_in_win_group[i], uh_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        MTCORE_DBG_PRINT("\t\t start target %d\n", uh_win->start_ranks_in_win_group[i]);
        mpi_errno = MTCORE_Win_Pscw_lock(uh_win->start_ranks_in_win_group[i], assert, uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* Indicate epoch status, later operations will be redirected to pscw_win
     * until start counter decreases to 0 .*/
    uh_win->epoch_stat = MTCORE_WIN_EPOCH_PSCW;
    uh_win->start_counter++;

  fn_exit:
    return mpi_errno;

  fn_fail:
    if (uh_win->start_ranks_in_win_group)
        free(uh_win->start_ranks_in_win_group);
    uh_win->start_group = MPI_GROUP_NULL;
    uh_win->start_ranks_in_win_group = NULL;

    goto fn_exit;
}

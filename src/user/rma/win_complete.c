/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

static int CSP_send_pscw_complete_msg(int start_grp_size, CSP_win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int i, user_rank;
    char comp_flg = 1;
    MPI_Request *reqs = NULL;
    MPI_Status *stats = NULL;
    int remote_cnt = 0;

    reqs = CSP_calloc(start_grp_size, sizeof(MPI_Request));
    stats = CSP_calloc(start_grp_size, sizeof(MPI_Status));

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);

    for (i = 0; i < start_grp_size; i++) {
        int target_rank = ug_win->start_ranks_in_win_group[i];

        /* Do not send to local target, otherwise it may deadlock.
         * We do not check the wrong sync case that user calls wait(self)
         * before complete(self). */
        if (user_rank == target_rank)
            continue;

        mpi_errno = PMPI_Isend(&comp_flg, 1, MPI_CHAR, target_rank,
                               CSP_PSCW_CW_TAG, ug_win->user_comm, &reqs[remote_cnt++]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* Set post flag to true on the main ghost of post origin. */
        CSP_DBG_PRINT("send pscw complete msg to target %d \n", target_rank);
    }

    /* Has to blocking wait here to poll progress. */
    mpi_errno = PMPI_Waitall(remote_cnt, reqs, stats);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    if (reqs)
        free(reqs);
    if (stats)
        free(stats);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int CSP_complete_flush(int start_grp_size CSP_ATTRIBUTE((unused)), CSP_win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank;
    int i;

    CSP_DBG_PRINT_FCNAME();

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);

    /* Flush ghosts to finish the sequence of locally issued RMA operations */
#ifdef CSP_ENABLE_SYNC_ALL_OPT

    /* Optimization for MPI implementations that have optimized lock_all.
     * However, user should be noted that, if MPI implementation issues lock messages
     * for every target even if it does not have any operation, this optimization
     * could lose performance and even lose asynchronous! */
    CSP_DBG_PRINT("[%d]flush_all(active_win 0x%x)\n", user_rank, ug_win->active_win);
    mpi_errno = PMPI_Win_flush_all(ug_win->active_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
#else

    /* Flush every ghost once in the single window.
     * TODO: track op issuing, only flush the ghosts which receive ops. */
    for (i = 0; i < ug_win->num_g_ranks_in_ug; i++) {
        mpi_errno = PMPI_Win_flush(ug_win->g_ranks_in_ug[i], ug_win->active_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

#ifdef CSP_ENABLE_LOCAL_LOCK_OPT
    /* Need flush local target */
    for (i = 0; i < start_grp_size; i++) {
        if (ug_win->start_ranks_in_win_group[i] == user_rank) {
            mpi_errno = PMPI_Win_flush(ug_win->my_rank_in_ug_comm, ug_win->active_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
    }
#endif

#endif

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_complete(MPI_Win win)
{
    CSP_win *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int start_grp_size = 0;
    int i;

    CSP_DBG_PRINT_FCNAME();

    CSP_fetch_ug_win_from_cache(win, ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_complete(win);
    }

    CSP_assert((ug_win->info_args.epoch_type & CSP_EPOCH_PSCW));

    if (ug_win->start_group == MPI_GROUP_NULL) {
        /* standard says do nothing for empty group */
        CSP_DBG_PRINT("Complete empty group\n");
        return mpi_errno;
    }

    mpi_errno = PMPI_Group_size(ug_win->start_group, &start_grp_size);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    CSP_assert(start_grp_size > 0);

    CSP_DBG_PRINT("Complete group 0x%x, size %d\n", ug_win->start_group, start_grp_size);

#ifdef CSP_ENABLE_EPOCH_STAT_CHECK
    /* Check access epoch status.
     * The current epoch must be pscw on all involved targets.*/
    if (ug_win->epoch_stat != CSP_WIN_EPOCH_PER_TARGET) {
        CSP_ERR_PRINT("Wrong synchronization call! "
                      "No opening per-target epoch in %s\n", __FUNCTION__);
        mpi_errno = -1;
        goto fn_fail;
    }
    else {
        for (i = 0; i < start_grp_size; i++) {
            int target_rank = ug_win->start_ranks_in_win_group[i];
            if (ug_win->targets[target_rank].epoch_stat != CSP_TARGET_EPOCH_PSCW) {
                CSP_ERR_PRINT("Wrong synchronization call! "
                              "No opening PSCW epoch on target %d in %s\n", target_rank,
                              __FUNCTION__);
                mpi_errno = -1;
                goto fn_fail;
            }
        }
    }
#endif

    mpi_errno = CSP_complete_flush(start_grp_size, ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    ug_win->is_self_locked = 0;

    mpi_errno = CSP_send_pscw_complete_msg(start_grp_size, ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Reset per-target epoch status. */
    for (i = 0; i < start_grp_size; i++) {
        int target_rank = ug_win->start_ranks_in_win_group[i];
        ug_win->targets[target_rank].epoch_stat = CSP_TARGET_NO_EPOCH;
    }

    /* Reset global epoch status. */
    ug_win->start_counter--;
    CSP_assert(ug_win->start_counter >= 0);
    if (ug_win->start_counter == 0 && ug_win->lock_counter == 0) {
        CSP_DBG_PRINT("all per-target epoch are cleared !\n");
        ug_win->epoch_stat = CSP_WIN_NO_EPOCH;
    }

    CSP_DBG_PRINT("Complete done\n");

  fn_exit:
    if (ug_win->start_ranks_in_win_group)
        free(ug_win->start_ranks_in_win_group);
    ug_win->start_group = MPI_GROUP_NULL;
    ug_win->start_ranks_in_win_group = NULL;

    return mpi_errno;

  fn_fail:
    goto fn_exit;

}

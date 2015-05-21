/*
 * win_complete.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include "csp.h"

static int CSP_Send_pscw_complete_msg(int start_grp_size, CSP_Win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int i, user_rank;
    char comp_flg = 1;
    MPI_Request *reqs = NULL;
    MPI_Status *stats = NULL;
    int remote_cnt = 0;

    reqs = calloc(start_grp_size, sizeof(MPI_Request));
    stats = calloc(start_grp_size, sizeof(MPI_Status));

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

static int CSP_Complete_flush(int start_grp_size, CSP_Win * ug_win)
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

    /* TODO: All the operations which we have not wrapped up will be failed, because they
     * are issued to user window. We need wrap up all operations.
     */

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_complete(MPI_Win win)
{
    CSP_Win *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int start_grp_size = 0;

    CSP_DBG_PRINT_FCNAME();

    CSP_Fetch_ug_win_from_cache(win, ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_complete(win);
    }

    CSP_Assert((ug_win->info_args.epoch_type & CSP_EPOCH_PSCW));

    if (ug_win->start_group == MPI_GROUP_NULL) {
        /* standard says do nothing for empty group */
        CSP_DBG_PRINT("Complete empty group\n");
        return mpi_errno;
    }

    mpi_errno = PMPI_Group_size(ug_win->start_group, &start_grp_size);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    CSP_Assert(start_grp_size > 0);

    CSP_DBG_PRINT("Complete group 0x%x, size %d\n", ug_win->start_group, start_grp_size);

    mpi_errno = CSP_Complete_flush(start_grp_size, ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    ug_win->is_self_locked = 0;

    mpi_errno = CSP_Send_pscw_complete_msg(start_grp_size, ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Indicate epoch status, later operations should not be redirected to active_win
     * after the start counter decreases to 0 .*/
    ug_win->start_counter--;
    if (ug_win->start_counter == 0) {
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

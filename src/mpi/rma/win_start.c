/*
 * win_start.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include "csp.h"

static int fill_ranks_in_win_grp(CSP_Win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int *ranks_in_start_grp = NULL;
    int i, start_grp_size;

    mpi_errno = PMPI_Group_size(ug_win->start_group, &start_grp_size);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    ranks_in_start_grp = calloc(start_grp_size, sizeof(int));
    for (i = 0; i < start_grp_size; i++) {
        ranks_in_start_grp[i] = i;
    }

    mpi_errno = PMPI_Group_translate_ranks(ug_win->start_group, start_grp_size,
                                           ranks_in_start_grp, ug_win->user_group,
                                           ug_win->start_ranks_in_win_group);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    if (ranks_in_start_grp)
        free(ranks_in_start_grp);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int CSP_Wait_pscw_post_msg(int start_grp_size, CSP_Win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank;
    int i;
    char post_flg;
    MPI_Request *reqs = NULL;
    MPI_Status *stats = NULL;
    int remote_cnt = 0;

    reqs = calloc(start_grp_size, sizeof(MPI_Request));
    stats = calloc(start_grp_size, sizeof(MPI_Status));

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);

    for (i = 0; i < start_grp_size; i++) {
        int target_rank = ug_win->start_ranks_in_win_group[i];

        /* Do not receive from local target, otherwise it may deadlock.
         * We do not check the wrong sync case that user calls start(self)
         * before/without post(self). */
        if (user_rank == target_rank)
            continue;

        mpi_errno = PMPI_Irecv(&post_flg, 1, MPI_CHAR, target_rank,
                               CSP_PSCW_PS_TAG, ug_win->user_comm, &reqs[remote_cnt++]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* Set post flag to true on the main ghost of post origin. */
        CSP_DBG_PRINT("receive pscw post msg from target %d \n", target_rank);
    }

    /* It is blocking. */
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

int MPI_Win_start(MPI_Group group, int assert, MPI_Win win)
{
    CSP_Win *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int start_grp_size = 0;

    CSP_DBG_PRINT_FCNAME();

    CSP_Fetch_ug_win_from_cache(win, ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_start(group, assert, win);
    }

    CSP_Assert((ug_win->info_args.epoch_type & CSP_EPOCH_PSCW));

    if (group == MPI_GROUP_NULL) {
        /* standard says do nothing for empty group */
        CSP_DBG_PRINT("Start empty group\n");
        return mpi_errno;
    }

    mpi_errno = PMPI_Group_size(group, &start_grp_size);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (start_grp_size <= 0) {
        /* standard says do nothing for empty group */
        CSP_DBG_PRINT("Start empty group\n");
        return mpi_errno;
    }

    ug_win->start_group = group;
    ug_win->start_ranks_in_win_group = calloc(start_grp_size, sizeof(int));
    CSP_DBG_PRINT("start group 0x%x, size %d\n", ug_win->start_group, start_grp_size);

    /* Both lock and start only allow no_check assert. */
    assert = (assert == MPI_MODE_NOCHECK) ? MPI_MODE_NOCHECK : 0;

    mpi_errno = fill_ranks_in_win_grp(ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Synchronize start-post if user does not specify nocheck */
    if ((assert & MPI_MODE_NOCHECK) == 0) {
        mpi_errno = CSP_Wait_pscw_post_msg(start_grp_size, ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    ug_win->is_self_locked = 0;
#ifdef CSP_ENABLE_LOCAL_LOCK_OPT
    /* During pscw epoch, it is allowed to access local target directly.
     * Note that user may do wrong RMA call such as access the local target
     * without start-post on local window. But we do not check it. */
    ug_win->is_self_locked = 1;
#endif

    /* Indicate epoch status, later operations will be redirected to active_win
     * until start counter decreases to 0 .*/
    ug_win->epoch_stat = CSP_WIN_EPOCH_PSCW;
    ug_win->start_counter++;

    CSP_DBG_PRINT("Start done\n");

  fn_exit:
    return mpi_errno;

  fn_fail:
    if (ug_win->start_ranks_in_win_group)
        free(ug_win->start_ranks_in_win_group);
    ug_win->start_group = MPI_GROUP_NULL;
    ug_win->start_ranks_in_win_group = NULL;

    goto fn_exit;
}

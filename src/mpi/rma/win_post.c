/*
 * win_post.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

static int MTCORE_Send_pscw_post_msg(int post_grp_size, MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int i, user_rank;
    char post_flg = 1;
    MPI_Request *reqs = NULL;
    MPI_Status *stats = NULL;
    int remote_cnt = 0;

    reqs = calloc(post_grp_size, sizeof(MPI_Request));
    stats = calloc(post_grp_size, sizeof(MPI_Status));

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);

    for (i = 0; i < post_grp_size; i++) {
        int origin_rank = uh_win->post_ranks_in_win_group[i];

        /* Do not send to local target, otherwise it may deadlock.
         * We do not check the wrong sync case that user calls start(self)
         * before post(self). */
        if (user_rank == origin_rank)
            continue;

        mpi_errno = PMPI_Isend(&post_flg, 1, MPI_CHAR, origin_rank,
                               MTCORE_PSCW_PS_TAG, uh_win->user_comm, &reqs[remote_cnt++]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* Set post flag to true on the main helper of post origin. */
        MTCORE_DBG_PRINT("send pscw post msg to origin %d \n", origin_rank);
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

static int fill_ranks_in_win_grp(MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int *ranks_in_post_grp = NULL;
    int i, post_grp_size;

    mpi_errno = PMPI_Group_size(uh_win->post_group, &post_grp_size);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    ranks_in_post_grp = calloc(post_grp_size, sizeof(int));
    for (i = 0; i < post_grp_size; i++) {
        ranks_in_post_grp[i] = i;
    }

    mpi_errno = PMPI_Group_translate_ranks(uh_win->post_group, post_grp_size,
                                           ranks_in_post_grp, uh_win->user_group,
                                           uh_win->post_ranks_in_win_group);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    if (ranks_in_post_grp)
        free(ranks_in_post_grp);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_post(MPI_Group group, int assert, MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int post_grp_size = 0;
    int i;

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    if (uh_win == NULL) {
        /* normal window */
        return PMPI_Win_post(group, assert, win);
    }

    MTCORE_Assert((uh_win->info_args.epoch_type & MTCORE_EPOCH_PSCW));

    if (group == MPI_GROUP_NULL) {
        /* standard says do nothing for empty group */
        MTCORE_DBG_PRINT("Post empty group\n");
        return mpi_errno;
    }

    mpi_errno = PMPI_Group_size(group, &post_grp_size);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (post_grp_size <= 0) {
        /* standard says do nothing for empty group */
        MTCORE_DBG_PRINT("Post empty group\n");
        return mpi_errno;
    }

    uh_win->post_group = group;
    uh_win->post_ranks_in_win_group = calloc(post_grp_size, sizeof(int));
    MTCORE_DBG_PRINT("post group 0x%x, size %d\n", uh_win->post_group, post_grp_size);

    /* Both lock and start only allow no_check assert. */
    assert = (assert == MPI_MODE_NOCHECK) ? MPI_MODE_NOCHECK : 0;

    mpi_errno = fill_ranks_in_win_grp(uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Synchronize start-post if user does not specify nocheck */
    if ((assert & MPI_MODE_NOCHECK) == 0) {
        mpi_errno = MTCORE_Send_pscw_post_msg(post_grp_size, uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* Need win_sync for synchronizing local window update.
     * It is eliminated if user explicitly specifies no preceding store. */
    if ((assert & MPI_MODE_NOSTORE) == 0) {
        mpi_errno = PMPI_Win_sync(uh_win->active_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    MTCORE_DBG_PRINT("Post done\n");

  fn_exit:
    return mpi_errno;

  fn_fail:
    if (uh_win->post_ranks_in_win_group)
        free(uh_win->post_ranks_in_win_group);
    uh_win->post_group = MPI_GROUP_NULL;
    uh_win->post_ranks_in_win_group = NULL;

    return mpi_errno;
}

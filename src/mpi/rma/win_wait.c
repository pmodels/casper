/*
 * win_wait.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

static int MTCORE_Wait_pscw_complete_msg(int post_grp_size, MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank;
    int i;
    char post_flg;
    MPI_Request *reqs = NULL;
    MPI_Status *stats = NULL;
    int remote_cnt = 0;

    reqs = calloc(post_grp_size, sizeof(MPI_Request));
    stats = calloc(post_grp_size, sizeof(MPI_Status));

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);

    for (i = 0; i < post_grp_size; i++) {
        int origin_rank = uh_win->post_ranks_in_win_group[i];

        /* Do not receive from local target, otherwise it may deadlock.
         * We do not check the wrong sync case that user calls start(self)
         * before/without post(self). */
        if (user_rank == origin_rank)
            continue;

        mpi_errno = PMPI_Irecv(&post_flg, 1, MPI_CHAR, origin_rank,
                               MTCORE_PSCW_CW_TAG, uh_win->user_comm, &reqs[remote_cnt++]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        MTCORE_DBG_PRINT("receive pscw complete msg from target %d \n", origin_rank);
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

int MPI_Win_wait(MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int post_grp_size = 0;
    int i;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    if (uh_win == NULL) {
        /* normal window */
        return PMPI_Win_wait(win);
    }

    MTCORE_Assert((uh_win->info_args.epoch_type & MTCORE_EPOCH_PSCW));

    if (uh_win->post_group == MPI_GROUP_NULL) {
        /* standard says do nothing for empty group */
        MTCORE_DBG_PRINT("Wait empty group\n");
        return mpi_errno;
    }

    mpi_errno = PMPI_Group_size(uh_win->post_group, &post_grp_size);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    MTCORE_Assert(post_grp_size > 0);

    MTCORE_DBG_PRINT("Wait group 0x%x, size %d\n", uh_win->post_group, post_grp_size);

    /* Wait for the completion on all origin processes */
    mpi_errno = MTCORE_Wait_pscw_complete_msg(post_grp_size, uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* TODO: MPI implementation should do memory barrier in flush handler. */
    mpi_errno = PMPI_Win_sync(uh_win->active_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    MTCORE_DBG_PRINT("Wait done\n");

  fn_exit:
    if (uh_win->post_ranks_in_win_group)
        free(uh_win->post_ranks_in_win_group);
    uh_win->post_group = MPI_GROUP_NULL;
    uh_win->post_ranks_in_win_group = NULL;

    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

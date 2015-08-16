/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"
#include "cspu_rma_sync.h"

/* Receive buffer for receiving complete-wait sync message.
 * We don't need its value, so just use a global char variable to ensure
 * receive buffer is always allocated.*/
char wait_flg;

/* Receive complete-wait synchronization messages from origin group.
 * WIN_WAIT calls it with blocking=true; WIN_TEST calls it with blocking=false
 * and a flag pointer. */
int CSP_recv_pscw_complete_msg(int post_grp_size, CSP_win * ug_win, int blocking, int *flag)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank;
    int i;
    MPI_Status *stats = NULL;
    int remote_cnt = 0;
    int new_issue = 0;

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    stats = CSP_calloc(post_grp_size, sizeof(MPI_Status));

    if (ug_win->wait_reqs == NULL) {
        ug_win->wait_reqs = CSP_calloc(post_grp_size, sizeof(MPI_Request));
        new_issue = 1;
    }

    for (i = 0; i < post_grp_size; i++) {
        int origin_rank = ug_win->post_ranks_in_win_group[i];

        /* Do not receive from local target, otherwise it may deadlock.
         * We do not check the wrong sync case that user calls start(self)
         * before/without post(self). */
        if (user_rank == origin_rank)
            continue;

        /* If receive is not issued yet, issue now. Otherwise just wait. */
        if (new_issue) {
            mpi_errno = PMPI_Irecv(&wait_flg, 1, MPI_CHAR, origin_rank, CSP_PSCW_CW_TAG,
                                   ug_win->user_comm, &(ug_win->wait_reqs[remote_cnt]));
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;

            CSP_DBG_PRINT("receive pscw complete msg from target %d \n", origin_rank);
        }
        remote_cnt++;   /* always run this loop to count issued requests. */
    }

    if (blocking) {
        mpi_errno = PMPI_Waitall(remote_cnt, ug_win->wait_reqs, stats);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
    else {
        CSP_assert(flag != NULL);
        mpi_errno = PMPI_Testall(remote_cnt, ug_win->wait_reqs, flag, stats);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* free requests only when all requests are complete. */
    if (blocking || (flag != NULL && (*flag))) {
        free(ug_win->wait_reqs);
        ug_win->wait_reqs = NULL;
    }

  fn_exit:
    if (stats)
        free(stats);
    return mpi_errno;

  fn_fail:
    if (ug_win->wait_reqs)
        free(ug_win->wait_reqs);
    ug_win->wait_reqs = NULL;
    goto fn_exit;
}

int MPI_Win_wait(MPI_Win win)
{
    CSP_win *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int post_grp_size = 0;

    CSP_DBG_PRINT_FCNAME();

    CSP_fetch_ug_win_from_cache(win, ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_wait(win);
    }

    CSP_assert((ug_win->info_args.epoch_type & CSP_EPOCH_PSCW));

    /* Check exposure epoch status.
     * Note that this is not only a user-friendly check, but also
     * used to avoid extra sync messages in wait/test.*/
    if (ug_win->exp_epoch_stat != CSP_WIN_EXP_EPOCH_PSCW) {
        CSP_DBG_PRINT("No pscw exposure epoch\n");
        return mpi_errno;
    }

    if (ug_win->post_group == MPI_GROUP_NULL) {
        /* standard says do nothing for empty group */
        CSP_DBG_PRINT("Wait empty group\n");
        return mpi_errno;
    }

    mpi_errno = PMPI_Group_size(ug_win->post_group, &post_grp_size);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    CSP_assert(post_grp_size > 0);

    CSP_DBG_PRINT("Wait group 0x%x, size %d\n", ug_win->post_group, post_grp_size);

    /* Wait for the completion on all origin processes */
    mpi_errno = CSP_recv_pscw_complete_msg(post_grp_size, ug_win, 1 /*blocking */ , NULL);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* NOTE: MPI implementation should do memory barrier in flush handler. */
    mpi_errno = PMPI_Win_sync(ug_win->global_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Reset exposure status.
     * All later wait/test will return immediately.*/
    ug_win->exp_epoch_stat = CSP_WIN_NO_EXP_EPOCH;

    CSP_DBG_PRINT("Wait done\n");

  fn_exit:
    if (ug_win->post_ranks_in_win_group)
        free(ug_win->post_ranks_in_win_group);
    ug_win->post_group = MPI_GROUP_NULL;
    ug_win->post_ranks_in_win_group = NULL;

    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

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
char CSPU_pscw_wait_flg;

/* Receive complete-wait synchronization messages from origin group.
 * WIN_WAIT calls it with blocking=true; WIN_TEST calls it with blocking=false
 * and a flag pointer. */
int CSPU_recv_pscw_complete_msg(int post_grp_size, CSPU_win_t * ug_win, int blocking, int *flag)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank;
    int i;
    MPI_Status *stats = NULL;
    int remote_cnt = 0;
    int new_issue = 0;

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->user_comm, &user_rank));
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
            CSP_CALLMPI(JUMP, PMPI_Irecv(&CSPU_pscw_wait_flg, 1, MPI_CHAR, origin_rank,
                                         CSPU_PSCW_CW_TAG, ug_win->user_comm,
                                         &(ug_win->wait_reqs[remote_cnt])));

            CSP_DBG_PRINT("receive pscw complete msg from target %d \n", origin_rank);
        }
        remote_cnt++;   /* always run this loop to count issued requests. */
    }

    if (blocking) {
        CSP_CALLMPI(JUMP, PMPI_Waitall(remote_cnt, ug_win->wait_reqs, stats));
    }
    else {
        CSP_ASSERT(flag != NULL);
        CSP_CALLMPI(JUMP, PMPI_Testall(remote_cnt, ug_win->wait_reqs, flag, stats));
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
    CSPU_win_t *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int post_grp_size = 0;

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();
    CSPU_ERRHAN_EXTOBJ_LOCAL_DCL();
    CSPU_WIN_ERRHAN_SET_EXTOBJ();

    CSPU_fetch_ug_win_from_cache(win, &ug_win);

    if (ug_win == NULL) {
        /* normal window */
        CSPU_ERRHAN_RESET_EXTOBJ();     /* reset before calling original MPI */
        return PMPI_Win_wait(win);
    }

    CSPU_THREAD_ENTER_OBJ_CS(ug_win);

    CSP_ASSERT((ug_win->info_args.epochs_used & CSP_EPOCH_PSCW));

    /* Check exposure epoch status.
     * Note that this is not only a user-friendly check, but also
     * used to avoid extra sync messages in wait/test.*/
    if (ug_win->exp_epoch_stat != CSPU_WIN_EXP_EPOCH_PSCW) {
        CSP_msg_print(CSP_MSG_ERROR, "Wrong synchronization call! "
                      "No opening PSCW exposure epoch in %s\n", __FUNCTION__);
        mpi_errno = MPI_ERR_RMA_SYNC;
        goto fn_fail;
    }

    if (ug_win->post_group == MPI_GROUP_NULL) {
        /* standard says do nothing for empty group */
        CSP_DBG_PRINT("Wait empty group\n");
        return mpi_errno;
    }

    CSP_CALLMPI(JUMP, PMPI_Group_size(ug_win->post_group, &post_grp_size));
    CSP_ASSERT(post_grp_size > 0);

    CSP_DBG_PRINT("Wait group 0x%x, size %d\n", ug_win->post_group, post_grp_size);

    /* Wait for the completion on all origin processes */
    mpi_errno = CSPU_recv_pscw_complete_msg(post_grp_size, ug_win, 1 /*blocking */ , NULL);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* NOTE: MPI implementation should do memory barrier in flush handler. */
    CSP_CALLMPI(JUMP, PMPI_Win_sync(ug_win->global_win));

    /* Reset exposure status.
     * All later wait/test will return immediately.*/
    ug_win->exp_epoch_stat = CSPU_WIN_NO_EXP_EPOCH;

    CSP_DBG_PRINT("Wait done\n");

  fn_exit:
    if (ug_win->post_ranks_in_win_group)
        free(ug_win->post_ranks_in_win_group);
    ug_win->post_group = MPI_GROUP_NULL;
    ug_win->post_ranks_in_win_group = NULL;

    CSPU_THREAD_EXIT_OBJ_CS(ug_win);
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before return */
    return mpi_errno;

  fn_fail:
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before error handling */
    CSPU_WIN_ERRHANLDING(win, &mpi_errno);
    goto fn_exit;
}

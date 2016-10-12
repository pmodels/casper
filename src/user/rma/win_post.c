/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

static int fill_ranks_in_win_grp(CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int *ranks_in_post_grp = NULL;
    int i, post_grp_size;

    CSP_CALLMPI(JUMP, PMPI_Group_size(ug_win->post_group, &post_grp_size));

    ranks_in_post_grp = CSP_calloc(post_grp_size, sizeof(int));
    for (i = 0; i < post_grp_size; i++) {
        ranks_in_post_grp[i] = i;
    }

    CSP_CALLMPI(JUMP, PMPI_Group_translate_ranks(ug_win->post_group, post_grp_size,
                                                 ranks_in_post_grp, ug_win->user_group,
                                                 ug_win->post_ranks_in_win_group));

  fn_exit:
    if (ranks_in_post_grp)
        free(ranks_in_post_grp);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int send_pscw_post_msg(int post_grp_size, CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int i, user_rank;
    char post_flg = 1;
    MPI_Request *reqs = NULL;
    MPI_Status *stats = NULL;
    int remote_cnt = 0;

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->user_comm, &user_rank));
    reqs = CSP_calloc(post_grp_size, sizeof(MPI_Request));
    stats = CSP_calloc(post_grp_size, sizeof(MPI_Status));

#ifdef CSP_ENABLE_EAGER_PSCW_SYNC
    /* requests for cw-sync messages */
    ug_win->wait_reqs = CSP_calloc(post_grp_size, sizeof(MPI_Request));
#endif

    for (i = 0; i < post_grp_size; i++) {
        int origin_rank = ug_win->post_ranks_in_win_group[i];

        /* Do not send to local target, otherwise it may deadlock.
         * We do not check the wrong sync case that user calls start(self)
         * before post(self). */
        if (user_rank == origin_rank)
            continue;

        CSP_CALLMPI(JUMP, PMPI_Isend(&post_flg, 1, MPI_CHAR, origin_rank,
                                     CSPU_PSCW_PS_TAG, ug_win->user_comm, &reqs[remote_cnt++]));

        /* Set post flag to true on the main ghost of post origin. */
        CSP_DBG_PRINT("send pscw post msg to origin %d \n", origin_rank);

#ifdef CSP_ENABLE_EAGER_PSCW_SYNC
        /* Eager receive for complete sync. Later wait/test on these requests. */
        CSP_CALLMPI(JUMP, PMPI_Irecv(&CSPU_pscw_wait_flg, 1, MPI_CHAR, origin_rank,
                                     CSPU_PSCW_CW_TAG, ug_win->user_comm,
                                     &(ug_win->wait_reqs[remote_cnt])));
#endif
    }

    /* Has to blocking wait here to poll progress. */
    CSP_CALLMPI(JUMP, PMPI_Waitall(remote_cnt, reqs, stats));

  fn_exit:
    if (reqs)
        free(reqs);
    if (stats)
        free(stats);
    return mpi_errno;

  fn_fail:
#ifdef CSP_ENABLE_EAGER_PSCW_SYNC
    if (ug_win->wait_reqs)
        free(ug_win->wait_reqs);
    ug_win->wait_reqs = NULL;
#endif
    goto fn_exit;
}

int MPI_Win_post(MPI_Group group, int assert, MPI_Win win)
{
    CSPU_win_t *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int post_grp_size = 0;
    int i CSP_ATTRIBUTE((unused));

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();
    CSPU_ERRHAN_EXTOBJ_LOCAL_DCL();
    CSPU_WIN_ERRHAN_SET_EXTOBJ();

    CSPU_fetch_ug_win_from_cache(win, &ug_win);

    if (ug_win == NULL) {
        /* normal window */
        CSPU_ERRHAN_RESET_EXTOBJ();     /* reset before calling original MPI */
        return PMPI_Win_post(group, assert, win);
    }

    CSPU_THREAD_ENTER_OBJ_CS(ug_win);

    CSP_ASSERT((ug_win->info_args.epochs_used & CSP_EPOCH_PSCW));

#ifdef CSP_ENABLE_RMA_ERR_CHECK
    /* Check exposure epoch status.
     * The current epoch can be none or FENCE.
     * We do not require closed FENCE epoch, because we don't know whether
     * the previous FENCE is closed or not.*/
    if (ug_win->exp_epoch_stat == CSPU_WIN_EXP_EPOCH_PSCW) {
        CSP_msg_print(CSP_MSG_ERROR, "Wrong synchronization call! "
                      "Previous PSCW exposure epoch is still open in %s\n", __FUNCTION__);
        mpi_errno = MPI_ERR_RMA_SYNC;
        goto fn_fail;
    }
#endif

    if (group == MPI_GROUP_NULL) {
        /* standard says do nothing for empty group */
        CSP_DBG_PRINT("Post empty group\n");
        return mpi_errno;
    }

    CSP_CALLMPI(JUMP, PMPI_Group_size(group, &post_grp_size));

    if (post_grp_size <= 0) {
        /* standard says do nothing for empty group */
        CSP_DBG_PRINT("Post empty group\n");
        return mpi_errno;
    }

    ug_win->post_group = group;
    ug_win->post_ranks_in_win_group = CSP_calloc(post_grp_size, sizeof(int));
    CSP_DBG_PRINT("post group 0x%x, size %d\n", ug_win->post_group, post_grp_size);

    /* Both lock and start only allow no_check assert. */
    assert = (assert == MPI_MODE_NOCHECK) ? MPI_MODE_NOCHECK : 0;

    mpi_errno = fill_ranks_in_win_grp(ug_win);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

#ifdef CSP_ENABLE_RMA_ERR_CHECK
    for (i = 0; i < post_grp_size; i++) {
        CSPU_TARGET_CHECK_RANK(ug_win->post_ranks_in_win_group[i], ug_win);
    }
#endif

    /* Synchronize start-post if user does not specify nocheck */
    if ((assert & MPI_MODE_NOCHECK) == 0) {
        mpi_errno = send_pscw_post_msg(post_grp_size, ug_win);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

    /* Need win_sync for synchronizing local window update.
     * Still need it to avoid instruction reordering of preceding load
     * even if user says no preceding store. */
    CSP_CALLMPI(JUMP, PMPI_Win_sync(ug_win->global_win));

    /* Indicate exposure epoch status. */
    ug_win->exp_epoch_stat = CSPU_WIN_EXP_EPOCH_PSCW;

    CSP_DBG_PRINT("Post done\n");

  fn_exit:
    CSPU_THREAD_EXIT_OBJ_CS(ug_win);
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before return */
    return mpi_errno;

  fn_fail:
    if (ug_win->post_ranks_in_win_group)
        free(ug_win->post_ranks_in_win_group);
    ug_win->post_group = MPI_GROUP_NULL;
    ug_win->post_ranks_in_win_group = NULL;

    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before error handling */
    CSPU_WIN_ERRHANLDING(win, &mpi_errno);
    goto fn_exit;
}

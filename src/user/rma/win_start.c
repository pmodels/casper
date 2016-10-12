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
    int *ranks_in_start_grp = NULL;
    int i, start_grp_size;

    CSP_CALLMPI(JUMP, PMPI_Group_size(ug_win->start_group, &start_grp_size));

    ranks_in_start_grp = CSP_calloc(start_grp_size, sizeof(int));
    for (i = 0; i < start_grp_size; i++) {
        ranks_in_start_grp[i] = i;
    }

    CSP_CALLMPI(JUMP, PMPI_Group_translate_ranks(ug_win->start_group, start_grp_size,
                                                 ranks_in_start_grp, ug_win->user_group,
                                                 ug_win->start_ranks_in_win_group));

  fn_exit:
    if (ranks_in_start_grp)
        free(ranks_in_start_grp);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int wait_pscw_post_msg(int start_grp_size, CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank;
    int i;
    char post_flg;
    MPI_Request *reqs = NULL;
    MPI_Status *stats = NULL;
    int remote_cnt = 0;

    reqs = CSP_calloc(start_grp_size, sizeof(MPI_Request));
    stats = CSP_calloc(start_grp_size, sizeof(MPI_Status));

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->user_comm, &user_rank));

    for (i = 0; i < start_grp_size; i++) {
        int target_rank = ug_win->start_ranks_in_win_group[i];

        /* Do not receive from local target, otherwise it may deadlock.
         * We do not check the wrong sync case that user calls start(self)
         * before/without post(self). */
        if (user_rank == target_rank)
            continue;

        CSP_CALLMPI(JUMP, PMPI_Irecv(&post_flg, 1, MPI_CHAR, target_rank,
                                     CSPU_PSCW_PS_TAG, ug_win->user_comm, &reqs[remote_cnt++]));

        /* Set post flag to true on the main ghost of post origin. */
        CSP_DBG_PRINT("receive pscw post msg from target %d \n", target_rank);
    }

    /* It is blocking. */
    CSP_CALLMPI(JUMP, PMPI_Waitall(remote_cnt, reqs, stats));

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
    CSPU_win_t *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int start_grp_size = 0;
    int i;
    int user_rank;

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();
    CSPU_ERRHAN_EXTOBJ_LOCAL_DCL();
    CSPU_WIN_ERRHAN_SET_EXTOBJ();

    CSPU_fetch_ug_win_from_cache(win, &ug_win);

    if (ug_win == NULL) {
        /* normal window */
        CSPU_ERRHAN_RESET_EXTOBJ();     /* reset before calling original MPI */
        return PMPI_Win_start(group, assert, win);
    }

    CSPU_THREAD_ENTER_OBJ_CS(ug_win);

    CSP_ASSERT((ug_win->info_args.epochs_used & CSP_EPOCH_PSCW));

    if (ug_win->epoch_stat == CSPU_WIN_EPOCH_FENCE)
        ug_win->is_self_locked = 0;     /* because we cannot reset it in previous FENCE. */

#ifdef CSP_ENABLE_RMA_ERR_CHECK
    /* Check access epoch status.
     * Unlike Lock, PSCW access epoch cannot overlap with any other access epoch
     * including lock to disjoint target, so we pass only when it is NO_EPOCH or FENCE.
     * We do not require closed FENCE epoch, because we don't know whether
     * the previous FENCE is closed or not.*/
    if (ug_win->epoch_stat != CSPU_WIN_NO_EPOCH && ug_win->epoch_stat != CSPU_WIN_EPOCH_FENCE) {
        CSP_msg_print(CSP_MSG_ERROR, "Wrong synchronization call! "
                      "Previous %s access epoch is still open in %s\n",
                      CSPU_WIN_GET_EPOCH_STAT_NAME(ug_win), __FUNCTION__);
        mpi_errno = MPI_ERR_RMA_SYNC;
        goto fn_sync_err;
    }
#endif

    /* Since nested access epoch is not allowed in PSCW, the origin itself must be unlocked. */
    CSP_ASSERT(ug_win->is_self_locked == 0);

    if (group == MPI_GROUP_NULL) {
        /* standard says do nothing for empty group */
        CSP_DBG_PRINT("Start empty group\n");
        return mpi_errno;
    }

    CSP_CALLMPI(JUMP, PMPI_Group_size(group, &start_grp_size));

    if (start_grp_size <= 0) {
        /* standard says do nothing for empty group */
        CSP_DBG_PRINT("Start empty group\n");
        return mpi_errno;
    }

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->user_comm, &user_rank));

    ug_win->start_group = group;
    ug_win->start_ranks_in_win_group = CSP_calloc(start_grp_size, sizeof(int));
    CSP_DBG_PRINT("start group 0x%x, size %d\n", ug_win->start_group, start_grp_size);

    /* Both lock and start only allow no_check assert. */
    assert = (assert == MPI_MODE_NOCHECK) ? MPI_MODE_NOCHECK : 0;

    mpi_errno = fill_ranks_in_win_grp(ug_win);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

#ifdef CSP_ENABLE_RMA_ERR_CHECK
    for (i = 0; i < start_grp_size; i++) {
        CSPU_TARGET_CHECK_RANK(ug_win->start_ranks_in_win_group[i], ug_win);
    }
#endif

    /* Synchronize start-post if user does not specify nocheck */
    if ((assert & MPI_MODE_NOCHECK) == 0) {
        mpi_errno = wait_pscw_post_msg(start_grp_size, ug_win);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

#ifdef CSP_ENABLE_LOCAL_RMA_OP_OPT
    {
        /* Enable local RMA optimization if local target is in the start group. */
        for (i = 0; i < start_grp_size; i++) {
            if (ug_win->start_ranks_in_win_group[i] == user_rank) {
                ug_win->is_self_locked = 1;
                break;
            }
        }
    }
#endif

    /* Indicate epoch status.
     * Later operations will be redirected to global_win for these targets.*/
    for (i = 0; i < start_grp_size; i++) {
        int target_rank = ug_win->start_ranks_in_win_group[i];
        ug_win->targets[target_rank].epoch_stat = CSPU_TARGET_EPOCH_PSCW;
    }
    ug_win->epoch_stat = CSPU_WIN_EPOCH_PER_TARGET;
    ug_win->start_counter++;

    CSP_DBG_PRINT("Start done\n");

  fn_exit:
    CSPU_THREAD_EXIT_OBJ_CS(ug_win);
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before return */
    return mpi_errno;

  fn_fail:
    if (ug_win->start_ranks_in_win_group)
        free(ug_win->start_ranks_in_win_group);
    ug_win->start_group = MPI_GROUP_NULL;
    ug_win->start_ranks_in_win_group = NULL;

  fn_sync_err:
    /* Do not release internal resource if it is RMA sync error.
     * Because these resources are for the existing PSCW epoch, which is correct. */

    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before error handling */
    CSPU_WIN_ERRHANLDING(win, &mpi_errno);
    goto fn_exit;
}

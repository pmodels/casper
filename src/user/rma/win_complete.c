/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"
#include "cspu_rma_sync.h"

static int send_pscw_complete_msg(int start_grp_size, CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int i, user_rank;
    char comp_flg = 1;
    MPI_Request *reqs = NULL;
    MPI_Status *stats = NULL;
    int remote_cnt = 0;

    reqs = CSP_calloc(start_grp_size, sizeof(MPI_Request));
    stats = CSP_calloc(start_grp_size, sizeof(MPI_Status));

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->user_comm, &user_rank));

    for (i = 0; i < start_grp_size; i++) {
        int target_rank = ug_win->start_ranks_in_win_group[i];

        /* Do not send to local target, otherwise it may deadlock.
         * We do not check the wrong sync case that user calls wait(self)
         * before complete(self). */
        if (user_rank == target_rank)
            continue;

        CSP_CALLMPI(JUMP, PMPI_Isend(&comp_flg, 1, MPI_CHAR, target_rank,
                                     CSPU_PSCW_CW_TAG, ug_win->user_comm, &reqs[remote_cnt++]));

        /* Set post flag to true on the main ghost of post origin. */
        CSP_DBG_PRINT("send pscw complete msg to target %d \n", target_rank);
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
    goto fn_exit;
}

int MPI_Win_complete(MPI_Win win)
{
    CSPU_win_t *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int start_grp_size = 0;
    int i;

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();
    CSPU_ERRHAN_EXTOBJ_LOCAL_DCL();
    CSPU_WIN_ERRHAN_SET_EXTOBJ();

    CSPU_fetch_ug_win_from_cache(win, &ug_win);

    if (ug_win == NULL) {
        /* normal window */
        CSPU_ERRHAN_RESET_EXTOBJ();     /* reset before calling original MPI */
        return PMPI_Win_complete(win);
    }

    CSPU_THREAD_ENTER_OBJ_CS(ug_win);

    CSP_ASSERT((ug_win->info_args.epochs_used & CSP_EPOCH_PSCW));

#ifdef CSP_ENABLE_RMA_ERR_CHECK
    /* Check access epoch status.
     * The current epoch must be pscw on all involved targets.*/
    if (ug_win->epoch_stat != CSPU_WIN_EPOCH_PER_TARGET) {
        CSP_msg_print(CSP_MSG_ERROR, "Wrong synchronization call! "
                      "No opening PSCW access epoch in %s\n", __FUNCTION__);
        mpi_errno = MPI_ERR_RMA_SYNC;
        goto fn_fail;
    }
#endif

    if (ug_win->start_group == MPI_GROUP_NULL) {
        /* standard says do nothing for empty group */
        CSP_DBG_PRINT("Complete empty group\n");
        return mpi_errno;
    }

    CSP_CALLMPI(JUMP, PMPI_Group_size(ug_win->start_group, &start_grp_size));
    CSP_ASSERT(start_grp_size > 0);

    CSP_DBG_PRINT("Complete group 0x%x, size %d\n", ug_win->start_group, start_grp_size);

#ifdef CSP_ENABLE_RMA_ERR_CHECK
    /* Check access epoch status.
     * The current epoch must be pscw on all involved targets.*/
    for (i = 0; i < start_grp_size; i++) {
        int target_rank = ug_win->start_ranks_in_win_group[i];
        if (ug_win->targets[target_rank].epoch_stat != CSPU_TARGET_EPOCH_PSCW) {
            CSP_msg_print(CSP_MSG_ERROR, "Wrong synchronization call! "
                          "No opening PSCW access epoch on target %d in %s\n",
                          target_rank, __FUNCTION__);
            mpi_errno = MPI_ERR_RMA_SYNC;
            goto fn_fail;
        }
    }
#endif

    /* Flush ghosts to finish the sequence of locally issued RMA operations. */
    mpi_errno = CSPU_win_global_flush_all(ug_win);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    mpi_errno = send_pscw_complete_msg(start_grp_size, ug_win);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    ug_win->is_self_locked = 0;

    /* Reset per-target epoch status. */
    for (i = 0; i < start_grp_size; i++) {
        int target_rank = ug_win->start_ranks_in_win_group[i];
        ug_win->targets[target_rank].epoch_stat = CSPU_TARGET_NO_EPOCH;
    }

    /* Reset global epoch status. */
    ug_win->start_counter--;
    CSP_ASSERT(ug_win->start_counter >= 0);
    if (ug_win->start_counter == 0 && ug_win->lock_counter == 0) {
        CSP_DBG_PRINT("all per-target epoch are cleared !\n");
        ug_win->epoch_stat = CSPU_WIN_NO_EPOCH;
    }

    CSP_DBG_PRINT("Complete done\n");

  fn_exit:
    if (ug_win->start_ranks_in_win_group)
        free(ug_win->start_ranks_in_win_group);
    ug_win->start_group = MPI_GROUP_NULL;
    ug_win->start_ranks_in_win_group = NULL;

    CSPU_THREAD_EXIT_OBJ_CS(ug_win);
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before return */
    return mpi_errno;

  fn_fail:
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before error handling */
    CSPU_WIN_ERRHANLDING(win, &mpi_errno);
    goto fn_exit;

}

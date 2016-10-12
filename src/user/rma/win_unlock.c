/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"
#include "cspu_rma_sync.h"


/* Unlock ghost processes for a given target.
 * It is called by both WIN_LOCK and WIN_LOCK_ALL (only lock-exist mode). */
int CSPU_win_target_unlock(int target_rank, CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    CSPU_win_target_t *target = NULL;
    int k;
    int user_rank;

    target = &(ug_win->targets[target_rank]);
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->user_comm, &user_rank));

    /* Unlock every ghost on every window for each target. */
    for (k = 0; k < CSP_ENV.num_g; k++) {
        int target_g_rank_in_ug = target->g_ranks_in_ug[k];

        CSP_DBG_PRINT(" unlock(ghost(%d), ug_win 0x%x), instead of "
                      "target rank %d\n", target_g_rank_in_ug, target->ug_win, target_rank);
        CSP_CALLMPI(JUMP, PMPI_Win_unlock(target_g_rank_in_ug, target->ug_win));
    }

    /* If target is itself, we need also release the lock of local rank  */
    if (user_rank == target_rank && ug_win->is_self_locked) {
        mpi_errno = CSPU_win_unlock_self(ug_win);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_unlock(int target_rank, MPI_Win win)
{
    CSPU_win_t *ug_win;
    CSPU_win_target_t *target;
    int mpi_errno = MPI_SUCCESS;
    int user_rank;

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();
    CSPU_ERRHAN_EXTOBJ_LOCAL_DCL();
    CSPU_WIN_ERRHAN_SET_EXTOBJ();

    CSPU_fetch_ug_win_from_cache(win, &ug_win);

    if (ug_win == NULL) {
        /* normal window */
        CSPU_ERRHAN_RESET_EXTOBJ();     /* reset before calling original MPI */
        return PMPI_Win_unlock(target_rank, win);
    }

    CSPU_THREAD_ENTER_OBJ_CS(ug_win);

    if (target_rank == MPI_PROC_NULL)
        goto fn_exit;

    CSP_ASSERT((ug_win->info_args.epochs_used & CSP_EPOCH_LOCK));

    CSPU_TARGET_CHECK_RANK(target_rank, ug_win);

    target = &(ug_win->targets[target_rank]);

#ifdef CSP_ENABLE_RMA_ERR_CHECK
    /* Check access epoch status.
     * The current epoch must be lock on target.*/
    if (ug_win->epoch_stat != CSPU_WIN_EPOCH_PER_TARGET) {
        CSP_msg_print(CSP_MSG_ERROR, "Wrong synchronization call! "
                      "No opening LOCK access epoch in %s\n", __FUNCTION__);
        mpi_errno = MPI_ERR_RMA_SYNC;
        goto fn_fail;
    }

    /* Check per-target access epoch status. */
    if (target->epoch_stat != CSPU_TARGET_EPOCH_LOCK) {
        CSP_msg_print(CSP_MSG_ERROR, "Wrong synchronization call! "
                      "No opening LOCK access epoch on target %d in %s\n", target_rank,
                      __FUNCTION__);
        mpi_errno = MPI_ERR_RMA_SYNC;
        goto fn_fail;
    }
#endif

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->user_comm, &user_rank));
    target->remote_lock_assert = 0;

#ifdef CSP_ENABLE_SYNC_ALL_OPT
    CSP_DBG_PRINT(" unlock_all(ug_win 0x%x), instead of target rank %d\n",
                  target->ug_win, target_rank);
    CSP_CALLMPI(JUMP, PMPI_Win_unlock_all(target->ug_win));
#else
    mpi_errno = CSPU_win_target_unlock(target_rank, ug_win);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);
#endif


#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    target->main_lock_stat = CSPU_MAIN_LOCK_RESET;
#endif

    if (user_rank == target_rank)
        ug_win->is_self_locked = 0;

    /* Reset per-target epoch status. */
    target->epoch_stat = CSPU_TARGET_NO_EPOCH;

    /* Reset global epoch status. */
    ug_win->lock_counter--;
    CSP_ASSERT(ug_win->lock_counter >= 0);
    if (ug_win->start_counter == 0 && ug_win->lock_counter == 0) {
        CSP_DBG_PRINT("all per-target epoch are cleared !\n");
        ug_win->epoch_stat = CSPU_WIN_NO_EPOCH;
    }

  fn_exit:
    CSPU_THREAD_EXIT_OBJ_CS(ug_win);
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before return */
    return mpi_errno;

  fn_fail:
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before error handling */
    CSPU_WIN_ERRHANLDING(win, &mpi_errno);
    goto fn_exit;
}

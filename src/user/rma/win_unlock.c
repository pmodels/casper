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
int CSP_win_target_unlock(int target_rank, CSP_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_win_target_t *target = NULL;
    int k;
    int user_rank;

    target = &(ug_win->targets[target_rank]);
    PMPI_Comm_rank(ug_win->user_comm, &user_rank);

    /* Unlock every ghost on every window for each target. */
    for (k = 0; k < CSP_ENV.num_g; k++) {
        int target_g_rank_in_ug = target->g_ranks_in_ug[k];

        CSP_DBG_PRINT(" unlock(ghost(%d), ug_win 0x%x), instead of "
                      "target rank %d\n", target_g_rank_in_ug, target->ug_win, target_rank);
        mpi_errno = PMPI_Win_unlock(target_g_rank_in_ug, target->ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* If target is itself, we need also release the lock of local rank  */
    if (user_rank == target_rank && ug_win->is_self_locked) {
        mpi_errno = CSP_win_unlock_self(ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_unlock(int target_rank, MPI_Win win)
{
    CSP_win_t *ug_win;
    CSP_win_target_t *target;
    int mpi_errno = MPI_SUCCESS;
    int user_rank;

    CSP_DBG_PRINT_FCNAME();

    CSP_fetch_ug_win_from_cache(win, &ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_unlock(target_rank, win);
    }

    /* casper window starts */

    if (target_rank == MPI_PROC_NULL)
        goto fn_exit;

    CSP_ASSERT((ug_win->info_args.epochs_used & CSP_EPOCH_LOCK));

    target = &(ug_win->targets[target_rank]);

#ifdef CSP_ENABLE_EPOCH_STAT_CHECK
    /* Check access epoch status.
     * The current epoch must be lock on target.*/
    if (ug_win->epoch_stat != CSP_WIN_EPOCH_PER_TARGET) {
        CSP_ERR_PRINT("Wrong synchronization call! "
                      "No opening per-target epoch in %s\n", __FUNCTION__);
        mpi_errno = -1;
        goto fn_fail;
    }

    /* Check per-target access epoch status. */
    if (target->epoch_stat != CSP_TARGET_EPOCH_LOCK) {
        CSP_ERR_PRINT("Wrong synchronization call! "
                      "No opening LOCK epoch on target %d in %s\n", target_rank, __FUNCTION__);
        mpi_errno = -1;
        goto fn_fail;
    }
#endif

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    target->remote_lock_assert = 0;

#ifdef CSP_ENABLE_SYNC_ALL_OPT
    CSP_DBG_PRINT(" unlock_all(ug_win 0x%x), instead of target rank %d\n",
                  target->ug_win, target_rank);
    mpi_errno = PMPI_Win_unlock_all(target->ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
#else
    mpi_errno = CSP_win_target_unlock(target_rank, ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
#endif


#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    target->main_lock_stat = CSP_MAIN_LOCK_RESET;
#endif

    if (user_rank == target_rank)
        ug_win->is_self_locked = 0;

    /* Reset per-target epoch status. */
    target->epoch_stat = CSP_TARGET_NO_EPOCH;

    /* Reset global epoch status. */
    ug_win->lock_counter--;
    CSP_ASSERT(ug_win->lock_counter >= 0);
    if (ug_win->start_counter == 0 && ug_win->lock_counter == 0) {
        CSP_DBG_PRINT("all per-target epoch are cleared !\n");
        ug_win->epoch_stat = CSP_WIN_NO_EPOCH;
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

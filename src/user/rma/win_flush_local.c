/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"
#include "cspu_rma_sync.h"

/* Locally flush ghost process for a given target.
 * It is called by FLUSH_LOCAL (all modes), and FLUSH_LOCAL_ALL (only lock-exist mode). */
int CSP_win_target_flush_local(int target_rank, CSP_win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_win_target *target = NULL;
    MPI_Win *win_ptr = NULL;
    int user_rank;
    int j;

    target = &(ug_win->targets[target_rank]);
    PMPI_Comm_rank(ug_win->user_comm, &user_rank);

    /* Get global window or a target window for no-lock mode or
     * lock-exist mode respectively. */
    CSP_target_get_epoch_win(0, target, ug_win, win_ptr);
    CSP_assert(win_ptr != NULL);

#if !defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    /* RMA operations are only issued to the main ghost, so we only flush it. */
    /* TODO: track op issuing, only flush the ghosts which received ops. */
    for (j = 0; j < target->num_segs; j++) {
        int main_g_off = target->segs[j].main_g_off;
        int target_g_rank_in_ug = target->g_ranks_in_ug[main_g_off];

        CSP_DBG_PRINT(" flush_local(ghost(%d), %s 0x%x), instead of target rank %d seg %d\n",
                      target_g_rank_in_ug, CSP_get_win_type(*win_ptr, ug_win), *win_ptr,
                      target_rank, j);

        mpi_errno = PMPI_Win_flush_local(target_g_rank_in_ug, *win_ptr);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
#else
    /* RMA operations may be distributed to all ghosts, so we should
     * flush all ghosts on all windows. */
    int k;
    for (k = 0; k < CSP_ENV.num_g; k++) {
        int target_g_rank_in_ug = target->g_ranks_in_ug[k];

        CSP_DBG_PRINT(" flush(ghost(%d), %s 0x%x), instead of target rank %d\n",
                      target_g_rank_in_ug, CSP_get_win_type(*win_ptr, ug_win), *win_ptr,
                      target_rank);

        mpi_errno = PMPI_Win_flush_local(target_g_rank_in_ug, *win_ptr);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
#endif /*end of CSP_ENABLE_RUNTIME_LOAD_OPT */

    if (user_rank == target_rank && ug_win->is_self_locked) {
        int global_win_flag = !(ug_win->info_args.epoch_type & CSP_EPOCH_LOCK);
        mpi_errno = CSP_win_flush_local_self(ug_win, global_win_flag);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_flush_local(int target_rank, MPI_Win win)
{
    CSP_win *ug_win;
    CSP_win_target *target;
    int mpi_errno = MPI_SUCCESS;

    CSP_DBG_PRINT_FCNAME();

    CSP_fetch_ug_win_from_cache(win, ug_win);
    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_flush_local(target_rank, win);
    }

    /* casper window starts */
    if (target_rank == MPI_PROC_NULL)
        goto fn_exit;

    CSP_assert((ug_win->info_args.epoch_type & CSP_EPOCH_LOCK) ||
               (ug_win->info_args.epoch_type & CSP_EPOCH_LOCK_ALL));

    target = &(ug_win->targets[target_rank]);

#ifdef CSP_ENABLE_EPOCH_STAT_CHECK
    /* Check access epoch status.
     * The current epoch must be lock_all or lock.*/
    if (ug_win->epoch_stat != CSP_WIN_EPOCH_LOCK_ALL &&
        (target->epoch_stat != CSP_TARGET_EPOCH_LOCK)) {
        CSP_ERR_PRINT("Wrong synchronization call! "
                      "No opening LOCK_ALL or LOCK epoch in %s\n", __FUNCTION__);
        mpi_errno = -1;
        goto fn_fail;
    }
#endif

#ifdef CSP_ENABLE_SYNC_ALL_OPT
    CSP_DBG_PRINT(" flush_local_all(ug_win 0x%x), instead of target rank %d\n",
                  target->ug_win, target_rank);
    mpi_errno = PMPI_Win_flush_local_all(target->ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
#else
    mpi_errno = CSP_win_target_flush_local(target_rank, ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
#endif

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}
/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"
#include "cspu_rma_sync.h"

int MPI_Win_lock_all(int assert, MPI_Win win)
{
    CSPU_win_t *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int user_nprocs;
    int i;

    CSPU_fetch_ug_win_from_cache(win, &ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_lock_all(assert, win);
    }

    /* casper window starts */

    CSP_ASSERT((ug_win->info_args.epochs_used & CSP_EPOCH_LOCK) ||
               (ug_win->info_args.epochs_used & CSP_EPOCH_LOCK_ALL));

    if (ug_win->epoch_stat == CSPU_WIN_EPOCH_FENCE)
        ug_win->is_self_locked = 0;     /* because we cannot reset it in previous FENCE. */

#ifdef CSP_ENABLE_EPOCH_STAT_CHECK
    /* Check access epoch status.
     * We do not require closed FENCE epoch, because we don't know whether
     * the previous FENCE is closed or not.*/
    if (ug_win->epoch_stat == CSPU_WIN_EPOCH_LOCK_ALL
        || ug_win->epoch_stat == CSPU_WIN_EPOCH_PER_TARGET) {
        CSP_msg_print(CSP_MSG_ERROR, "Wrong synchronization call! "
                      "Previous %s epoch is still open in %s\n",
                      (ug_win->epoch_stat == CSPU_WIN_EPOCH_LOCK_ALL) ? "LOCK_ALL" : "PER_TARGET",
                      __FUNCTION__);
        mpi_errno = MPI_ERR_RMA_SYNC;
        goto fn_fail;
    }
    CSP_ASSERT(ug_win->is_self_locked == 0);
    CSP_ASSERT(ug_win->start_counter == 0 && ug_win->lock_counter == 0);
#endif

    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);

    for (i = 0; i < user_nprocs; i++) {
        ug_win->targets[i].remote_lock_assert = assert;
    }

    CSP_DBG_PRINT(" lock_all, MPI_MODE_NOCHECK %d(assert %d)\n", (assert & MPI_MODE_NOCHECK) != 0,
                  assert);

    if (!(ug_win->info_args.epochs_used & CSP_EPOCH_LOCK)) {
        /* In no-lock epoch, lock_all already issued on global window
         * in win_allocate. */
        CSP_DBG_PRINT(" lock_all(global_win 0x%x) (no actual lock call)\n", ug_win->global_win);

        /* Do not need grant local lock, because only shared lock in current epoch.
         * But memory consistency is still necessary for local load/store. */
        mpi_errno = PMPI_Win_sync(ug_win->global_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        ug_win->is_self_locked = 1;
    }
    else {
        /* In lock-exist epoch, separate windows are bound with targets. */

        /* Lock ghost processes for all target processes. */
#ifdef CSP_ENABLE_SYNC_ALL_OPT
        for (i = 0; i < ug_win->num_ug_wins; i++) {
            CSP_DBG_PRINT(" lock_all(ug_win 0x%x)\n", ug_win->ug_wins[i]);
            mpi_errno = PMPI_Win_lock_all(assert, ug_win->ug_wins[i]);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
        ug_win->is_self_locked = 1;
#else
        for (i = 0; i < user_nprocs; i++) {
            mpi_errno = CSPU_win_target_lock(MPI_LOCK_SHARED, assert, i, ug_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
#endif
    }

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    for (i = 0; i < user_nprocs; i++) {
        ug_win->targets[i].main_lock_stat = CSPU_MAIN_LOCK_RESET;
        CSPU_reset_target_opload(i, ug_win);
    }
#endif

    /* Indicate epoch status.
     * Later operations will be redirected to single window.*/
    ug_win->epoch_stat = CSPU_WIN_EPOCH_LOCK_ALL;

  fn_exit:
    return mpi_errno;

  fn_fail:
    CSPU_WIN_ERROR_RETURN(ug_win, mpi_errno);
    goto fn_exit;
}

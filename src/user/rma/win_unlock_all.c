/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"
#include "cspu_rma_sync.h"

int MPI_Win_unlock_all(MPI_Win win)
{
    CSP_win *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int user_nprocs;
    int i;

    CSP_DBG_PRINT_FCNAME();

    CSP_fetch_ug_win_from_cache(win, ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_unlock_all(win);
    }

    /* casper window starts */

    CSP_assert((ug_win->info_args.epoch_type & CSP_EPOCH_LOCK) ||
               (ug_win->info_args.epoch_type & CSP_EPOCH_LOCK_ALL));

#ifdef CSP_ENABLE_EPOCH_STAT_CHECK
    /* Check access epoch status.
     * The current epoch must be lock_all.*/
    if (ug_win->epoch_stat != CSP_WIN_EPOCH_LOCK_ALL) {
        CSP_ERR_PRINT("Wrong synchronization call! "
                      "No opening LOCK_ALL epoch in %s\n", __FUNCTION__);
        mpi_errno = -1;
        goto fn_fail;
    }
    CSP_assert(ug_win->start_counter == 0 && ug_win->lock_counter == 0);
#endif

    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);

    for (i = 0; i < user_nprocs; i++) {
        ug_win->targets[i].remote_lock_assert = 0;
    }

    if (!(ug_win->info_args.epoch_type & CSP_EPOCH_LOCK)) {
        /* In no-lock epoch, unlock_all will be issued on global window
         * in win_free. We only need flush_all here.*/

        CSP_DBG_PRINT(" unlock_all(global_win 0x%x) (no actual unlock call)\n", ug_win->global_win);
        mpi_errno = CSP_win_global_flush_all(ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* memory consistency for local load/store. */
        mpi_errno = PMPI_Win_sync(ug_win->global_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
    else {
        /* In lock-exist epoch, separate windows are bound with targets. */

#ifdef CSP_ENABLE_SYNC_ALL_OPT
        for (i = 0; i < ug_win->num_ug_wins; i++) {
            CSP_DBG_PRINT(" unlock_all(ug_win 0x%x)\n", ug_win->ug_wins[i]);
            mpi_errno = PMPI_Win_unlock_all(ug_win->ug_wins[i]);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

#else
        for (i = 0; i < user_nprocs; i++) {
            mpi_errno = CSP_win_target_unlock(i, ug_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
#endif
    }

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    int j;
    for (i = 0; i < user_nprocs; i++) {
        for (j = 0; j < ug_win->targets[i].num_segs; j++) {
            ug_win->targets[i].segs[j].main_lock_stat = CSP_MAIN_LOCK_RESET;
        }
    }
#endif

    ug_win->is_self_locked = 0;

    /* Reset epoch status. */
    ug_win->epoch_stat = CSP_WIN_NO_EPOCH;

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}
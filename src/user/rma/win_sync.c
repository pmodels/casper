/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Win_sync(MPI_Win win)
{
    CSP_win *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank = 0, user_nprocs = 0;
    int i;

    CSP_DBG_PRINT_FCNAME();

    CSP_fetch_ug_win_from_cache(win, ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_sync(win);
    }

    /* casper window starts */

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);

    /* For no-lock window, just sync on single window. */
    if (!(ug_win->info_args.epochs_used & CSP_EPOCH_LOCK)) {
#ifdef CSP_ENABLE_EPOCH_STAT_CHECK
        /* Check access epoch status.
         * The current epoch must be lock_all.*/
        if (ug_win->epoch_stat != CSP_WIN_EPOCH_LOCK_ALL) {
            CSP_ERR_PRINT("Wrong synchronization call! "
                          "No opening LOCK_ALL epoch in %s\n", __FUNCTION__);
            mpi_errno = -1;
            goto fn_fail;
        }
#endif

        mpi_errno = PMPI_Win_sync(ug_win->global_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        CSP_DBG_PRINT(" win sync on %s single win 0x%x\n",
                      CSP_win_epoch_stat_name[ug_win->epoch_stat], ug_win->global_win);
    }

    /* For window that may contain locks, we should sync on all per-target windows
     * that are involved in opened lock epoch.*/
    else if (ug_win->epoch_stat == CSP_WIN_EPOCH_PER_TARGET) {
        CSP_win_target *target = NULL;
        int synced CSP_ATTRIBUTE((unused)) = 0;
        PMPI_Comm_size(ug_win->user_comm, &user_nprocs);

        for (i = 0; i < user_nprocs; i++) {
            target = &ug_win->targets[i];
            if (target->epoch_stat == CSP_TARGET_EPOCH_LOCK) {
                mpi_errno = PMPI_Win_sync(target->ug_win);
                if (mpi_errno != MPI_SUCCESS)
                    goto fn_fail;

                CSP_DBG_PRINT(" win sync on %s target %d, win 0x%x\n",
                              CSP_target_epoch_stat_name[target->epoch_stat], i, target->ug_win);
                synced = 1;
            }
        }

#ifdef CSP_ENABLE_EPOCH_STAT_CHECK
        /* Check access epoch status.
         * At least one target must be locked.*/
        if (synced == 0) {
            CSP_ERR_PRINT("Wrong synchronization call! "
                          "No opening LOCK epoch in %s\n", __FUNCTION__);
            mpi_errno = -1;
            goto fn_fail;
        }
#endif
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

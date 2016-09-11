/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"
#include "cspu_rma_sync.h"

/* Flush all ghost processes on global window.
 * It is called by FENCE/COMPLETE, or UNLOCK_ALL/FLUSH_ALL (only no-Lock mode).*/
int CSP_win_global_flush_all(CSP_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int i CSP_ATTRIBUTE((unused));

#ifdef CSP_ENABLE_SYNC_ALL_OPT
    CSP_DBG_PRINT(" flush_all(global_win 0x%x)\n", ug_win->global_win);
    mpi_errno = PMPI_Win_flush_all(ug_win->global_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
#else
    /* Flush every ghost once in the single window.
     * TODO: track op issuing, only flush the ghosts which receive ops. */
    for (i = 0; i < ug_win->num_g_ranks_in_ug; i++) {
        CSP_DBG_PRINT(" flush(ghost %d, global_win 0x%x)\n", ug_win->g_ranks_in_ug[i],
                      ug_win->global_win);
        mpi_errno = PMPI_Win_flush(ug_win->g_ranks_in_ug[i], ug_win->global_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    if (ug_win->is_self_locked) {
        mpi_errno = CSP_win_flush_self(ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
#endif

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_flush_all(MPI_Win win)
{
    CSP_win_t *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int user_nprocs;
    int i;

    CSP_DBG_PRINT_FCNAME();

    CSP_fetch_ug_win_from_cache(win, ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_flush_all(win);
    }

    /* casper window starts */

    CSP_assert((ug_win->info_args.epochs_used & CSP_EPOCH_LOCK) ||
               (ug_win->info_args.epochs_used & CSP_EPOCH_LOCK_ALL));

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

    if (!(ug_win->info_args.epochs_used & CSP_EPOCH_LOCK)) {
        /* In no-lock epoch, single window is shared by multiple targets. */
        mpi_errno = CSP_win_global_flush_all(ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
    else {
        /* In lock-exist epoch, separate windows are bound with targets. */

#ifdef CSP_ENABLE_SYNC_ALL_OPT
        for (i = 0; i < ug_win->num_ug_wins; i++) {
            CSP_DBG_PRINT(" flush_all(ug_win 0x%x)\n", ug_win->ug_wins[i]);
            mpi_errno = PMPI_Win_flush_all(ug_win->ug_wins[i]);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
#else
        for (i = 0; i < user_nprocs; i++) {
            mpi_errno = CSP_win_target_flush(i, ug_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
#endif /*end of CSP_ENABLE_SYNC_ALL_OPT */
    }

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    int j;
    for (i = 0; i < user_nprocs; i++) {
        for (j = 0; j < ug_win->targets[i].num_segs; j++) {
            /* Lock of main ghost is granted, we can start load balancing from the next flush/unlock.
             * Note that only target which was issued operations to is guaranteed to be granted. */
            if (ug_win->targets[i].segs[j].main_lock_stat == CSP_MAIN_LOCK_OP_ISSUED) {
                ug_win->targets[i].segs[j].main_lock_stat = CSP_MAIN_LOCK_GRANTED;
                CSP_DBG_PRINT(" main lock (rank %d, seg %d) granted\n", i, j);
            }

            CSP_reset_target_opload(i, ug_win);
        }
    }
#endif

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

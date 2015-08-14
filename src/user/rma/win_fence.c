/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

static int CSP_fence_flush_all(CSP_win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs;
    int i;

    CSP_DBG_PRINT_FCNAME();

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);

    /* Flush all ghosts to finish the sequence of locally issued RMA operations */
#ifdef CSP_ENABLE_SYNC_ALL_OPT
    CSP_DBG_PRINT("[%d]flush_all(active_win 0x%x)\n", user_rank, ug_win->active_win);
    mpi_errno = PMPI_Win_flush_all(ug_win->active_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
#else
    /* TODO: track op issuing, only flush the ghosts which receive ops. */
    for (i = 0; i < ug_win->num_g_ranks_in_ug; i++) {
        mpi_errno = PMPI_Win_flush(ug_win->g_ranks_in_ug[i], ug_win->active_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

#ifdef CSP_ENABLE_LOCAL_LOCK_OPT
    mpi_errno = PMPI_Win_flush(ug_win->my_rank_in_ug_comm, ug_win->active_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
#endif

#endif

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    int j;
    for (i = 0; i < user_nprocs; i++) {
        for (j = 0; j < ug_win->targets[i].num_segs; j++) {
            /* Runtime load balancing is allowed in fence epoch because
             * 1. fence is a global collective call, all targets already "exposed" their epoch.
             * 2. no conflicting lock/lockall on fence window. */
            ug_win->targets[i].segs[j].main_lock_stat = CSP_MAIN_LOCK_GRANTED;
            CSP_reset_target_opload(i, ug_win);
        }
    }
#endif

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_fence(int assert, MPI_Win win)
{
    CSP_win *ug_win;
    int mpi_errno = MPI_SUCCESS;

    CSP_DBG_PRINT_FCNAME();

    CSP_fetch_ug_win_from_cache(win, ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_fence(assert, win);
    }

    CSP_assert((ug_win->info_args.epoch_type & CSP_EPOCH_FENCE));

#ifdef CSP_ENABLE_EPOCH_STAT_CHECK
    /* Check access epoch status.
     * We do not require closed FENCE epoch, because we don't know whether
     * the previous FENCE is closed or not.*/
    if (ug_win->epoch_stat == CSP_WIN_EPOCH_LOCK_ALL
        || ug_win->epoch_stat == CSP_WIN_EPOCH_PER_TARGET) {
        CSP_ERR_PRINT("Wrong synchronization call! "
                      "Previous %s epoch is still open in %s\n",
                      (ug_win->epoch_stat == CSP_WIN_EPOCH_LOCK_ALL) ? "LOCK_ALL" : "PER_TARGET",
                      __FUNCTION__);
        mpi_errno = -1;
        goto fn_fail;
    }
    CSP_assert(ug_win->start_counter == 0 && ug_win->lock_counter == 0);

    /* Check exposure epoch status.
     * The current epoch can be none or FENCE.*/
    if (ug_win->exp_epoch_stat == CSP_WIN_EXP_EPOCH_PSCW) {
        CSP_ERR_PRINT("Wrong synchronization call! "
                      "Previous PSCW exposure epoch is still open in %s\n", __FUNCTION__);
        mpi_errno = -1;
        goto fn_fail;
    }
#endif

    /* Eliminate flush_all if user explicitly specifies no preceding RMA calls. */
    if ((assert & MPI_MODE_NOPRECEDE) == 0) {
        mpi_errno = CSP_fence_flush_all(ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* Eliminate win_sync if user explicitly specifies no preceding store.
     * Still need it to avoid instruction reordering of preceding load even if
     * user says no preceding store.*/
    mpi_errno = PMPI_Win_sync(ug_win->active_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Cannot eliminate barrier for either no_precede or no_succeed.
     * In no_precede fence, it is used for synchronization between local store
     * and remote RMA; In no_succeed fence, it is also required to wait for
     * remote RMA completion.
     * The only time it is safe to drop it is when user specifies
     * noprecede + nostore + noput which means everyone is doing load/get. */
    if ((assert & MPI_MODE_NOPRECEDE & MPI_MODE_NOSTORE & MPI_MODE_NOPUT) == 0) {
        mpi_errno = PMPI_Barrier(ug_win->user_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    ug_win->is_self_locked = 0;
#ifdef CSP_ENABLE_LOCAL_LOCK_OPT
    /* During fence epoch, it is allowed to access local target directly */
    ug_win->is_self_locked = 1;
#endif

    /* Indicate epoch status.
     * Later operations will be redirected to active_win */
    ug_win->epoch_stat = CSP_WIN_EPOCH_FENCE;

    /* Indicate exposure epoch status. */
    ug_win->exp_epoch_stat = CSP_WIN_EXP_EPOCH_FENCE;

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

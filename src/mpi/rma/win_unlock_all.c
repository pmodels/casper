/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "csp.h"
#include "csp_rma_local.h"

static int CSP_win_mixed_unlock_all_impl(CSP_win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs;
    int i, k;

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);

#ifdef CSP_ENABLE_SYNC_ALL_OPT

    /* Optimization for MPI implementations that have optimized lock_all.
     * However, user should be noted that, if MPI implementation issues lock messages
     * for every target even if it does not have any operation, this optimization
     * could lose performance and even lose asynchronous! */
    for (i = 0; i < ug_win->num_ug_wins; i++) {
        CSP_DBG_PRINT("[%d]unlock_all(ug_win 0x%x)\n", user_rank, ug_win->ug_wins[i]);
        mpi_errno = PMPI_Win_unlock_all(ug_win->ug_wins[i]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
#else
    for (i = 0; i < user_nprocs; i++) {
        for (k = 0; k < CSP_ENV.num_g; k++) {
            int target_g_rank_in_ug = ug_win->targets[i].g_ranks_in_ug[k];

            CSP_DBG_PRINT("[%d]unlock(Ghost(%d), ug_win 0x%x), instead of "
                          "target rank %d\n", user_rank, target_g_rank_in_ug,
                          ug_win->targets[i].ug_win, i);
            mpi_errno = PMPI_Win_unlock(target_g_rank_in_ug, ug_win->targets[i].ug_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
    }
#endif

    if (ug_win->is_self_locked) {
        mpi_errno = CSP_win_unlock_self_impl(ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_unlock_all(MPI_Win win)
{
    CSP_win *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs;
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

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);

    for (i = 0; i < user_nprocs; i++) {
        ug_win->targets[i].remote_lock_assert = 0;
    }

    if (!(ug_win->info_args.epoch_type & CSP_EPOCH_LOCK)) {

        /* In lock_all only epoch, unlock all ghosts on the single window. */

#ifdef CSP_ENABLE_SYNC_ALL_OPT

        /* Optimization for MPI implementations that have optimized lock_all.
         * However, user should be noted that, if MPI implementation issues lock messages
         * for every target even if it does not have any operation, this optimization
         * could lose performance and even lose asynchronous! */
        CSP_DBG_PRINT("[%d]unlock_all(ug_win 0x%x)\n", user_rank, ug_win->ug_wins[0]);
        mpi_errno = PMPI_Win_unlock_all(ug_win->ug_wins[0]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
#else
        mpi_errno = PMPI_Win_unlock_all(ug_win->ug_wins[0]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
#if 0   /* segmentation fault */
        for (i = 0; i < ug_win->num_g_ranks_in_ug; i++) {
            mpi_errno = PMPI_Win_unlock(ug_win->g_ranks_in_ug[i], ug_win->ug_wins[0]);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
#endif
#endif

#ifdef CSP_ENABLE_LOCAL_LOCK_OPT
#if 0   /* segmentation fault */
        mpi_errno = CSP_win_unlock_self_impl(ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
#else
        ug_win->is_self_locked = 0;
#endif
#endif
    }
    else {

        /* In lock_all/lock mixed epoch, separate windows are bound with each target. */
        mpi_errno = CSP_win_mixed_unlock_all_impl(ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    int j;
    for (i = 0; i < user_nprocs; i++) {
        for (j = 0; j < ug_win->targets[i].num_segs; j++) {
            ug_win->targets[i].segs[j].main_lock_stat = CSP_MAIN_LOCK_RESET;
        }
    }
#endif

    /* Reset epoch status. */
    ug_win->epoch_stat = CSP_WIN_NO_EPOCH;

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

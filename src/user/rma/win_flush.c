/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"
#include "cspu_rma_local.h"


int MPI_Win_flush(int target_rank, MPI_Win win)
{
    CSP_win *ug_win;
    CSP_win_target *target;
    int mpi_errno = MPI_SUCCESS;
    int user_rank;
    int j;

    CSP_DBG_PRINT_FCNAME();

    CSP_fetch_ug_win_from_cache(win, ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_flush(target_rank, win);
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

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
#ifdef CSP_ENABLE_LOCAL_LOCK_OPT
    if (user_rank == target_rank) {
        /* If LOCAL_LOCK_OPT is enabled, PUT/GET may be issued to local
         * target. Thus we need flush the local target as well.
         * Note that ACC operations are always issued to main ghost,
         * since atomicity and ordering issue. */
        mpi_errno = CSP_win_flush_self_impl(ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
#endif

    {
#ifdef CSP_ENABLE_SYNC_ALL_OPT

        /* Optimization for MPI implementations that have optimized lock_all.
         * However, user should be noted that, if MPI implementation issues lock messages
         * for every target even if it does not have any operation, this optimization
         * could lose performance and even lose asynchronous! */
        CSP_DBG_PRINT("[%d]flush_all(ug_win 0x%x), instead of target rank %d\n",
                      user_rank, target->ug_win, target_rank);
        mpi_errno = PMPI_Win_flush_all(target->ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
#else

#if !defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
        /* RMA operations are only issued to the main ghost, so we only flush it. */
        /* TODO: track op issuing, only flush the ghosts which receive ops. */
        for (j = 0; j < target->num_segs; j++) {
            int main_g_off = target->segs[j].main_g_off;
            int target_g_rank_in_ug = target->g_ranks_in_ug[main_g_off];
            CSP_DBG_PRINT("[%d]flush(Ghost(%d), ug_wins 0x%x), instead of "
                          "target rank %d seg %d\n", user_rank, target_g_rank_in_ug,
                          target->segs[j].ug_win, target_rank, j);

            mpi_errno = PMPI_Win_flush(target_g_rank_in_ug, target->segs[j].ug_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

#else
        /* RMA operations may be distributed to all ghosts, so we should
         * flush all ghosts on all windows.
         *
         * Note that some flushes could be eliminated before the main lock of a
         * segment granted (see above). However, we have to loop all the segments
         * in order to check each lock status, and we may flush the same ghost
         * on the same window twice if the lock is granted on that segment.
         * i.e., flush (H0, win0) and (H1, win0) twice for seg0 and seg1.
         *
         * Consider flush does nothing if no operations on that target in most
         * MPI implementation, simpler code is better */
        int k;
        for (k = 0; k < CSP_ENV.num_g; k++) {
            int target_g_rank_in_ug = target->g_ranks_in_ug[k];
            CSP_DBG_PRINT("[%d]flush(Ghost(%d), ug_wins 0x%x), instead of "
                          "target rank %d\n", user_rank, target_g_rank_in_ug,
                          target->ug_win, target_rank);

            mpi_errno = PMPI_Win_flush(target_g_rank_in_ug, target->ug_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
#endif /*end of CSP_ENABLE_RUNTIME_LOAD_OPT */
#endif /*end of CSP_ENABLE_SYNC_ALL_OPT */
    }

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    for (j = 0; j < target->num_segs; j++) {
        /* Lock of main ghost is granted, we can start load balancing from the next flush/unlock.
         * Note that only target which was issued operations to is guaranteed to be granted. */
        if (target->segs[j].main_lock_stat == CSP_MAIN_LOCK_OP_ISSUED) {
            target->segs[j].main_lock_stat = CSP_MAIN_LOCK_GRANTED;
            CSP_DBG_PRINT("[%d] main lock (rank %d, seg %d) granted\n", user_rank, target_rank, j);
        }

        CSP_reset_target_opload(target_rank, ug_win);
    }
#endif

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

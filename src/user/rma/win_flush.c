/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"
#include "cspu_rma_sync.h"

/* Flush ghost process for a given target.
 * It is called by FLUSH (all modes), and FLUSH_ALL (only lock-exist mode). */
int CSPU_win_target_flush(int target_rank, CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    CSPU_win_target_t *target = NULL;
    MPI_Win *win_ptr = NULL;
    int user_rank;
    int target_g_rank_in_ug;
    int main_g_off CSP_ATTRIBUTE((unused));
    int k CSP_ATTRIBUTE((unused));

    target = &(ug_win->targets[target_rank]);
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->user_comm, &user_rank));

    /* Get global window or a target window for no-lock mode or
     * lock-exist mode respectively. */
    CSPU_TARGET_GET_EPOCH_WIN(target, ug_win, win_ptr);
    CSP_ASSERT(win_ptr != NULL);

#if !defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    /* RMA operations are only issued to the main ghost, so we only flush it. */
    /* TODO: track op issuing, only flush the ghosts which received ops. */
    main_g_off = target->main_g_off;
    target_g_rank_in_ug = target->g_ranks_in_ug[main_g_off];

    CSP_DBG_PRINT(" flush(ghost(%d), %s 0x%x), instead of target rank %d\n",
                  target_g_rank_in_ug, CSPU_GET_WIN_TYPE(*win_ptr, ug_win), *win_ptr, target_rank);

    CSP_CALLMPI(JUMP, PMPI_Win_flush(target_g_rank_in_ug, *win_ptr));
#else
    /* RMA operations may be distributed to all ghosts, so we should
     * flush all ghosts on all windows. Consider flush does nothing if no
     * operations on that target in most MPI implementation, simpler code is better */
    for (k = 0; k < CSP_ENV.num_g; k++) {
        target_g_rank_in_ug = target->g_ranks_in_ug[k];

        CSP_DBG_PRINT(" flush(ghost(%d), %s 0x%x), instead of target rank %d\n",
                      target_g_rank_in_ug, CSPU_GET_WIN_TYPE(*win_ptr, ug_win), *win_ptr,
                      target_rank);

        CSP_CALLMPI(JUMP, PMPI_Win_flush(target_g_rank_in_ug, *win_ptr));
    }
#endif /*end of CSP_ENABLE_RUNTIME_LOAD_OPT */

    if (user_rank == target_rank && ug_win->is_self_locked) {
        mpi_errno = CSPU_win_flush_self(ug_win);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_flush(int target_rank, MPI_Win win)
{
    CSPU_win_t *ug_win;
    CSPU_win_target_t *target CSP_ATTRIBUTE((unused));
    MPI_Win *win_ptr CSP_ATTRIBUTE((unused)) = NULL;
    int mpi_errno = MPI_SUCCESS;

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();
    CSPU_ERRHAN_EXTOBJ_LOCAL_DCL();
    CSPU_WIN_ERRHAN_SET_EXTOBJ();

    CSPU_fetch_ug_win_from_cache(win, &ug_win);

    if (ug_win == NULL) {
        /* normal window */
        CSPU_ERRHAN_RESET_EXTOBJ();     /* reset before calling original MPI */
        return PMPI_Win_flush(target_rank, win);
    }

    CSPU_THREAD_ENTER_OBJ_CS(ug_win);

    if (target_rank == MPI_PROC_NULL)
        goto fn_exit;

    CSP_ASSERT((ug_win->info_args.epochs_used & CSP_EPOCH_LOCK) ||
               (ug_win->info_args.epochs_used & CSP_EPOCH_LOCK_ALL));

    CSPU_TARGET_CHECK_RANK(target_rank, ug_win);

    target = &(ug_win->targets[target_rank]);

#ifdef CSP_ENABLE_RMA_ERR_CHECK
    /* Check access epoch status.
     * The current epoch must be lock_all or lock.*/
    if (ug_win->epoch_stat != CSPU_WIN_EPOCH_LOCK_ALL &&
        (target->epoch_stat != CSPU_TARGET_EPOCH_LOCK)) {
        CSP_msg_print(CSP_MSG_ERROR, "Wrong synchronization call! "
                      "No opening LOCK_ALL or LOCK access epoch in %s\n", __FUNCTION__);
        mpi_errno = MPI_ERR_RMA_SYNC;
        goto fn_fail;
    }
#endif

#ifdef CSP_ENABLE_SYNC_ALL_OPT
    /* Get global window or a target window for no-lock mode or
     * lock-exist mode respectively. */
    CSPU_TARGET_GET_EPOCH_WIN(target, ug_win, win_ptr);
    CSP_ASSERT(win_ptr != NULL);

    CSP_DBG_PRINT(" flush_all(%s 0x%x), instead of target rank %d\n",
                  CSPU_GET_WIN_TYPE(*win_ptr, ug_win), *win_ptr, target_rank);
    CSP_CALLMPI(JUMP, PMPI_Win_flush_all(*win_ptr));
#else
    mpi_errno = CSPU_win_target_flush(target_rank, ug_win);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);
#endif

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    /* Lock of main ghost is granted, we can start load balancing from the next flush/unlock.
     * Note that only target which was issued operations to is guaranteed to be granted. */
    if (target->main_lock_stat == CSPU_MAIN_LOCK_OP_ISSUED) {
        target->main_lock_stat = CSPU_MAIN_LOCK_GRANTED;
        CSP_DBG_PRINT(" main lock (rank %d) granted\n", target_rank);
    }

    CSPU_reset_target_opload(target_rank, ug_win);
#endif

  fn_exit:
    CSPU_THREAD_EXIT_OBJ_CS(ug_win);
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before return */
    return mpi_errno;

  fn_fail:
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before error handling */
    CSPU_WIN_ERRHANLDING(win, &mpi_errno);
    goto fn_exit;
}

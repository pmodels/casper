/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"
#include "cspu_rma_sync.h"

static int win_global_flush_local_all(CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int i CSP_ATTRIBUTE((unused));

#ifdef CSP_ENABLE_SYNC_ALL_OPT
    CSP_DBG_PRINT(" flush_local_all(global_win 0x%x)\n", ug_win->global_win);
    CSP_CALLMPI(JUMP, PMPI_Win_flush_local_all(ug_win->global_win));
#else
    /* Flush every ghost once in the single window.
     * TODO: track op issuing, only flush the ghosts which receive ops. */
    for (i = 0; i < ug_win->num_g_ranks_in_ug; i++) {
        CSP_DBG_PRINT(" flush_local(ghost %d, global_win 0x%x)\n", ug_win->g_ranks_in_ug[i],
                      ug_win->global_win);
        CSP_CALLMPI(JUMP, PMPI_Win_flush_local(ug_win->g_ranks_in_ug[i], ug_win->global_win));
    }

    if (ug_win->is_self_locked) {
        mpi_errno = CSPU_win_flush_local_self(ug_win);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }
#endif

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_flush_local_all(MPI_Win win)
{
    CSPU_win_t *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int user_nprocs;
    int i CSP_ATTRIBUTE((unused));

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();
    CSPU_ERRHAN_EXTOBJ_LOCAL_DCL();
    CSPU_WIN_ERRHAN_SET_EXTOBJ();

    CSPU_fetch_ug_win_from_cache(win, &ug_win);
    if (ug_win == NULL) {
        /* normal window */
        CSPU_ERRHAN_RESET_EXTOBJ();     /* reset before calling original MPI */
        return PMPI_Win_flush_local_all(win);
    }

    CSPU_THREAD_ENTER_OBJ_CS(ug_win);

    CSP_ASSERT((ug_win->info_args.epochs_used & CSP_EPOCH_LOCK) ||
               (ug_win->info_args.epochs_used & CSP_EPOCH_LOCK_ALL));

#ifdef CSP_ENABLE_RMA_ERR_CHECK
    /* Check access epoch status.
     * The current epoch must be lock_all.*/
    if (ug_win->epoch_stat != CSPU_WIN_EPOCH_LOCK_ALL) {
        CSP_msg_print(CSP_MSG_ERROR, "Wrong synchronization call! "
                      "No opening LOCK_ALL access epoch in %s\n", __FUNCTION__);
        mpi_errno = MPI_ERR_RMA_SYNC;
        goto fn_fail;
    }
#endif

    CSP_ASSERT(ug_win->start_counter == 0 && ug_win->lock_counter == 0);
    CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_win->user_comm, &user_nprocs));

    if (!(ug_win->info_args.epochs_used & CSP_EPOCH_LOCK)) {
        /* In no-lock epoch, single window is shared by multiple targets. */
        mpi_errno = win_global_flush_local_all(ug_win);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }
    else {
        /* In lock-exist epoch, separate windows are bound with targets. */
#ifdef CSP_ENABLE_SYNC_ALL_OPT
        for (i = 0; i < ug_win->num_ug_wins; i++) {
            CSP_DBG_PRINT(" flush_local_all(ug_win 0x%x)\n", ug_win->ug_wins[i]);
            CSP_CALLMPI(JUMP, PMPI_Win_flush_local_all(ug_win->ug_wins[i]));
        }
#else
        for (i = 0; i < user_nprocs; i++) {
            mpi_errno = CSPU_win_target_flush_local(i, ug_win);
            CSP_CHKMPIFAIL_JUMP(mpi_errno);
        }
#endif /*end of CSP_ENABLE_SYNC_ALL_OPT */
    }

  fn_exit:
    CSPU_THREAD_EXIT_OBJ_CS(ug_win);
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before return */
    return mpi_errno;

  fn_fail:
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before error handling */
    CSPU_WIN_ERRHANLDING(win, &mpi_errno);
    goto fn_exit;
}

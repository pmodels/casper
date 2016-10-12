/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"
#include "cspu_rma_sync.h"

static int fence_flush_all(CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs;
    int i CSP_ATTRIBUTE((unused));

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->user_comm, &user_rank));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_win->user_comm, &user_nprocs));

    /* Flush all ghosts to finish the sequence of locally issued RMA operations */
    mpi_errno = CSPU_win_global_flush_all(ug_win);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    for (i = 0; i < user_nprocs; i++) {
        /* Runtime load balancing is allowed in fence epoch because
         * 1. fence is a global collective call, all targets already "exposed" their epoch.
         * 2. no conflicting lock/lockall on fence window. */
        ug_win->targets[i].main_lock_stat = CSPU_MAIN_LOCK_GRANTED;
        CSPU_reset_target_opload(i, ug_win);
    }
#endif

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_fence(int assert, MPI_Win win)
{
    CSPU_win_t *ug_win;
    int mpi_errno = MPI_SUCCESS;

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();
    CSPU_ERRHAN_EXTOBJ_LOCAL_DCL();
    CSPU_WIN_ERRHAN_SET_EXTOBJ();

    CSPU_fetch_ug_win_from_cache(win, &ug_win);

    if (ug_win == NULL) {
        /* normal window */
        CSPU_ERRHAN_RESET_EXTOBJ();     /* reset before calling original MPI */
        return PMPI_Win_fence(assert, win);
    }

    CSPU_THREAD_ENTER_OBJ_CS(ug_win);

    CSP_ASSERT((ug_win->info_args.epochs_used & CSP_EPOCH_FENCE));

    if (ug_win->epoch_stat == CSPU_WIN_EPOCH_FENCE)
        ug_win->is_self_locked = 0;     /* because we cannot reset it in previous FENCE. */

#ifdef CSP_ENABLE_RMA_ERR_CHECK
    /* Check access epoch status.
     * We do not require closed FENCE epoch, because we don't know whether
     * the previous FENCE is closed or not.*/
    if (ug_win->epoch_stat != CSPU_WIN_NO_EPOCH && ug_win->epoch_stat != CSPU_WIN_EPOCH_FENCE) {
        CSP_msg_print(CSP_MSG_ERROR, "Wrong synchronization call! "
                      "Previous %s access epoch is still open in %s\n",
                      CSPU_WIN_GET_EPOCH_STAT_NAME(ug_win), __FUNCTION__);
        mpi_errno = MPI_ERR_RMA_SYNC;
        goto fn_fail;
    }

    /* Check exposure epoch status.
     * The current epoch can be none or FENCE.*/
    if (ug_win->exp_epoch_stat == CSPU_WIN_EXP_EPOCH_PSCW) {
        CSP_msg_print(CSP_MSG_ERROR, "Wrong synchronization call! "
                      "Previous PSCW exposure epoch is still open in %s\n", __FUNCTION__);
        mpi_errno = MPI_ERR_RMA_SYNC;
        goto fn_fail;
    }
#endif

    CSP_ASSERT(ug_win->is_self_locked == 0);
    CSP_ASSERT(ug_win->start_counter == 0 && ug_win->lock_counter == 0);

    /* Eliminate flush_all if user explicitly specifies no preceding RMA calls. */
    if ((assert & MPI_MODE_NOPRECEDE) == 0) {
        mpi_errno = fence_flush_all(ug_win);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

    /* Always need sync to avoid instruction reordering of preceding load even if
     * user says no preceding store.*/
    CSP_CALLMPI(JUMP, PMPI_Win_sync(ug_win->global_win));

    /* Eliminate barrier when user specifies noprecede + nostore + noput.
     * In all other cases, barrier is still required. In no_precede fence, it
     * is required to synchronize between local store and remote RMA; in no_succeed
     * fence, it is also required to wait for remote RMA completion.
     * Only when user specifies noprecede + nostore + noput, which means everyone
     * is only doing load/get, it is safe to drop barrier. */
    if ((assert & MPI_MODE_NOPRECEDE & MPI_MODE_NOSTORE & MPI_MODE_NOPUT) == 0) {
        CSP_CALLMPI(JUMP, PMPI_Barrier(ug_win->user_comm));
    }

#ifdef CSP_ENABLE_LOCAL_RMA_OP_OPT
    /* During fence epoch, it is allowed to access local target directly */
    ug_win->is_self_locked = 1;
#endif

    /* Indicate epoch status.
     * Later operations will be redirected to global_win */
    ug_win->epoch_stat = CSPU_WIN_EPOCH_FENCE;

    /* Indicate exposure epoch status. */
    ug_win->exp_epoch_stat = CSPU_WIN_EXP_EPOCH_FENCE;

  fn_exit:
    CSPU_THREAD_EXIT_OBJ_CS(ug_win);
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before return */
    return mpi_errno;

  fn_fail:
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before error handling */
    CSPU_WIN_ERRHANLDING(win, &mpi_errno);
    goto fn_exit;
}

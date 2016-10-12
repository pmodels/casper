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
    CSPU_win_t *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank = 0, user_nprocs = 0;
    int i;

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();
    CSPU_ERRHAN_EXTOBJ_LOCAL_DCL();
    CSPU_WIN_ERRHAN_SET_EXTOBJ();

    CSPU_fetch_ug_win_from_cache(win, &ug_win);

    if (ug_win == NULL) {
        /* normal window */
        CSPU_ERRHAN_RESET_EXTOBJ();     /* reset before calling original MPI */
        return PMPI_Win_sync(win);
    }

    CSPU_THREAD_ENTER_OBJ_CS(ug_win);

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->user_comm, &user_rank));

    /* For no-lock window, just sync on single window. */
    if (!(ug_win->info_args.epochs_used & CSP_EPOCH_LOCK)) {
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

        CSP_CALLMPI(JUMP, PMPI_Win_sync(ug_win->global_win));

        CSP_DBG_PRINT(" win sync on %s single win 0x%x\n",
                      CSPU_WIN_GET_EPOCH_STAT_NAME(ug_win), ug_win->global_win);
    }

    /* For window that may contain locks, we should sync on all per-target windows
     * that are involved in opened lock epoch.*/
    else if (ug_win->epoch_stat == CSPU_WIN_EPOCH_PER_TARGET) {
        CSPU_win_target_t *target = NULL;
        int synced CSP_ATTRIBUTE((unused)) = 0;
        CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_win->user_comm, &user_nprocs));

        for (i = 0; i < user_nprocs; i++) {
            target = &ug_win->targets[i];
            if (target->epoch_stat == CSPU_TARGET_EPOCH_LOCK) {
                CSP_CALLMPI(JUMP, PMPI_Win_sync(target->ug_win));

                CSP_DBG_PRINT(" win sync on %s target %d, win 0x%x\n",
                              CSPU_target_epoch_stat_name[target->epoch_stat], i, target->ug_win);
                synced = 1;
            }
        }

#ifdef CSP_ENABLE_RMA_ERR_CHECK
        /* Check access epoch status.
         * At least one target must be locked.*/
        if (synced == 0) {
            CSP_msg_print(CSP_MSG_ERROR, "Wrong synchronization call! "
                          "No opening LOCK access epoch in %s\n", __FUNCTION__);
            mpi_errno = MPI_ERR_RMA_SYNC;
            goto fn_fail;
        }
#endif
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

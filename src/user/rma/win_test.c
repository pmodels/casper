/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"
#include "cspu_rma_sync.h"

int MPI_Win_test(MPI_Win win, int *flag)
{
    CSPU_win_t *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int post_grp_size = 0;

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();
    CSPU_ERRHAN_EXTOBJ_LOCAL_DCL();
    CSPU_WIN_ERRHAN_SET_EXTOBJ();

    CSPU_fetch_ug_win_from_cache(win, &ug_win);

    if (ug_win == NULL) {
        /* normal window */
        CSPU_ERRHAN_RESET_EXTOBJ();     /* reset before calling original MPI */
        return PMPI_Win_test(win, flag);
    }

    CSPU_THREAD_ENTER_OBJ_CS(ug_win);

    CSP_ASSERT((ug_win->info_args.epochs_used & CSP_EPOCH_PSCW));

    /* Check exposure epoch status.
     * Note that this is not only a user-friendly check, but also
     * used to avoid extra sync messages in wait/test.*/
    if (ug_win->exp_epoch_stat != CSPU_WIN_EXP_EPOCH_PSCW) {
        CSP_msg_print(CSP_MSG_ERROR, "Wrong synchronization call! "
                      "No opening PSCW exposure epoch in %s\n", __FUNCTION__);
        mpi_errno = MPI_ERR_RMA_SYNC;
        goto fn_fail;
    }

    if (ug_win->post_group == MPI_GROUP_NULL) {
        /* standard says do nothing for empty group */
        CSP_DBG_PRINT("Test empty group\n");
        return mpi_errno;
    }

    CSP_CALLMPI(JUMP, PMPI_Group_size(ug_win->post_group, &post_grp_size));
    CSP_ASSERT(post_grp_size > 0);

    CSP_DBG_PRINT("Test group 0x%x, size %d\n", ug_win->post_group, post_grp_size);

    /* Test the completion on all origin processes */
    mpi_errno = CSPU_recv_pscw_complete_msg(post_grp_size, ug_win, 0 /*non-blocking */ , flag);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* NOTE: MPI implementation should do memory barrier in flush handler. */
    CSP_CALLMPI(JUMP, PMPI_Win_sync(ug_win->global_win));

    if ((*flag)) {
        /* Reset exposure status if all sync messages are received.
         * All later wait/test will return immediately. */
        ug_win->exp_epoch_stat = CSPU_WIN_NO_EXP_EPOCH;
    }

    CSP_DBG_PRINT("Test done\n");

  fn_exit:
    /* free pscw arrays only when all requests are complete. */
    if ((*flag)) {
        if (ug_win->post_ranks_in_win_group)
            free(ug_win->post_ranks_in_win_group);
        ug_win->post_group = MPI_GROUP_NULL;
        ug_win->post_ranks_in_win_group = NULL;
    }

    CSPU_THREAD_EXIT_OBJ_CS(ug_win);
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before return */
    return mpi_errno;

  fn_fail:
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before error handling */
    CSPU_WIN_ERRHANLDING(win, &mpi_errno);
    goto fn_exit;
}

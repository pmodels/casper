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
    CSP_win *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int post_grp_size = 0;

    CSP_DBG_PRINT_FCNAME();

    CSP_fetch_ug_win_from_cache(win, ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_test(win, flag);
    }

    CSP_assert((ug_win->info_args.epoch_type & CSP_EPOCH_PSCW));

    /* Check exposure epoch status.
     * Note that this is not only a user-friendly check, but also
     * used to avoid extra sync messages in wait/test.*/
    if (ug_win->exp_epoch_stat != CSP_WIN_EXP_EPOCH_PSCW) {
        CSP_DBG_PRINT("No pscw exposure epoch\n");
        return mpi_errno;
    }

    if (ug_win->post_group == MPI_GROUP_NULL) {
        /* standard says do nothing for empty group */
        CSP_DBG_PRINT("Test empty group\n");
        return mpi_errno;
    }

    mpi_errno = PMPI_Group_size(ug_win->post_group, &post_grp_size);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    CSP_assert(post_grp_size > 0);

    CSP_DBG_PRINT("Test group 0x%x, size %d\n", ug_win->post_group, post_grp_size);

    /* Test the completion on all origin processes */
    mpi_errno = CSP_recv_pscw_complete_msg(post_grp_size, ug_win, 0 /*non-blocking */ , flag);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* NOTE: MPI implementation should do memory barrier in flush handler. */
    mpi_errno = PMPI_Win_sync(ug_win->global_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if ((*flag)) {
        /* Reset exposure status if all sync messages are received.
         * All later wait/test will return immediately. */
        ug_win->exp_epoch_stat = CSP_WIN_NO_EXP_EPOCH;
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

    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

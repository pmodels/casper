/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Win_set_errhandler(MPI_Win win, MPI_Errhandler errhandler)
{
    CSP_win_t *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int i;

    /* Always set error handler to user window . */
    PMPI_Win_set_errhandler(win, errhandler);

    CSP_fetch_ug_win_from_cache(win, &ug_win);
    if (ug_win == NULL) {
        /* normal window */
        return mpi_errno;
    }

    /* casper window starts */

    /* Set error handler on global window for fence|pscw|lockall-only window. */
    if ((ug_win->info_args.epochs_used & CSP_EPOCH_FENCE) ||
        (ug_win->info_args.epochs_used & CSP_EPOCH_PSCW) ||
        (ug_win->info_args.epochs_used == CSP_EPOCH_LOCK_ALL)) {
        mpi_errno = PMPI_Win_set_errhandler(ug_win->global_win, errhandler);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* Set handler also on all per-target windows if lock epoch exists. */
    if (ug_win->info_args.epochs_used & CSP_EPOCH_LOCK) {
        for (i = 0; i < ug_win->num_ug_wins; i++) {
            mpi_errno = PMPI_Win_set_errhandler(ug_win->ug_wins[i], errhandler);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
    }

    /* Though no MPI calls on the local shared window except Win_free, we
     * still set handler for it. Because standard does not clearly mention
     * whether an error in free call can be caught. */
    mpi_errno = PMPI_Win_set_errhandler(ug_win->local_ug_win, errhandler);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

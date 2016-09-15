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
    CSPU_win_t *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int i;

    /* Always set error handler to user window . */
    PMPI_Win_set_errhandler(win, errhandler);

    CSPU_fetch_ug_win_from_cache(win, &ug_win);
    if (ug_win == NULL) {
        /* normal window */
        return mpi_errno;
    }

    /* casper window starts */

    /* Set user defined error handler to all internal windows.
     *
     * FIXME: If an RMA error happens on the internal window, the internal window
     * will be exposed to user error handler function. Possible ways to workaround
     * it could be:
     * (1) Wrap up the user error handler to replace the internal error window.
     *   Drawback: need portably handle CXX/FORTRAN calls (e.g., how to detect caller language ?).
     * (2) Set MPI_ERRORS_RETURN for all internal windows, pass the returned error
     *   to user window at fn_fail.
     *   Drawback: would lose user-modified error code.
     * Now we implement in the second way for simplicity. */

    /* Set error handler on global window for fence|pscw|lockall-only window. */
    if ((ug_win->info_args.epochs_used & CSP_EPOCH_FENCE) ||
        (ug_win->info_args.epochs_used & CSP_EPOCH_PSCW) ||
        (ug_win->info_args.epochs_used == CSP_EPOCH_LOCK_ALL)) {
        mpi_errno = PMPI_Win_set_errhandler(ug_win->global_win, MPI_ERRORS_RETURN);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* Set handler also on all per-target windows if lock epoch exists. */
    if (ug_win->info_args.epochs_used & CSP_EPOCH_LOCK) {
        for (i = 0; i < ug_win->num_ug_wins; i++) {
            mpi_errno = PMPI_Win_set_errhandler(ug_win->ug_wins[i], MPI_ERRORS_RETURN);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
    }

    /* Though no MPI calls on the local shared window except Win_free, we
     * still set handler for it. Because standard does not clearly mention
     * whether an error in free call can be caught. */
    mpi_errno = PMPI_Win_set_errhandler(ug_win->local_ug_win, MPI_ERRORS_RETURN);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

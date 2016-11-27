/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Win_get_errhandler(MPI_Win win, MPI_Errhandler * errhandler)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Errhandler cached_errhandler = MPI_ERRHANDLER_NULL;
    MPI_Win_errhandler_function *cached_fnc = NULL;

    /* Only user-specified window error handler is cached, because
     * standard does not say whether a window has default error handler. */
    CSPU_win_errhan_get(win, &cached_errhandler, &cached_fnc);
    if (cached_errhandler != MPI_ERRHANDLER_NULL) {
        (*errhandler) = cached_errhandler;

    }
    else {
        /* Return the default error handler from MPI. */
        CSP_CALLMPI(JUMP, PMPI_Win_get_errhandler(win, errhandler));
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

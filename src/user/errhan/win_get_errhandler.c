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

    /* The error handler in any communicator exposed to user should be wrapped,
     * and the original user error handler is in cached. */
    CSPU_win_errhan_get(win, &cached_errhandler, &cached_fnc);
    CSP_ASSERT(cached_errhandler != MPI_ERRHANDLER_NULL);

    /* Return the original error handler. */
    (*errhandler) = cached_errhandler;

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

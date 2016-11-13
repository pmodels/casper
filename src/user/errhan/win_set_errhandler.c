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
    int mpi_errno = MPI_SUCCESS;
    MPI_Win_errhandler_function *errhandler_fnc = NULL;

    if (errhandler != MPI_ERRORS_ARE_FATAL && errhandler != MPI_ERRORS_RETURN) {
        /* Get cached user function on this handler */
        CSPU_errhan_get_fnc(errhandler, (void **) (&errhandler_fnc));
        CSP_ASSERT(errhandler_fnc != NULL);
    }

    /* Cache [window -> error handler & callback function].
     * Note that we use manual hash instead of win_get_attr, to avoid additional
     * MPI calls in error handling that might result in infinite recursion.*/
    CSPU_win_errhan_cache(win, errhandler, errhandler_fnc);

    return mpi_errno;
}

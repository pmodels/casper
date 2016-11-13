/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Comm_get_errhandler(MPI_Comm comm, MPI_Errhandler * errhandler)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Errhandler cached_errhandler = MPI_ERRHANDLER_NULL;
    MPI_Comm_errhandler_function *cached_fnc = NULL;

    /* The error handler in any communicator exposed to user should be wrapped,
     * and the original user error handler is in cached. */
    CSPU_comm_errhan_get(comm, &cached_errhandler, &cached_fnc);
    CSP_ASSERT(cached_errhandler != MPI_ERRHANDLER_NULL);

    /* Return the original error handler. */
    (*errhandler) = cached_errhandler;

    return mpi_errno;
}

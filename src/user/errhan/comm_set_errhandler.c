/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Comm_set_errhandler(MPI_Comm comm, MPI_Errhandler errhandler)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Comm_errhandler_function *errhandler_fnc = NULL;

    if (errhandler != MPI_ERRORS_ARE_FATAL && errhandler != MPI_ERRORS_RETURN) {
        /* Get cached user function on this handler */
        CSPU_errhan_get_fnc(errhandler, (void **) (&errhandler_fnc));
        CSP_ASSERT(errhandler_fnc != NULL);
    }

    /* Wrap up user error handler and cache [comm -> error handler & callback].
     * Note that we use manual hash instead of comm_get_attr, to avoid additional
     * MPI calls in error handling that might result in infinite recursion.*/
    mpi_errno = CSPU_comm_errhan_wrap(comm, errhandler, errhandler_fnc);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* Also wrap and cache for COMM_USER_WORLD if it is COMM_WORLD. */
    if (comm == MPI_COMM_WORLD) {
        mpi_errno = CSPU_comm_errhan_wrap(CSP_COMM_USER_WORLD, errhandler, errhandler_fnc);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    CSPU_comm_call_errhandler(comm, &mpi_errno);
    goto fn_exit;
}

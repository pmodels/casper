/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

int MPI_Errhandler_set(MPI_Comm comm, MPI_Errhandler errhandler)
{
    /* Deprecated function, replaced by MPI_Comm_set_errhandler.
     * See overwritten content in MPI_Comm_set_errhandler. */
    return MPI_Comm_set_errhandler(comm, errhandler);
}

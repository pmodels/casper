/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *
 *  (C) 2016 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#ifndef OMPI_OMIT_MPI1_COMPAT_DECLS

int MPI_Errhandler_create(MPI_Handler_function * errhandler_fn, MPI_Errhandler * errhandler)
{
    /* Deprecated function, replaced by MPI_Comm_create_errhandler.
     * See overwritten content in MPI_Comm_create_errhandler. */
    return MPI_Comm_create_errhandler((MPI_Comm_errhandler_function *) errhandler_fn, errhandler);
}

#endif /* OMPI_OMIT_MPI1_COMPAT_DECLS */

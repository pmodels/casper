/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>
#include "ctest.h"

/*
 *  This test checks finalize.
 *  The first process calls finalize, then other processes call win_allocate.
 *  The other processes may hang if the ghost process does not wait till all
 *  local processes arrive finalize. */

#define WAIT_TIME 100   /* 100us */
#define WIN_SIZE 16     /* count of double */

int rank, nprocs;
double *winbuf = NULL;
MPI_Win win = MPI_WIN_NULL;
MPI_Comm win_comm = MPI_COMM_NULL;

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (nprocs < 3) {
        fprintf(stderr, "Please run using at least 3 processes\n");
        goto exit;
    }

    MPI_Comm_split(MPI_COMM_WORLD, rank > 0, 0, &win_comm);
    MPI_Barrier(MPI_COMM_WORLD);

    if (rank > 0) {
        /* make sure P0 has called finalize */
        double t0 = MPI_Wtime();
        while ((MPI_Wtime() - t0) * 1000 * 1000 < WAIT_TIME);

        MPI_Win_allocate(sizeof(double) * WIN_SIZE, sizeof(double), MPI_INFO_NULL,
                         win_comm, &winbuf, &win);
        MPI_Win_free(&win);
    }

  exit:
    if (win_comm != MPI_COMM_NULL)
        MPI_Comm_free(&win_comm);
    MPI_Finalize();

    if (rank == 0)
        CTEST_report_result(0);

    return 0;
}

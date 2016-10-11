/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>
#include "ctest.h"

/*
 *  This test checks multiple win_allocate and win_free.
 */

#define ITER 50
int rank, nprocs;
double *winbuf[ITER];
MPI_Win win[ITER];
int size = 16;

/* check N * win_allocate + N * win_free.*/
static void run_test1()
{
    int x;

    for (x = 0; x < ITER; x++) {
        /* size in byte */
        MPI_Win_allocate(sizeof(double) * size, sizeof(double), MPI_INFO_NULL,
                         MPI_COMM_WORLD, &winbuf[x], &win[x]);
    }

    for (x = 0; x < ITER; x++) {
        MPI_Win_free(&win[x]);
    }
}

/* check N * [win_allocate + win_free].*/
static void run_test2()
{
    int x;

    for (x = 0; x < ITER; x++) {
        /* size in byte */
        MPI_Win_allocate(sizeof(double) * size, sizeof(double), MPI_INFO_NULL,
                         MPI_COMM_WORLD, &winbuf[x], &win[x]);

        MPI_Win_free(&win[x]);
    }
}

/* check N * [win_allocate + win_free] on windows sharing the same ghosts.
 * This sub test checks windows with disjoint user subsets but sharing the same ghosts.
 * i.e., [P0, P1] are bound to G0, [P2, P3] are bound to G1, now two communicators
 * [P0, P2] and [P1, P3] are creating window concurrently.*/
static void run_test3()
{
    int x;
    MPI_Comm oddeven_comm = MPI_COMM_NULL;
    MPI_Comm_split(MPI_COMM_WORLD, rank % 2, rank, &oddeven_comm);

    for (x = 0; x < ITER; x++) {
        /* size in byte */
        MPI_Win_allocate(sizeof(double) * size, sizeof(double), MPI_INFO_NULL,
                         oddeven_comm, &winbuf[x], &win[x]);
        MPI_Win_free(&win[x]);
    }

    MPI_Comm_free(&oddeven_comm);
}

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (nprocs < 2) {
        fprintf(stderr, "Please run using at least 2 processes\n");
        goto exit;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    run_test1();

    MPI_Barrier(MPI_COMM_WORLD);
    run_test2();

    MPI_Barrier(MPI_COMM_WORLD);
    run_test3();

  exit:
    if (rank == 0)
        CTEST_report_result(0);

    MPI_Finalize();

    return 0;
}

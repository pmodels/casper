/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>

/* This benchmark evaluates the overhead of CASPER wrapped MPI_Win_lock
 * when lock a self target using 2 processes with mode nocheck.
 * (only rank 0 is used, but window requires at least 2 processes)
 *
 * Rank 0 locks itself and unlock. It does not need RMA operations because
 * local lock will be granted immediately. However, we still issue a accumulate
 * in order to comparing with lock_self_overhead_no_loadstore. */

/* #define DEBUG */
#define CHECK
#define ITER 1000000
#define SKIP 100

double *winbuf = NULL;
double locbuf[1];
int rank, nprocs;
MPI_Win win = MPI_WIN_NULL;
int NOP = 1;

#include <casper.h>
int CSP_NUM_G = 1;

static int run_test()
{
    int i, x, errs_total = 0;
    int dst;
    double t0, t_total = 0.0;

    dst = 0;
    for (x = 0; x < SKIP; x++) {
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, dst, MPI_MODE_NOCHECK, win);
        for (i = 0; i < NOP; i++)
            MPI_Put(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, win);
        MPI_Win_unlock(dst, win);
    }

    t0 = MPI_Wtime();

    for (x = 0; x < ITER; x++) {
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, dst, MPI_MODE_NOCHECK, win);
        for (i = 0; i < NOP; i++)
            MPI_Put(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, win);
        MPI_Win_unlock(dst, win);
    }

    t_total = (MPI_Wtime() - t0) * 1000 * 1000; /*us */
    t_total /= ITER;

    fprintf(stdout, "casper-nocheck: iter %d num_op %d nprocs %d nh %d total_time %.2lf\n",
            ITER, NOP, nprocs, CSP_NUM_G, t_total);

    return errs_total;
}

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    CSP_ghost_size(&CSP_NUM_G);

    if (argc >= 2) {
        NOP = atoi(argv[1]);
    }

    locbuf[0] = 1.0;
    MPI_Win_allocate(sizeof(double), sizeof(double), MPI_INFO_NULL, MPI_COMM_WORLD, &winbuf, &win);

    if (rank == 0) {
        run_test();
    }

    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    MPI_Finalize();

    return 0;
}

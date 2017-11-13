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

/* This benchmark measures the overhead of Win_lock_all with
 * user-specified number of processes (>= 2). Rank 0 locks
 * all the processes and issues accumulate operations to all
 * of them; the other processes wait at barrier. */

/* #define DEBUG */
#define CHECK
#define ITER_S 100000
#define ITER_L 100000
#define SKIP 100
#define NPROCS_M 16

double *winbuf = NULL;
double locbuf[1];
int rank, nprocs;
MPI_Win win = MPI_WIN_NULL;
int ITER = ITER_S;
#ifdef ENABLE_CSP
#include <casper.h>
int CSP_NUM_G = 1;
#endif

static int run_test()
{
    int x, errs = 0;
    int dst;
    double t0, t_total = 0.0;
    double sum = 0.0;

    if (nprocs <= NPROCS_M) {
        ITER = ITER_S;
    }
    else {
        ITER = ITER_L;
    }

    if (rank == 0) {
        for (x = 0; x < SKIP; x++) {
            MPI_Win_lock_all(0, win);
            /* Send to all the other processes including itself in order to
             * make sure that all the targets are exactly locked. */
            for (dst = 0; dst < nprocs; dst++) {
                MPI_Accumulate(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, MPI_SUM, win);
            }
            MPI_Win_unlock_all(win);
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
        t0 = MPI_Wtime();

        for (x = 0; x < ITER; x++) {
            MPI_Win_lock_all(0, win);
            /* Send to all the other processes including itself in order to
             * make sure that all the targets are exactly locked. */
            for (dst = 0; dst < nprocs; dst++) {
                MPI_Accumulate(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, MPI_SUM, win);
            }
            MPI_Win_unlock_all(win);
        }

        t_total += (MPI_Wtime() - t0) * 1000 * 1000;    /*us */
        t_total /= ITER;
    }

    MPI_Barrier(MPI_COMM_WORLD);

#ifdef CHECK
    /* need lock on self rank for checking window buffer.
     * otherwise, the result may be incorrect because flush/unlock
     * doesn't wait for target completion in exclusive lock */
    MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win);
    sum = 1.0 * (ITER + SKIP);
    if (CTEST_double_diff(winbuf[0], sum)) {
        fprintf(stderr, "[%d]computation error : winbuf[%d] %.2lf != %.2lf\n",
                rank, 0, winbuf[0], sum);
        errs += 1;
    }
    MPI_Win_unlock(rank, win);
#endif

    if (rank == 0) {
#ifdef ENABLE_CSP
        fprintf(stdout, "casper: iter %d nprocs %d nh %d total_time %lf\n", ITER, nprocs,
                CSP_NUM_G, t_total);
#else
        fprintf(stdout, "orig: iter %d nprocs %d total_time %.2lf\n", ITER, nprocs, t_total);
#endif
    }

    return errs;
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
#ifdef ENABLE_CSP
    CSP_ghost_size(&CSP_NUM_G);
#endif

    locbuf[0] = (rank + 1) * 1.0;
    MPI_Win_allocate(sizeof(double), sizeof(double), MPI_INFO_NULL, MPI_COMM_WORLD, &winbuf, &win);

    run_test();

  exit:

    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    MPI_Finalize();

    return 0;
}

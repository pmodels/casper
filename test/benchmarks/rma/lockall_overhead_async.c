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

/* This benchmark evaluates the overhead of Win_lock_all with
 * user-specified number of processes (>= 2). Rank 0 locks
 * all the processes and issues accumulate operations to all
 * of them; the other processes keep computation (busy wait) until
 * receive completion flag from rank 0. */

/* #define DEBUG */
#define CHECK
#define ITER_LL 1000
#define ITER_S 100000
#define ITER_L 50000
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

unsigned long SLEEP_MAX = 100, SLEEP_MIN = 100, SLEEP_ITER = 2; /* us */
unsigned long SLEEP_TIME;
int NOP = 100;

static int target_computation()
{
    if (SLEEP_TIME == 0)
        return 0;

    double start = MPI_Wtime() * 1000 * 1000;
    while (MPI_Wtime() * 1000 * 1000 - start < SLEEP_TIME);
    return 0;
}

static int run_test()
{
    int i, x, errs = 0;
    int dst, flag = 0;
    double t0, t_total = 0.0;
    double sum = 0.0;
    MPI_Status stat;
    MPI_Request req;
    int buf[1] = { 1 };

    if (nprocs <= NPROCS_M) {
        ITER = ITER_S;
    }
    else if (nprocs > NPROCS_M && SLEEP_TIME < 50) {
        ITER = ITER_L;
    }
    else {
        ITER = ITER_L;
    }


    if (rank == 0) {
        for (x = 0; x < SKIP; x++) {
            MPI_Win_lock_all(0, win);
            for (dst = 0; dst < nprocs; dst++) {
                MPI_Accumulate(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, MPI_SUM, win);
            }
            MPI_Win_unlock_all(win);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    t0 = MPI_Wtime();

    if (rank == 0) {
        for (x = 0; x < ITER; x++) {
            MPI_Win_lock_all(0, win);
            for (dst = 0; dst < nprocs; dst++) {
                for (i = 0; i < NOP; i++) {
                    MPI_Accumulate(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, MPI_SUM, win);
                }
            }
            MPI_Win_unlock_all(win);
        }
    }
    else {
        /* target processes are computing until receives completion from origin. */
        MPI_Irecv(buf, 1, MPI_INT, 0, 899, MPI_COMM_WORLD, &req);
        while (!flag) {
            target_computation();
            MPI_Test(&req, &flag, &stat);
        }
    }

    t_total += (MPI_Wtime() - t0) * 1000 * 1000;        /*us */
    t_total /= ITER;

    if (rank == 0) {
        /* notify target rma is done. */
        for (dst = 1; dst < nprocs; dst++) {
            MPI_Send(buf, 1, MPI_INT, dst, 899, MPI_COMM_WORLD);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

#ifdef CHECK
    MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win);
    sum = 1.0 * (ITER * NOP + SKIP);
    if (CTEST_double_diff(winbuf[0], sum)) {
        fprintf(stderr, "[%d]computation error : winbuf[%d] %.2lf != %.2lf\n",
                rank, 0, winbuf[0], sum);
        errs += 1;
    }
    MPI_Win_unlock(rank, win);
#endif

    if (rank == 0) {
#ifdef ENABLE_CSP
        fprintf(stdout,
                "casper: iter %d comp_size %lu num_op %d nprocs %d nh %d total_time %.2lf\n", ITER,
                SLEEP_TIME, NOP, nprocs, CSP_NUM_G, t_total);
#else
        fprintf(stdout, "orig: iter %d comp_size %lu num_op %d nprocs %d total_time %.2lf\n",
                ITER, SLEEP_TIME, NOP, nprocs, t_total);
#endif
    }

    return errs;
}

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#ifdef ENABLE_CSP
    CSP_ghost_size(&CSP_NUM_G);
#endif

    if (nprocs < 2) {
        fprintf(stderr, "Please run using at least 2 processes\n");
        goto exit;
    }

    if (argc >= 4) {
        SLEEP_MIN = atoi(argv[1]);
        SLEEP_MAX = atoi(argv[2]);
        SLEEP_ITER = atoi(argv[3]);
    }
    if (argc >= 5) {
        NOP = atoi(argv[4]);
    }

    locbuf[0] = (rank + 1) * 1.0;
    MPI_Win_allocate(sizeof(double), sizeof(double), MPI_INFO_NULL, MPI_COMM_WORLD, &winbuf, &win);

    for (SLEEP_TIME = SLEEP_MIN; SLEEP_TIME <= SLEEP_MAX; SLEEP_TIME *= SLEEP_ITER) {
        /* reset */
        MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win);
        winbuf[0] = 0.0;
        MPI_Win_unlock(rank, win);
        MPI_Barrier(MPI_COMM_WORLD);

        run_test();

        /* only run once if user disabled async */
        if (SLEEP_TIME == 0)
            break;
    }

  exit:

    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    MPI_Finalize();

    return 0;
}

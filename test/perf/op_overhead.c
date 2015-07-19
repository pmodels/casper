/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>

/* This benchmark measures the overhead of RMA operations using 2 processes.
 * Rank 0 issues lock and specified operation to rank 1. */

/* #define DEBUG */
#define CHECK
#define ITER 10000
#define SKIP 100

double *winbuf = NULL;
double *locbuf = NULL;
int rank, nprocs;
MPI_Win win = MPI_WIN_NULL;
int NOP = 1;
int OP_SIZE = 1;
const char *OP_TYPE_NM[3] = { "ACC", "PUT", "GET" };

enum {
    OP_ACC,
    OP_PUT,
    OP_GET
};
int OP_TYPE = OP_ACC;

#ifdef ENABLE_CSP
#include <casper.h>
int CSP_NUM_G = 1;
#endif

static void DO_OP_LOOP(int dst, int iter)
{
    int i, x;

    switch (OP_TYPE) {
    case OP_ACC:
        for (x = 0; x < iter; x++) {
            for (i = 0; i < NOP; i++)
                MPI_Accumulate(&locbuf[0], OP_SIZE, MPI_DOUBLE, dst, 0, OP_SIZE, MPI_DOUBLE,
                               MPI_SUM, win);
            MPI_Win_flush(dst, win);
        }
        break;
    case OP_PUT:
        for (x = 0; x < iter; x++) {
            for (i = 0; i < NOP; i++)
                MPI_Put(&locbuf[0], OP_SIZE, MPI_DOUBLE, dst, 0, OP_SIZE, MPI_DOUBLE, win);
            MPI_Win_flush(dst, win);
        }
        break;
    case OP_GET:
        for (x = 0; x < iter; x++) {
            for (i = 0; i < NOP; i++)
                MPI_Get(&locbuf[0], OP_SIZE, MPI_DOUBLE, dst, 0, OP_SIZE, MPI_DOUBLE, win);
            MPI_Win_flush(dst, win);
        }
        break;
    }
}

static int run_test()
{
    int errs_total = 0;
    int dst;
    double t0, t_total = 0.0;

    dst = 1;
    if (rank == 0) {

        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, dst, MPI_MODE_NOCHECK, win);
        DO_OP_LOOP(dst, SKIP);
        MPI_Win_unlock(dst, win);

        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, dst, MPI_MODE_NOCHECK, win);

        t0 = MPI_Wtime();
        DO_OP_LOOP(dst, ITER);
        t_total = (MPI_Wtime() - t0) * 1000 * 1000;     /*us */
        t_total /= ITER;

        MPI_Win_unlock(dst, win);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
#ifdef ENABLE_CSP
        fprintf(stdout, "casper: iter %d %s num_op %d opsize %d nprocs %d nh %d total_time %.2lf\n",
                ITER, OP_TYPE_NM[OP_TYPE], NOP, OP_SIZE, nprocs, CSP_NUM_G, t_total);
#else
        fprintf(stdout, "orig: iter %d %s num_op %d opsize %d nprocs %d total_time %.2lf\n",
                ITER, OP_TYPE_NM[OP_TYPE], NOP, OP_SIZE, nprocs, t_total);
#endif
    }

    return errs_total;
}

int main(int argc, char *argv[])
{
    int i, OP_SIZE_MIN = 1, OP_SIZE_MAX = 1, OP_SIZE_ITER = 2;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#ifdef ENABLE_CSP
    CSP_ghost_size(&CSP_NUM_G);
#endif

    if (argc >= 4) {
        OP_SIZE_MIN = atoi(argv[1]);
        OP_SIZE_MAX = atoi(argv[2]);
        OP_SIZE_ITER = atoi(argv[3]);
    }
    if (argc >= 5) {
        NOP = atoi(argv[4]);
    }
    if (argc >= 6) {
        OP_TYPE = atoi(argv[5]);
    }

    if ((OP_TYPE != OP_ACC) && (OP_TYPE != OP_PUT) && (OP_TYPE != OP_GET)) {
        if (rank == 0)
            fprintf(stderr, "Wrong op type %d\n", OP_TYPE);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    locbuf = calloc(OP_SIZE_MAX, sizeof(double));
    MPI_Win_allocate(sizeof(double) * OP_SIZE_MAX, sizeof(double), MPI_INFO_NULL, MPI_COMM_WORLD,
                     &winbuf, &win);

    for (i = 0; i < OP_SIZE_MAX; i++) {
        locbuf[i] = i * 1.0;
        winbuf[i] = 0;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    for (OP_SIZE = OP_SIZE_MIN; OP_SIZE <= OP_SIZE_MAX; OP_SIZE *= OP_SIZE_ITER) {
        run_test();
        MPI_Barrier(MPI_COMM_WORLD);

        if (OP_SIZE == OP_SIZE_MAX || OP_SIZE_ITER == 1)
            break;
    }

    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    if (locbuf)
        free(locbuf);
    MPI_Finalize();

    return 0;
}

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
 * This test checks acc with lock.
 */

#define NUM_OPS 2
#define TOTAL_NUM_OPS (NUM_OPS * 2)
#define CHECK
#define OUTPUT_FAIL_DETAIL

double *winbuf = NULL;
double *locbuf = NULL;
int rank, nprocs;
MPI_Win win = MPI_WIN_NULL;
int ITER = 2;
double max_result = 0.0;
double sum_result = 0.0;

static int run_test(int nop)
{
    int i, x, errs = 0, errs_total = 0;
    int dst;

    dst = (rank + 1) % nprocs;

    /* check lock(neighbor)/acc & flush+ NOP * acc/unlock. */
    for (x = 0; x < ITER; x++) {
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, dst, 0, win);

        MPI_Accumulate(&locbuf[dst * nop], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, MPI_SUM, win);
        MPI_Accumulate(&locbuf[dst * nop], 1, MPI_DOUBLE, dst, 1, 1, MPI_DOUBLE, MPI_MAX, win);
        MPI_Win_flush(dst, win);

        for (i = 1; i < nop; i++) {
            MPI_Accumulate(&locbuf[i + dst * nop], 1, MPI_DOUBLE, dst, 0, 1,
                           MPI_DOUBLE, MPI_SUM, win);
            MPI_Accumulate(&locbuf[i + dst * nop], 1, MPI_DOUBLE, dst, 1, 1,
                           MPI_DOUBLE, MPI_MAX, win);
#ifdef MVA
            MPI_Win_flush(dst, win);    /* use it to poke progress in order to finish local CQEs */
#endif
        }

        MPI_Win_unlock(dst, win);
    }

    /* need barrier before checking local window buffer */
    MPI_Barrier(MPI_COMM_WORLD);

    /* need lock on self rank for checking window buffer.
     * otherwise, the result may be incorrect because flush/unlock
     * doesn't wait for target completion in exclusive lock */
    MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win);

    if (CTEST_double_diff(winbuf[0], sum_result * ITER)) {
        fprintf(stderr, "[%d]winbuf[%d] %.1lf != %.1lf (%.1lf * %d)\n", rank, 0,
                winbuf[0], sum_result * ITER, sum_result, ITER);
        errs++;
    }
    if (CTEST_double_diff(winbuf[1], max_result)) {
        fprintf(stderr, "[%d]winbuf[%d] %.1lf != %.1lf\n", rank, 1, winbuf[1], max_result);
        errs++;
    }

    MPI_Win_unlock(rank, win);

    if (errs > 0) {
        fprintf(stderr, "[%d] checking failed\n", rank);
#ifdef OUTPUT_FAIL_DETAIL
        CTEST_print_double_array(locbuf, nop * nprocs, "locbuf");
        fprintf(stderr, "winbuf: %.1lf %.1lf\n", winbuf[0], winbuf[1]);
#endif
    }

    MPI_Allreduce(&errs, &errs_total, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    return errs_total;
}

int main(int argc, char *argv[])
{
    int size = NUM_OPS;
    int i, errs = 0;
    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (nprocs < 2) {
        fprintf(stderr, "Please run using at least 2 processes\n");
        goto exit;
    }

    locbuf = calloc(NUM_OPS * nprocs, sizeof(double));
    for (i = 0; i < NUM_OPS * nprocs; i++) {
        locbuf[i] = 1.0 * i;
    }

    /* size in byte */
    MPI_Win_allocate(sizeof(double) * 2, sizeof(double), MPI_INFO_NULL,
                     MPI_COMM_WORLD, &winbuf, &win);

    /*
     * P0: SUM{0:NOPS-1}, MAX{0:NOPS-1}
     * P1: SUM{NOPS:2NOPS-1}, MAX{NOPS:2NOPS-1}
     * ...
     */

    /* reset window */
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win);
    for (i = 0; i < 2; i++) {
        winbuf[i] = 0.0;
    }
    MPI_Win_unlock(rank, win);

    max_result = locbuf[NUM_OPS * rank + NUM_OPS - 1];
    for (i = 0; i < NUM_OPS; i++) {
        sum_result += locbuf[NUM_OPS * rank + i];
    }

    MPI_Barrier(MPI_COMM_WORLD);
    errs = run_test(size);

  exit:
    if (rank == 0)
        CTEST_report_result(errs);

    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    if (locbuf)
        free(locbuf);

    MPI_Finalize();

    return 0;
}

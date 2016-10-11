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
 * This test checks acc-get with fence.
 */

#define NUM_OPS 5
#define CHECK
#define OUTPUT_FAIL_DETAIL

double *winbuf = NULL;
double *locbuf = NULL, *checkbuf = NULL;
int rank, nprocs;
MPI_Win win = MPI_WIN_NULL;
int ITER = 2;

/* check fence/NOP * acc[all]/fence/get[all]/fence. */
static int run_test1(int nop)
{
    int i, x, errs = 0, errs_total = 0;
    int dst;

    for (x = 0; x < ITER; x++) {
        /* change data */
        for (i = 0; i < NUM_OPS * nprocs; i++) {
            locbuf[i] = (1.0 + x) * i;
        }

        MPI_Win_fence(0, win);

        for (dst = 0; dst < nprocs; dst++) {
            for (i = 0; i < nop; i++) {
                MPI_Accumulate(&locbuf[dst * nop + i], 1, MPI_DOUBLE, dst, rank, 1, MPI_DOUBLE,
                               MPI_MAX, win);
            }
        }
        MPI_Win_fence(0, win);

        for (dst = 0; dst < nprocs; dst++) {
            MPI_Get(&checkbuf[dst], 1, MPI_DOUBLE, dst, rank, 1, MPI_DOUBLE, win);
        }

        MPI_Win_fence(0, win);

        /* check in every iteration */
        for (dst = 0; dst < nprocs; dst++) {
            if (CTEST_precise_double_diff(checkbuf[dst], locbuf[dst * nop + nop - 1])) {
                fprintf(stderr, "[%d] iter %d checkbuf[%d] %.1lf != %.1lf\n", rank, x,
                        dst, checkbuf[dst], locbuf[dst * nop + nop - 1]);
                errs++;
            }
        }
    }

    if (errs > 0) {
        fprintf(stderr, "[%d] checking failed\n", rank);
#ifdef OUTPUT_FAIL_DETAIL
        CTEST_print_double_array(locbuf, nop * nprocs, "locbuf");
#endif
    }

    MPI_Allreduce(&errs, &errs_total, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    return errs_total;
}

/* check fence(no_precede)/NOP * acc[all]/fence/get[all]/fence(no_succeed). */
static int run_test2(int nop)
{
    int i, x, errs = 0, errs_total = 0;
    int dst;

    for (x = 0; x < ITER; x++) {
        /* change data */
        for (i = 0; i < NUM_OPS * nprocs; i++) {
            locbuf[i] = (1.0 + x) * i;
        }

        MPI_Win_fence(MPI_MODE_NOPRECEDE, win);

        for (dst = 0; dst < nprocs; dst++) {
            for (i = 0; i < nop; i++) {
                MPI_Accumulate(&locbuf[dst * nop + i], 1, MPI_DOUBLE, dst, rank, 1, MPI_DOUBLE,
                               MPI_MAX, win);
            }
        }
        MPI_Win_fence(0, win);

        for (dst = 0; dst < nprocs; dst++) {
            MPI_Get(&checkbuf[dst], 1, MPI_DOUBLE, dst, rank, 1, MPI_DOUBLE, win);
        }

        MPI_Win_fence(MPI_MODE_NOSUCCEED, win);

        /* check in every iteration */
        for (dst = 0; dst < nprocs; dst++) {
            if (CTEST_precise_double_diff(checkbuf[dst], locbuf[dst * nop + nop - 1])) {
                fprintf(stderr, "[%d] iter %d checkbuf[%d] %.1lf != %.1lf\n", rank, x,
                        dst, checkbuf[dst], locbuf[dst * nop + nop - 1]);
                errs++;
            }
        }
    }

    if (errs > 0) {
        fprintf(stderr, "[%d] checking failed\n", rank);
#ifdef OUTPUT_FAIL_DETAIL
        CTEST_print_double_array(locbuf, nop * nprocs, "locbuf");
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
    checkbuf = calloc(nprocs, sizeof(double));

    /* size in byte */
    MPI_Win_allocate(sizeof(double) * nprocs, sizeof(double), MPI_INFO_NULL,
                     MPI_COMM_WORLD, &winbuf, &win);

    /* reset */
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win);
    for (i = 0; i < nprocs; i++) {
        winbuf[i] = 0.0;
        checkbuf[i] = 0.0;
    }
    MPI_Win_unlock(rank, win);

    MPI_Barrier(MPI_COMM_WORLD);
    errs = run_test1(size);
    if (errs)
        goto exit;

    MPI_Barrier(MPI_COMM_WORLD);

    /* reset */
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win);
    for (i = 0; i < nprocs; i++) {
        winbuf[i] = 0.0;
        checkbuf[i] = 0.0;
    }
    MPI_Win_unlock(rank, win);

    MPI_Barrier(MPI_COMM_WORLD);
    errs = run_test2(size);
    if (errs)
        goto exit;

  exit:
    if (rank == 0)
        CTEST_report_result(errs);

    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    if (locbuf)
        free(locbuf);
    if (checkbuf)
        free(checkbuf);

    MPI_Finalize();

    return 0;
}

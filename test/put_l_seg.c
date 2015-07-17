/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mpi.h>
#include "ctest.h"

/*
 * This test checks lockall with put for segment-binding mode.
 */

#define NUM_OPS 3
#define OP_SIZE 32      /*count of double, window will be divided if odd np and even nh */
#define CHECK
#define OUTPUT_FAIL_DETAIL

double *winbuf = NULL;
double *locbuf = NULL;
int rank, nprocs;
MPI_Win win = MPI_WIN_NULL;
int ITER = 5;

static int run_test1(int nop)
{
    int i, j, x, errs = 0, errs_total = 0;
    int dst, dst_disp = 0, orig_disp = 0;

    for (x = 0; x < ITER; x++) {

        if (rank == 0) {
            MPI_Win_lock_all(0, win);
            for (dst = 0; dst < nprocs; dst++) {
                MPI_Put(&locbuf[dst * OP_SIZE * nop], OP_SIZE, MPI_DOUBLE, dst, 0, OP_SIZE,
                        MPI_DOUBLE, win);
            }
            MPI_Win_flush_all(win);

            for (dst = 0; dst < nprocs; dst++) {
                for (i = 1; i < nop; i++) {
                    dst_disp = OP_SIZE * i;
                    orig_disp = dst * OP_SIZE * nop + i * OP_SIZE;

                    MPI_Put(&locbuf[orig_disp], OP_SIZE, MPI_DOUBLE, dst, dst_disp, OP_SIZE,
                            MPI_DOUBLE, win);
                }
            }
            MPI_Win_unlock_all(win);
        }

        MPI_Barrier(MPI_COMM_WORLD);

        /* check in every iteration */
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win);
        for (i = 0; i < nop; i++) {
            for (j = 0; j < OP_SIZE; j++) {
                dst_disp = OP_SIZE * i + j;
                orig_disp = rank * OP_SIZE * nop + i * OP_SIZE + j;
                if (CTEST_double_diff(winbuf[dst_disp], locbuf[orig_disp])) {
                    fprintf(stderr, "[%d] winbuf[%d] %.1lf != locbuf[%d]%.1lf\n", rank, dst_disp,
                            winbuf[dst_disp], orig_disp, locbuf[orig_disp]);
                    errs++;
                }
            }
        }
        MPI_Win_unlock(rank, win);
        MPI_Barrier(MPI_COMM_WORLD);
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

static int run_test2(int nop)
{
    int i, j, x, errs = 0, errs_total = 0;
    int dst, dst_disp = 0, orig_disp = 0;

    for (x = 0; x < ITER; x++) {

        if (rank == 0) {
            MPI_Win_lock_all(0, win);
            for (dst = 0; dst < nprocs; dst++) {
                for (i = 0; i < nop; i++) {
                    dst_disp = OP_SIZE * i;
                    orig_disp = dst * OP_SIZE * nop + i * OP_SIZE;

                    MPI_Put(&locbuf[orig_disp], OP_SIZE, MPI_DOUBLE, dst, dst_disp, OP_SIZE,
                            MPI_DOUBLE, win);
                }
            }
            MPI_Win_unlock_all(win);
        }

        MPI_Barrier(MPI_COMM_WORLD);

        /* check in every iteration */
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win);
        for (i = 0; i < nop; i++) {
            for (j = 0; j < OP_SIZE; j++) {
                dst_disp = OP_SIZE * i + j;
                orig_disp = rank * OP_SIZE * nop + i * OP_SIZE + j;
                if (CTEST_double_diff(winbuf[dst_disp], locbuf[orig_disp])) {
                    fprintf(stderr, "[%d] winbuf[%d] %.1lf != locbuf[%d]%.1lf\n", rank, dst_disp,
                            winbuf[dst_disp], orig_disp, locbuf[orig_disp]);
                    errs++;
                }
            }
        }
        MPI_Win_unlock(rank, win);
        MPI_Barrier(MPI_COMM_WORLD);
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

    locbuf = calloc(NUM_OPS * OP_SIZE * nprocs, sizeof(double));
    for (i = 0; i < NUM_OPS * OP_SIZE * nprocs; i++) {
        locbuf[i] = 1.0 * i;
    }

    /* size in byte */
    MPI_Win_allocate(OP_SIZE * NUM_OPS * sizeof(double), sizeof(double), MPI_INFO_NULL,
                     MPI_COMM_WORLD, &winbuf, &win);

    memset(winbuf, 0, OP_SIZE * NUM_OPS * sizeof(double));

    MPI_Barrier(MPI_COMM_WORLD);
    errs = run_test1(size);
    if (errs)
        goto exit;

    MPI_Barrier(MPI_COMM_WORLD);
    errs = run_test2(size);
    if (errs)
        goto exit;

  exit:
    if (rank == 0) {
        fprintf(stdout, "%d errors\n", errs);
    }

    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    if (locbuf)
        free(locbuf);

    MPI_Finalize();

    return 0;
}

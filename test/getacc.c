/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mpi.h>
#include "ctest.h"

/*
 * This test expects to report following error if MPI_Get_accumulate has not
 * been implemented in CASPER, Otherwise no error.
 *      Fatal error in MPI_Get_accumulate: Wrong synchronization of RMA calls.
 */

#define NUM_OPS 32
#define TOTAL_NUM_OPS (NUM_OPS * 2)
#define CHECK
#define OUTPUT_FAIL_DETAIL

double *winbuf = NULL;
double *locbuf = NULL;
int rank, nprocs;
MPI_Win win = MPI_WIN_NULL;
int ITER = 10;
double max_result = 0.0;
double sum_result = 0.0;

#ifdef DEBUG
#define debug_printf(str,...) {fprintf(stdout, str, ## __VA_ARGS__);fflush(stdout);}
#else
#define debug_printf(str,...) {}
#endif

static int run_test(int nop)
{
    int i, x, errs = 0, errs_total = 0;
    int dst;
    double local_sum[NUM_OPS], local_max[NUM_OPS];

    memset(local_sum, 0, sizeof(local_sum));
    memset(local_max, 0, sizeof(local_max));

    MPI_Win_lock_all(0, win);

    /* It is shared lock, a target is only updated by one process */
    dst = (rank + 1) % nprocs;

    for (x = 0; x < ITER; x++) {
        MPI_Get_accumulate(&locbuf[dst * nop], 1, MPI_DOUBLE, &local_sum[0], 1, MPI_DOUBLE, dst, 0,
                           1, MPI_DOUBLE, MPI_SUM, win);
        MPI_Get_accumulate(&locbuf[dst * nop], 1, MPI_DOUBLE, &local_max[0], 1, MPI_DOUBLE, dst, 1,
                           1, MPI_DOUBLE, MPI_MAX, win);
        MPI_Win_flush_all(win);

        for (i = 1; i < nop; i++) {
            MPI_Get_accumulate(&locbuf[i + dst * nop], 1, MPI_DOUBLE, &local_sum[i], 1, MPI_DOUBLE,
                               dst, 0, 1, MPI_DOUBLE, MPI_SUM, win);
            MPI_Get_accumulate(&locbuf[i + dst * nop], 1, MPI_DOUBLE, &local_max[i], 1, MPI_DOUBLE,
                               dst, 1, 1, MPI_DOUBLE, MPI_MAX, win);
#ifdef MVA
            MPI_Win_flush(dst, win);    /* use it to poke progress in order to finish local CQEs */
#endif
        }
        MPI_Win_flush_all(win);
    }

    MPI_Win_unlock_all(win);

    /* need barrier before checking local window buffer */
    MPI_Barrier(MPI_COMM_WORLD);

    /* need lock on local rank for accessing local window buffer. */
    MPI_Win_lock_all(0, win);
    if (CTEST_double_diff(winbuf[0], sum_result * ITER)) {
        fprintf(stderr, "[%d]winbuf[%d] %.1lf != %.1lf (%.1lf * %d)\n", rank, 0,
                winbuf[0], sum_result * ITER, sum_result, ITER);
        errs++;
    }
    if (CTEST_double_diff(winbuf[1], max_result)) {
        fprintf(stderr, "[%d]winbuf[%d] %.1lf != %.1lf\n", rank, 1, winbuf[1], max_result);
        errs++;
    }
    MPI_Win_unlock_all(win);

    if (errs > 0) {
        fprintf(stderr, "[%d] checking failed\n", rank);
#ifdef OUTPUT_FAIL_DETAIL
        CTEST_print_double_array(locbuf, nop * nprocs, "locbuf");
        CTEST_print_double_array(winbuf, 2, "winbuf");
#endif
    }

    MPI_Allreduce(&errs, &errs_total, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    return errs_total;
}

int main(int argc, char *argv[])
{
    int size = NUM_OPS;
    int i, errs = 0;
    MPI_Info info = MPI_INFO_NULL;

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

#ifdef TEST_EPOCHS_USED_LOCKALL
    MPI_Info_create(&info);
    MPI_Info_set(info, (char *) "epochs_used", (char *) "lockall");
#endif

    /* size in byte */
    MPI_Win_allocate(sizeof(double) * 2, sizeof(double), info, MPI_COMM_WORLD, &winbuf, &win);

    /*
     * P0: SUM{0:NOPS-1}, MAX{0:NOPS-1}
     * P1: SUM{NOPS:2NOPS-1}, MAX{NOPS:2NOPS-1}
     * ...
     */

    /* reset window */
    MPI_Win_lock_all(0, win);
    for (i = 0; i < 2; i++) {
        winbuf[i] = 0.0;
    }
    MPI_Win_unlock_all(win);

    max_result = locbuf[NUM_OPS * rank + NUM_OPS - 1];
    for (i = 0; i < NUM_OPS; i++) {
        sum_result += locbuf[NUM_OPS * rank + i];
    }

    MPI_Barrier(MPI_COMM_WORLD);
    errs = run_test(size);

  exit:
    if (rank == 0)
        CTEST_report_result(errs);

    if (info != MPI_INFO_NULL)
        MPI_Info_free(&info);
    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    if (locbuf)
        free(locbuf);

    MPI_Finalize();

    return 0;
}

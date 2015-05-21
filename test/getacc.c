/*
 * getacc.c
 *
 * Expect reporting following error if MPI_Get_accumulate has not been
 * implemented in CASPER, Otherwise no error.
 *  Fatal error in MPI_Get_accumulate: Wrong synchronization of RMA calls
 *
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mpi.h>

#define SLEEP_TIME 100  /* 100us */
#define NUM_OPS 32
#define TOTAL_NUM_OPS (NUM_OPS * 2)
#define CHECK
#define OUTPUT_FAIL_DETAIL

double *winbuf = NULL;
double *locbuf = NULL;
int rank, nprocs;
int comp_size = 1;
MPI_Win win = MPI_WIN_NULL;
int ITER = 10;
double max_result = 0.0;
double sum_result = 0.0;

#undef DEBUG
#ifdef DEBUG
#define debug_printf(str...) {fprintf(stdout, str);fflush(stdout);}
#else
#define debug_printf(str...) {}
#endif

static int target_computation_init()
{
    return 0;
}

static int target_computation()
{
    double start = MPI_Wtime() * 1000 * 1000;
    while (MPI_Wtime() * 1000 * 1000 - start < SLEEP_TIME);
    return 0;
}

static int target_computation_exit()
{
    return 0;
}

static int run_test(int nop)
{
    int i, x, errs = 0, errs_total = 0;
    int dst;
    double local_sum[NUM_OPS], local_max[NUM_OPS];

    memset(local_sum, 0, sizeof(local_sum));
    memset(local_max, 0, sizeof(local_max));

    target_computation_init();
    MPI_Win_lock_all(0, win);

    /* It is shared lock, a target is only updated by one process */
    dst = (rank + 1) % nprocs;

    for (x = 0; x < ITER; x++) {
        MPI_Get_accumulate(&locbuf[dst * nop], 1, MPI_DOUBLE, &local_sum[0], 1, MPI_DOUBLE, dst, 0,
                           1, MPI_DOUBLE, MPI_SUM, win);
        MPI_Get_accumulate(&locbuf[dst * nop], 1, MPI_DOUBLE, &local_max[0], 1, MPI_DOUBLE, dst, 1,
                           1, MPI_DOUBLE, MPI_MAX, win);
        MPI_Win_flush_all(win);
        debug_printf("[%d] iter %d: dst %d += locbuf[%d] %.1lf, local sum %.1lf, max %.1lf\n",
                     rank, x, dst, dst * nop, locbuf[dst * nop], local_sum[0], local_max[0]);

        target_computation();

        for (i = 1; i < nop; i++) {
            MPI_Get_accumulate(&locbuf[i + dst * nop], 1, MPI_DOUBLE, &local_sum[i], 1, MPI_DOUBLE,
                               dst, 0, 1, MPI_DOUBLE, MPI_SUM, win);
            MPI_Get_accumulate(&locbuf[i + dst * nop], 1, MPI_DOUBLE, &local_max[i], 1, MPI_DOUBLE,
                               dst, 1, 1, MPI_DOUBLE, MPI_MAX, win);
            debug_printf("[%d] iter %d: dst %d += locbuf[%d] %.1lf\n", rank, x, dst, i + dst * nop,
                         locbuf[i + dst * nop]);
#ifdef MVA
            MPI_Win_flush(dst, win);    /* use it to poke progress in order to finish local CQEs */
#endif
        }
        MPI_Win_flush_all(win);
        for (i = 1; i < nop; i++)
            debug_printf("[%d] iter %d: local sum[%d] %.1lf, max[%d] %.1lf\n", rank, x, i,
                         local_sum[i], i, local_max[i]);
    }

    MPI_Win_unlock_all(win);

    /* need barrier before checking local window buffer */
    MPI_Barrier(MPI_COMM_WORLD);

    target_computation_exit();

    /* need lock on local rank for accessing local window buffer. */
    MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win);
    if (winbuf[0] != sum_result * ITER) {
        fprintf(stderr, "[%d]winbuf[%d] %.1lf != %.1lf (%.1lf * %d)\n", rank, 0,
                winbuf[0], sum_result * ITER, sum_result, ITER);
        errs++;
    }
    if (winbuf[1] != max_result) {
        fprintf(stderr, "[%d]winbuf[%d] %.1lf != %.1lf\n", rank, 1, winbuf[1], max_result);
        errs++;
    }
    MPI_Win_unlock(rank, win);

    if (errs > 0) {
        fprintf(stderr, "[%d] checking failed\n", rank);
#ifdef OUTPUT_FAIL_DETAIL
        fprintf(stderr, "[%d] locbuf:\n", rank);
        for (i = 0; i < nop * nprocs; i++) {
            fprintf(stderr, "%.1lf ", locbuf[i]);
        }
        fprintf(stderr, "\n");
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

    comp_size = SLEEP_TIME;
    locbuf = calloc(NUM_OPS * nprocs, sizeof(double));
    for (i = 0; i < NUM_OPS * nprocs; i++) {
        locbuf[i] = 1.0 * i;
        debug_printf("[%d] locbuf[%d] = %.1lf\n", rank, i, locbuf[i]);
    }

    /* size in byte */
    MPI_Win_allocate(sizeof(double) * 2, sizeof(double), MPI_INFO_NULL,
                     MPI_COMM_WORLD, &winbuf, &win);

    /*
     * P0: SUM{0:NOPS-1}, MAX{0:NOPS-1}
     * P1: SUM{NOPS:2NOPS-1}, MAX{NOPS:2NOPS-1}
     * ...
     */
    for (i = 0; i < 2; i++) {
        winbuf[i] = 0.0;
    }

    max_result = locbuf[NUM_OPS * rank + NUM_OPS - 1];
    for (i = 0; i < NUM_OPS; i++) {
        sum_result += locbuf[NUM_OPS * rank + i];
    }

    MPI_Barrier(MPI_COMM_WORLD);
    errs = run_test(size);

    if (rank == 0) {
        fprintf(stdout, "%d errors\n", errs);
    }

  exit:

    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    if (locbuf)
        free(locbuf);

    MPI_Finalize();

    return 0;
}

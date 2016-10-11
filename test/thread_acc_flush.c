/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mpi.h>
#include <pthread.h>
#include "ctest.h"

/* [THREAD_MULTIPLE TEST]
 * This test checks RMA accumulate with flush on the same window.
 *
 * Single window is allocated on each process. Then every process creates multiple
 * threads, and each thread concurrently issues accumulate to its next rank on that
 * window. The final result on window region should be the same as single threaded
 * execution.*/

#define NUM_OPS 5
#define CHECK
#define OUTPUT_FAIL_DETAIL
#define NTHREADS 4
#define ITER 10

static pthread_t threads[NTHREADS];
static CTEST_thread_tid_arg_t thread_args[NTHREADS];

static MPI_Win win = MPI_WIN_NULL;
static double *winbuf = NULL;
static double locbuf[NUM_OPS * NTHREADS];
static int rank, nprocs;

#ifdef DEBUG
#define debug_printf(str,...) {fprintf(stdout, str, ## __VA_ARGS__);fflush(stdout);}
#else
#define debug_printf(str,...) {}
#endif

/* Every process reset the buffer in local window  (single thread accesses). */
static void reset_win(void)
{
    int i;

    MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win);
    for (i = 0; i < NUM_OPS * NTHREADS; i++)
        winbuf[i] = 0.0;
    MPI_Win_unlock(rank, win);
}

/* Every process initialize the local origin buffer (single thread accesses). */
static void init_local_data(void)
{
    int i;
    for (i = 0; i < NUM_OPS * NTHREADS; i++)
        locbuf[i] = 1.0 * (rank + i + 1);
}

/* Every process checks the result in local window  (single thread accesses). */
static int check_data(void)
{
    int i, check_err = 0;
    int src = 0;

    src = (rank - 1 + nprocs) % nprocs;

    MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win);
    for (i = 0; i < NUM_OPS * NTHREADS; i++) {
        double exp_val = 1.0 * (src + i + 1) * ITER;    /* SUM */

        if (CTEST_precise_double_diff(winbuf[i], exp_val)) {
            fprintf(stderr, "[%d] winbuf[%d] %.1lf != %.1lf\n", rank, i, winbuf[i], exp_val);
            check_err++;
        }
    }
    MPI_Win_unlock(rank, win);

    return check_err;
}

#ifdef DEBUG
/* Every process prints the values in local window  (single thread accesses, collective). */
static void print_winbufs(void)
{
    int i, p;
    MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win);
    for (p = 0; p < nprocs; p++) {
        if (rank == p) {
            printf("winbuf on rank %d:\n", rank);
            for (i = 0; i < NUM_OPS * NTHREADS; i++) {
                printf("%.2f ", winbuf[i]);
            }
            printf("\n");
            fflush(stdout);
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }
    MPI_Win_unlock(rank, win);
}
#endif

/* Every thread issues accumulate to the next rank with [tid * NUM_OPS] displacement
 * (multiple threads access). */
static void *run_test(void *arg)
{
    CTEST_thread_tid_arg_t *my_arg = (CTEST_thread_tid_arg_t *) arg;
    int tid = my_arg->tid;
    int i, x;
    int dst;
    MPI_Aint disp;

    debug_printf("rank %d thread %d start\n", rank, tid);

    dst = (rank + 1) % nprocs;
    disp = tid * NUM_OPS;
    for (x = 0; x < ITER; x++) {
        debug_printf("rank %d thread %d test iter %d\n", rank, tid, x);

        /* enable load balancing */
        MPI_Accumulate(&locbuf[disp], 1, MPI_DOUBLE, dst, disp, 1, MPI_DOUBLE, MPI_SUM, win);
        MPI_Win_flush(dst, win);

        for (i = 1; i < NUM_OPS; i++)
            MPI_Accumulate(&locbuf[disp + i], 1, MPI_DOUBLE, dst, disp + i, 1,
                           MPI_DOUBLE, MPI_SUM, win);
        MPI_Win_flush(dst, win);
    }

    debug_printf("rank %d thread %d test done\n", rank, tid);
    return NULL;
}

int main(int argc, char *argv[])
{
    int provided = 0;
    int perrs, errs_total = 0;
    int i;

    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided != MPI_THREAD_MULTIPLE) {
        fprintf(stdout, "This test requires MPI_THREAD_MULTIPLE, but %d\n", provided);
        fflush(stdout);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (nprocs < 2) {
        fprintf(stderr, "Please run using at least 2 processes\n");
        goto exit;
    }

    MPI_Win_allocate(sizeof(double) * NUM_OPS * NTHREADS, sizeof(double), MPI_INFO_NULL,
                     MPI_COMM_WORLD, &winbuf, &win);
    reset_win();
    init_local_data();
    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Win_lock_all(0, win);   /* two threads cannot lock concurrently */

    for (i = 0; i < NTHREADS; i++) {
        thread_args[i].tid = i;
        CTEST_create_thread(&threads[i], &run_test, &thread_args[i]);
    }

    for (i = 0; i < NTHREADS; i++)
        pthread_join(threads[i], 0);

    MPI_Win_unlock_all(win);

    MPI_Barrier(MPI_COMM_WORLD);
    perrs = check_data();
    MPI_Reduce(&perrs, &errs_total, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

#ifdef DEBUG
/* Every process prints the values in local window  (single thread accesses). */
    print_winbufs();
#endif
    MPI_Win_free(&win);

  exit:
    if (rank == 0)
        CTEST_report_result(errs_total);

    MPI_Finalize();

    return 0;
}

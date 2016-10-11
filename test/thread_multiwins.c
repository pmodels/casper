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
 * This test checks window allocation with MPI_THREAD_MULTIPLE safety.
 *
 * Each thread allocates its window on separate communicator. Then each
 * thread simply issues accumulate to the next rank on that thread's window.
 * Since each thread accesses a different window with different memory
 * region, the result should be the same as single thread execution.*/

#define NUM_OPS 5
#define CHECK
#define OUTPUT_FAIL_DETAIL
#define NTHREADS 4
#define ITER 10

static int err;
static CTEST_atomic_var_t atomic_err;
static pthread_t threads[NTHREADS];
static CTEST_thread_tid_arg_t thread_args[NTHREADS];

static MPI_Comm comms[NTHREADS];
static MPI_Win wins[NTHREADS];
static double *winbufs[NTHREADS];
static double locbuf[NUM_OPS * NTHREADS], checkbuf[NUM_OPS * NTHREADS];
static int rank, nprocs;

#ifdef DEBUG
#define debug_printf(str,...) {fprintf(stdout, str, ## __VA_ARGS__);fflush(stdout);}
#else
#define debug_printf(str,...) {}
#endif

/* Every thread resets the buffer in its local window
 * (multiple threads access on separate windows). */
static void reset_win(int tid)
{
    int i;
    MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, wins[tid]);
    for (i = 0; i < NUM_OPS; i++)
        winbufs[tid][i] = 0.0;
    MPI_Win_unlock(rank, wins[tid]);
}

/* Every thread initializes its local origin buffer in every iteration
 * (multiple threads access on separate buffers). */
static void change_data(int tid, int x)
{
    int i;
    for (i = 0; i < NUM_OPS; i++)
        locbuf[tid * NUM_OPS + i] = 1.0 * (x + 1) * (i + 1);
}

/* Every thread checks the result after every iteration
 * (multiple threads access on separate buffers). */
static int check_data(int tid, int dst)
{
    int i, check_err = 0;
    int buf_disp = tid * NUM_OPS;

    /* note that it is in an epoch */
    memset(&checkbuf[buf_disp], 0, NUM_OPS * sizeof(double));

    MPI_Get(&checkbuf[buf_disp], NUM_OPS, MPI_DOUBLE, dst, 0, NUM_OPS, MPI_DOUBLE, wins[tid]);
    MPI_Win_flush(dst, wins[tid]);

    for (i = 0; i < NUM_OPS; i++) {
        if (CTEST_precise_double_diff(checkbuf[buf_disp + i], locbuf[buf_disp + i])) {
            fprintf(stderr, "[%d] tid %d, winbuf[%d] %.1lf != %.1lf\n",
                    dst, tid, i, checkbuf[buf_disp + i], locbuf[buf_disp + i]);
            check_err++;
        }
    }

    return check_err;
}

/* Every thread allocates window, issues accumulate, frees window.
 * (multiple threads access on separate windows and buffers). */
static void *run_test(void *arg)
{
    CTEST_thread_tid_arg_t *my_arg = (CTEST_thread_tid_arg_t *) arg;
    int tid = my_arg->tid;
    int i, x, check_err = 0;
    int dst;

    debug_printf("rank %d thread %d start on comm 0x%x\n", rank, tid, comms[tid]);

    MPI_Win_allocate(sizeof(double) * NUM_OPS, sizeof(double), MPI_INFO_NULL,
                     comms[tid], &winbufs[tid], &wins[tid]);
    reset_win(tid);

    debug_printf("rank %d thread %d allocated window 0x%x\n", rank, tid, wins[tid]);
    MPI_Barrier(comms[tid]);

    dst = (rank + 1) % nprocs;
    for (x = 0; x < ITER; x++) {
        change_data(tid, x);

        debug_printf("rank %d thread %d test iter %d\n", rank, tid, x);
        MPI_Win_lock_all(0, wins[tid]);

        /* enable load balancing */
        MPI_Accumulate(&locbuf[tid * NUM_OPS], 1, MPI_DOUBLE, dst, 0, 1,
                       MPI_DOUBLE, MPI_MAX, wins[tid]);
        MPI_Win_flush(dst, wins[tid]);

        for (i = 1; i < NUM_OPS; i++)
            MPI_Accumulate(&locbuf[tid * NUM_OPS + i], 1, MPI_DOUBLE, dst, i, 1,
                           MPI_DOUBLE, MPI_MAX, wins[tid]);
        MPI_Win_flush(dst, wins[tid]);

        check_err = check_data(tid, dst);
        CTEST_ATOMIC_VAR_ADD(atomic_err, int, check_err);

        MPI_Win_unlock_all(wins[tid]);
    }

    MPI_Win_free(&wins[tid]);
    debug_printf("rank %d thread %d test done, check_err=%d\n", rank, tid, check_err);

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

    /* Separate communicator used by each thread */
    for (i = 0; i < NTHREADS; i++)
        MPI_Comm_dup(MPI_COMM_WORLD, &comms[i]);

    CTEST_ATOMIC_VAR_INIT(atomic_err, &err);

    for (i = 0; i < NTHREADS; i++) {
        thread_args[i].tid = i;
        CTEST_create_thread(&threads[i], &run_test, &thread_args[i]);
    }

    for (i = 0; i < NTHREADS; i++)
        pthread_join(threads[i], 0);

    CTEST_ATOMIC_VAR_READ(atomic_err, int, perrs);

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Reduce(&perrs, &errs_total, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    CTEST_ATOMIC_VAR_DESTROY(atomic_err);

    for (i = 0; i < NTHREADS; i++)
        MPI_Comm_free(&comms[i]);

  exit:
    if (rank == 0)
        CTEST_report_result(errs_total);

    MPI_Finalize();

    return 0;
}

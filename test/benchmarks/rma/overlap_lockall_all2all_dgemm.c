/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>

/* This benchmark evaluates the asynchronous progress in a comm-dgemm-comm test.
 * Every process issues accumulate to all the other processes in every communication
 * stage, and then performs dgemm computation, then performs the communication again. */

#define USE_DGEMM
#include <f2c.h>
#include <clapack.h>
#include <blaswrap.h>

#define D_DGEMM_SIZE 500

//#define DEBUG
//#define CHECK
#define ITER_S 500
#define ITER_L 200
#define NPROCS_M 16

#ifdef DEBUG
#define debug_printf(str,...) {fprintf(stdout, str, ## __VA_ARGS__);fflush(stdout);}
#else
#define debug_printf(str,...) {}
#endif

double *winbuf = NULL;
double *locbuf = NULL;
int rank, nprocs, nprocs_local;
MPI_Win win = MPI_WIN_NULL;
int ITER = ITER_S;

int DGEMM_SIZE = D_DGEMM_SIZE;
int NOP = 100;

doublereal *A, *B, *C;
static int target_computation_init()
{
    A = (doublereal *) malloc(DGEMM_SIZE * DGEMM_SIZE * sizeof(doublereal));
    B = (doublereal *) malloc(DGEMM_SIZE * DGEMM_SIZE * sizeof(doublereal));
    C = (doublereal *) malloc(DGEMM_SIZE * DGEMM_SIZE * sizeof(doublereal));
    if (A == NULL || B == NULL || C == NULL) {
        printf("\n ERROR: Can't allocate memory for matrices. Aborting... \n\n");
        free(A);
        free(B);
        free(C);
        return 1;
    }
    return 0;
}

/**
 * C=alpha*A*B+beta*C
 */
static int target_computation()
{
    integer m, n, k, i, j;
    doublereal alpha, beta;
    char *ntran = "N";

    m = k = n = DGEMM_SIZE;
    alpha = 1.0;
    beta = 2.0;

    for (i = 0; i < (m * k); i++) {
        A[i] = (doublereal) (i + 1);
    }

    for (i = 0; i < (k * n); i++) {
        B[i] = (doublereal) (-i - 1);
    }

    for (i = 0; i < (m * n); i++) {
        C[i] = 1.0;
    }

    dgemm_(ntran, ntran, &m, &n, &k, &alpha, A, &k, B, &n, &beta, C, &n);
    return 0;
}

static int target_computation_exit()
{
    free(A);
    free(B);
    free(C);
    return 0;
}

static int run_test(int nop)
{
    int i, x, errs = 0, errs_total = 0;
    MPI_Status stat;
    int dst;
    int winbuf_offset = 0;
    double t0, avg_total_time = 0.0, t_total = 0.0;
    double sum = 0.0;

    if (nprocs <= NPROCS_M) {
        ITER = ITER_S;
    }
    else {
        ITER = ITER_L;
    }

    target_computation_init();
    MPI_Win_lock_all(0, win);

    t0 = MPI_Wtime();
    for (x = 0; x < ITER; x++) {

        // send to all the left processes in a ring style
        for (dst = (rank + 1) % nprocs; dst != rank; dst = (dst + 1) % nprocs) {
            MPI_Accumulate(&locbuf[0], 1, MPI_DOUBLE, dst, rank, 1, MPI_DOUBLE, MPI_SUM, win);
        }
        MPI_Win_flush_all(win);

        target_computation();

        for (dst = (rank + 1) % nprocs; dst != rank; dst = (dst + 1) % nprocs) {
            for (i = 1; i < nop; i++) {
                MPI_Accumulate(&locbuf[i], 1, MPI_DOUBLE, dst, rank, 1, MPI_DOUBLE, MPI_SUM, win);
            }
        }
        MPI_Win_flush_all(win);

        debug_printf("[%d]MPI_Win_flush all done\n", x);
    }
    t_total += MPI_Wtime() - t0;
    t_total /= ITER;

    MPI_Win_unlock_all(win);
    MPI_Barrier(MPI_COMM_WORLD);

    target_computation_exit();

#ifdef CHECK
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win);
    sum = 0.0;
    for (i = 0; i < nop; i++) {
        sum += locbuf[i];
    }
    sum *= ITER;
    for (i = 0; i < nprocs; i++) {
        if (i == rank)
            continue;
        if (winbuf[i] != sum) {
            fprintf(stderr,
                    "[%d]computation error : winbuf[%d] %.2lf != %.2lf, nop %d\n",
                    rank, i, winbuf[i], sum, nop);
            errs += 1;
        }
    }
    MPI_Win_unlock(rank, win);
#endif

    MPI_Reduce(&t_total, &avg_total_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Allreduce(&errs, &errs_total, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    if (rank == 0) {
        avg_total_time /= nprocs;
#ifdef ENABLE_CSP
        fprintf(stdout,
                "casper: comp_size %d num_op %d nprocs %d total_time %lf\n",
                DGEMM_SIZE, nop, nprocs, avg_total_time);
#else
        fprintf(stdout,
                "orig: comp_size %d num_op %d nprocs %d total_time %lf\n",
                DGEMM_SIZE, nop, nprocs, avg_total_time);
#endif
    }

    return errs_total;
}

int main(int argc, char *argv[])
{
    int i, errs;
    int min_dg_size = SLEEP_TIME, max_dg_size = SLEEP_TIME, iter_dg_size = 1, size;

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    debug_printf("[%d]init done, %d/%d\n", rank, rank, nprocs);

    if (nprocs < 2) {
        fprintf(stderr, "Please run using at least 2 processes\n");
        goto exit;
    }

    if (argc >= 3) {
        NOP = atoi(argv[2]);
    }

    if (argc >= 6) {
        min_dg_size = atoi(argv[3]);
        max_dg_size = atoi(argv[4]);
        iter_dg_size = atoi(argv[5]);
    }

    locbuf = malloc(sizeof(double) * NOP);
    for (i = 0; i < NOP; i++) {
        locbuf[i] = 1.0;
    }

    // size in byte
    MPI_Win_allocate(sizeof(double) * nprocs, sizeof(double), MPI_INFO_NULL,
                     MPI_COMM_WORLD, &winbuf, &win);
    debug_printf("[%d]win_allocate done\n", rank);

    for (size = min_dg_size; size <= max_dg_size; size *= iter_dg_size) {
        /* reset window */
        MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win);
        for (i = 0; i < nprocs; i++) {
            winbuf[i] = 0.0;
        }
        MPI_Win_unlock(rank, win);

        MPI_Barrier(MPI_COMM_WORLD);
        DGEMM_SIZE = size;

        errs = run_test(NOP);
        if (errs > 0)
            break;
    }

  exit:

    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    if (locbuf)
        free(locbuf);

    MPI_Finalize();

    return 0;
}

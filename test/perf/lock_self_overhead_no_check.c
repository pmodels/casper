/*
 * lock_self_overhead.c
 *
 *  This benchmark evaluates the overhead of Manticore wrapped MPI_Win_lock
 *  when lock a self target using 2 processes (only rank 0 is used, but window requires at least 2 processes).
 *
 *  Rank 0 locks itself and unlock. It does not need RMA operations because
 *  local lock will be granted immediately. However, we still issue a accumulate
 *  in order to comparing with lock_self_overhead_no_loadstore.c
 *
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>

/* #define DEBUG */
#define CHECK
#define ITER 1000000
#define SKIP 100

double *winbuf = NULL;
double locbuf[1];
int rank, nprocs;
MPI_Win win = MPI_WIN_NULL;
int NOP = 1;

#ifdef MTCORE
extern int MTCORE_NUM_H;
#endif

static int run_test()
{
    int i, x, errs = 0, errs_total = 0;
    MPI_Status stat;
    int dst;
    int winbuf_offset = 0;
    double t0, avg_total_time = 0.0, t_total = 0.0;
    double sum = 0.0;

    dst = 0;
    for (x = 0; x < SKIP; x++) {
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, dst, MPI_MODE_NOCHECK, win);
        for (i = 0; i < NOP; i++)
            MPI_Put(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, win);
        MPI_Win_unlock(dst, win);
    }

    t0 = MPI_Wtime();

    for (x = 0; x < ITER; x++) {
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, dst, MPI_MODE_NOCHECK, win);
        for (i = 0; i < NOP; i++)
            MPI_Put(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, win);
        MPI_Win_unlock(dst, win);
    }

    t_total = (MPI_Wtime() - t0) * 1000 * 1000; /*us */
    t_total /= ITER;

#ifdef CHECK
    if (rank == dst) {
        /* need lock on self rank for checking window buffer.
         * otherwise, the result may be incorrect because flush/unlock
         * doesn't wait for target completion in exclusive lock */
        MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win);
        sum = 1.0 * (ITER + SKIP) * NOP;
        if (winbuf[0] != sum) {
            fprintf(stderr, "[%d]computation error : winbuf %.2lf != %.2lf\n", rank, winbuf[0],
                    sum);
            errs += 1;
        }
        MPI_Win_unlock(rank, win);
    }

    errs_total = errs;
    if (errs_total > 0)
        goto exit;
#endif

    fprintf(stdout, "mtcore-nocheck: iter %d num_op %d nprocs %d nh %d total_time %.2lf\n",
            ITER, NOP, nprocs, MTCORE_NUM_H, t_total);

  exit:

    return errs_total;
}

int main(int argc, char *argv[])
{
    int errs;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

#ifdef MTCORE
    /* first argv is nh */
    if (argc >= 3) {
        NOP = atoi(argv[2]);
    }
#else
    if (argc >= 2) {
        NOP = atoi(argv[1]);
    }
#endif

    locbuf[0] = 1.0;
    MPI_Win_allocate(sizeof(double), sizeof(double), MPI_INFO_NULL, MPI_COMM_WORLD, &winbuf, &win);

    if (rank == 0) {
        errs = run_test();
    }

  exit:

    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    MPI_Finalize();

    return 0;
}

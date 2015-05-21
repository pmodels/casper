/*
 * win-lockall-overhead.c
 *
 *  This benchmark evaluates the overhead of Win_lock_all with
 *  user-specified number of processes (>= 2). Rank 0 locks
 *  all the processes and issues accumulate operations to all
 *  of them.
 *
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>

/* #define DEBUG */
#define CHECK

#define ITER_S 10000
#define ITER_L 5000
#define ITER_LL 1000
#define ITER_LLL 500

#define SKIP 10
#define NPROCS_M 16

double *winbuf = NULL;
double locbuf[1];
int rank, nprocs;
MPI_Win win = MPI_WIN_NULL;
int ITER = ITER_S;
#ifdef ENABLE_CSP
extern int CSP_NUM_G;
#endif

int NOP_MAX = 1, NOP_MIN = 1, NOP_ITER = 2;     /* us */
unsigned long SLEEP_TIME = 100;
int NOP;

static int target_computation()
{
    double start = MPI_Wtime() * 1000 * 1000;
    while (MPI_Wtime() * 1000 * 1000 - start < SLEEP_TIME);
    return 0;
}

static int run_test()
{
    int i, x, errs = 0;
    int dst, flag = 0;
    double t0, avg_total_time = 0.0, t_total = 0.0;
    double sum = 0.0;
    MPI_Status stat;
    MPI_Request req;
    int buf[1] = { 1 };

    if (nprocs <= NPROCS_M) {
        ITER = ITER_S;
    }
    else if (nprocs > NPROCS_M && SLEEP_TIME < 50 && NOP < 64) {
        ITER = ITER_L;
    }
    else if (SLEEP_TIME < 100 && NOP < 128) {
        ITER = ITER_LL;
    }
    else {
        ITER = ITER_LLL;
    }

    for (x = 0; x < SKIP; x++) {
        MPI_Win_lock_all(0, win);
        for (dst = 0; dst < nprocs; dst++) {
            MPI_Accumulate(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, MPI_SUM, win);
        }
        MPI_Win_unlock_all(win);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    t0 = MPI_Wtime();

    for (x = 0; x < ITER; x++) {
        MPI_Win_lock_all(0, win);

        for (dst = 0; dst < nprocs; dst++) {
            for (i = 0; i < NOP; i++) {
                MPI_Accumulate(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, MPI_SUM, win);
            }
        }

        target_computation();

        MPI_Win_unlock_all(win);
    }

    t_total = (MPI_Wtime() - t0) * 1000 * 1000; /*us */
    t_total /= ITER;

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Reduce(&t_total, &avg_total_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        avg_total_time = avg_total_time / nprocs;       /* us */

#ifdef ENABLE_CSP
        fprintf(stdout, "casper: iter %d comp_size %d num_op %d nprocs %d nh %d total_time %.2lf\n",
                ITER, SLEEP_TIME, NOP, nprocs, CSP_NUM_G, avg_total_time);
#else
        fprintf(stdout, "orig: iter %d comp_size %d num_op %d nprocs %d total_time %.2lf\n",
                ITER, SLEEP_TIME, NOP, nprocs, avg_total_time);
#endif
    }

    return errs;
}

int main(int argc, char *argv[])
{
    int errs = 0;
    MPI_Info win_info = MPI_INFO_NULL;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (nprocs < 2) {
        fprintf(stderr, "Please run using at least 2 processes\n");
        goto exit;
    }

#ifdef ENABLE_CSP
    /* first argv is nh */
    if (argc >= 5) {
        NOP_MIN = atoi(argv[2]);
        NOP_MAX = atoi(argv[3]);
        NOP_ITER = atoi(argv[4]);
    }
    if (argc >= 6) {
        SLEEP_TIME = atoi(argv[5]);
    }
#else
    if (argc >= 4) {
        NOP_MIN = atoi(argv[1]);
        NOP_MAX = atoi(argv[2]);
        NOP_ITER = atoi(argv[3]);
    }
    if (argc >= 5) {
        SLEEP_TIME = atoi(argv[4]);
    }
#endif

    locbuf[0] = (rank + 1) * 1.0;

    MPI_Info_create(&win_info);
    MPI_Info_set(win_info, (char *) "epoch_type", (char *) "lockall");

    MPI_Win_allocate(sizeof(double), sizeof(double), win_info, MPI_COMM_WORLD, &winbuf, &win);

    for (NOP = NOP_MIN; NOP <= NOP_MAX; NOP *= NOP_ITER) {
        /* reset */
        MPI_Win_lock_all(0, win);
        winbuf[0] = 0.0;
        MPI_Win_unlock_all(win);
        MPI_Barrier(MPI_COMM_WORLD);

        errs = run_test();
    }

  exit:
    if (win_info != MPI_INFO_NULL)
        MPI_Info_free(&win_info);
    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    MPI_Finalize();

    return 0;
}

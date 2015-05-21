/*
 * lock_self_overhead_no_loadstore.c
 *
 *  This benchmark evaluates the overhead of CASPER wrapped MPI_Win_lock
 *  when lock a self target using 2 processes with no_local_load_store option.
 *  (only rank 0 is used, but window requires at least 2 processes).
 *
 *  Rank 0 locks itself and issues an accumulate operation to grant that lock.
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

#ifdef ENABLE_CSP
extern int CSP_NUM_G;
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
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, dst, 0, win);
        for (i = 0; i < NOP; i++)
            MPI_Put(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, win);
        MPI_Win_unlock(dst, win);
    }

    t0 = MPI_Wtime();

    for (x = 0; x < ITER; x++) {
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, dst, 0, win);
        for (i = 0; i < NOP; i++)
            MPI_Put(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, win);
        MPI_Win_unlock(dst, win);
    }

    t_total = (MPI_Wtime() - t0) * 1000 * 1000; /*us */
    t_total /= ITER;

    fprintf(stdout, "casper-nols: iter %d num_op %d nprocs %d nh %d total_time %.2lf\n",
            ITER, NOP, nprocs, CSP_NUM_G, t_total);

  exit:

    return errs_total;
}

int main(int argc, char *argv[])
{
    int errs;
    MPI_Info win_info = MPI_INFO_NULL;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

#ifdef ENABLE_CSP
    /* first argv is nh */
    if (argc >= 3) {
        NOP = atoi(argv[2]);
    }
#else
    if (argc >= 2) {
        NOP = atoi(argv[1]);
    }
#endif

    MPI_Info_create(&win_info);
    MPI_Info_set(win_info, (char *) "no_local_load_store", (char *) "true");

    locbuf[0] = 1.0;
    MPI_Win_allocate(sizeof(double), sizeof(double), win_info, MPI_COMM_WORLD, &winbuf, &win);

    if (rank == 0) {
        errs = run_test();
    }
  exit:

    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    if (win_info != MPI_INFO_NULL)
        MPI_Info_free(&win_info);
    MPI_Finalize();

    return 0;
}

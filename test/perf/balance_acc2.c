/*
 * balance_acc2.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>

#define NUM_OPS_MIN 10
#define NUM_OPS_MAX 10000
#define NUM_OPS_INC 4

#define CHECK
#define SKIP 10

double *winbuf = NULL;
double *locbuf = NULL;
int rank, nprocs;
MPI_Win win = MPI_WIN_NULL;
int ITER = 1000;

#ifdef ENABLE_CSP
extern int CSP_NUM_G;
#endif

static int run_test(int nop)
{
    int i, x;
    int dst;
    double t0, t_total = 0.0;

    if (rank == 0) {
        for (x = 0; x < SKIP; x++) {
            MPI_Win_lock_all(0, win);
            for (dst = 0; dst < nprocs; dst++) {
                MPI_Accumulate(&locbuf[dst * nop], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, MPI_SUM,
                               win);
            }
            MPI_Win_unlock_all(win);
        }

        t0 = MPI_Wtime();

        for (x = 0; x < ITER; x++) {
            MPI_Win_lock_all(0, win);
            /* no load balancing */
            for (dst = 0; dst < nprocs; dst++) {
                for (i = 0; i < nop; i++) {
                    MPI_Accumulate(&locbuf[dst * nop + i], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE,
                                   MPI_SUM, win);
                }
            }

            MPI_Win_unlock_all(win);
        }

        t_total += (MPI_Wtime() - t0) * 1000 * 1000;
        t_total /= ITER;
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
#ifdef ENABLE_CSP
        fprintf(stdout, "casper: nprocs %d nh %d num_op %d total_time %lf\n", nprocs,
                CSP_NUM_G, nop, t_total);
#else
        fprintf(stdout, "orig: num_op %d nprocs %d total_time %lf\n", nprocs, nop, t_total);
#endif
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int size = NUM_OPS_MIN;
    int i, errs = 0;
    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (nprocs < 2) {
        fprintf(stderr, "Please run using at least 2 processes\n");
        goto exit;
    }

    locbuf = calloc(NUM_OPS_MAX * nprocs, sizeof(double));
    for (i = 0; i < NUM_OPS_MAX * nprocs; i++) {
        locbuf[i] = 1.0 * i;
    }

    /* size in byte */
    MPI_Win_allocate(sizeof(double), sizeof(double), MPI_INFO_NULL, MPI_COMM_WORLD, &winbuf, &win);
    winbuf[0] = 0.0;

    MPI_Barrier(MPI_COMM_WORLD);
    for (size = NUM_OPS_MIN; size < NUM_OPS_MAX; size *= NUM_OPS_INC) {
        run_test(size);
    }

  exit:

    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    if (locbuf)
        free(locbuf);

    MPI_Finalize();

    return 0;
}

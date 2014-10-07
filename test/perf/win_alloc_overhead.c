/*
 * win_allocate.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>

#define ITER 100
int rank, nprocs;
double *winbuf[ITER];
MPI_Win win[ITER];
int size = 16;

static void run_test1()
{
    int x;
    double t0, t1, t_alloc, t_free;

    t0 = MPI_Wtime();
    for (x = 0; x < ITER; x++) {
        /* size in byte */
        MPI_Win_allocate(sizeof(double) * size, sizeof(double), MPI_INFO_NULL,
                         MPI_COMM_WORLD, &winbuf[x], &win[x]);
    }
    t1 = MPI_Wtime();
    t_alloc = (t1 - t0) / ITER;

    for (x = 0; x < ITER; x++) {
        MPI_Win_free(&win[x]);
    }
    t_free = (MPI_Wtime() - t1) / ITER;

    if (rank == 0)
        fprintf(stdout, "nproc %d size %d allocate %lf free %lf\n", nprocs, size, t_alloc, t_free);
}

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (nprocs < 2) {
        fprintf(stderr, "Please run using at least 2 processes\n");
        goto exit;
    }

    if (argc > 1) {
        size = atoi(argv[1]);
    }

    if (size <= 0) {
        fprintf(stderr, "wrong size %d\n", size);
        goto exit;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    run_test1();

  exit:
    MPI_Finalize();

    return 0;
}

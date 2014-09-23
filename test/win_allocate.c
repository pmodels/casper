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

#define ITER 50
int rank, nprocs;
double *winbuf[ITER];
MPI_Win win[ITER];
int size = 16;

static void run_test1()
{
    int x;

    fprintf(stdout, "[%d]-----check %d * [win_allocate(0x%x) + win_free] \n", rank, ITER,
            sizeof(double) * size);

    for (x = 0; x < ITER; x++) {
        /* size in byte */
        MPI_Win_allocate(sizeof(double) * size, sizeof(double), MPI_INFO_NULL,
                         MPI_COMM_WORLD, &winbuf[x], &win[x]);
    }

    for (x = 0; x < ITER; x++) {
        MPI_Win_free(&win[x]);
    }
}

static void run_test2()
{
    int x;

    fprintf(stdout, "[%d]-----check %d * [win_allocate(0x%x)] + %d * [win_free] \n",
            rank, ITER, sizeof(double) * size, ITER);

    for (x = 0; x < ITER; x++) {
        /* size in byte */
        MPI_Win_allocate(sizeof(double) * size, sizeof(double), MPI_INFO_NULL,
                         MPI_COMM_WORLD, &winbuf[x], &win[x]);

        MPI_Win_free(&win[x]);
    }
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

    MPI_Barrier(MPI_COMM_WORLD);
    run_test1();

    MPI_Barrier(MPI_COMM_WORLD);
    run_test2();

  exit:
    if (rank == 0) {
        fprintf(stdout, "0 errors\n");
    }
    MPI_Finalize();

    return 0;
}

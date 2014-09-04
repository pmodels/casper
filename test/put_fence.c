/*
 * put.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>

#define NUM_OPS 5
#define CHECK
#define OUTPUT_FAIL_DETAIL

double *winbuf = NULL;
double *locbuf = NULL;
int rank, nprocs;
MPI_Win win = MPI_WIN_NULL;
int ITER = 2;

static int run_test1(int nop)
{
    int i, x, errs = 0, errs_total = 0;
    int dst;

    fprintf(stdout, "[%d]-----check fence/%d * put[0 - %d]/fence\n", rank, nop - 1, nprocs - 1);

    for (x = 0; x < ITER; x++) {
        MPI_Win_fence(0, win);

        for (dst = 0; dst < nprocs; dst++) {
            for (i = 0; i < nop; i++) {
                MPI_Put(&locbuf[dst + i * nprocs], 1, MPI_DOUBLE, dst, i, 1, MPI_DOUBLE, win);
            }
        }
        MPI_Win_fence(0, win);

        /* check in every iteration */
        MPI_Win_fence(0, win);
        for (i = 0; i < nop; i++) {
            if (winbuf[i] != (1.0 * rank + i * nprocs)) {
                fprintf(stderr, "[%d] winbuf[%d] %.1lf != %.1lf\n", rank, i,
                        winbuf[i], 1.0 * rank + i * nprocs);
                errs++;
            }
        }
        MPI_Win_fence(0, win);
    }

    if (errs > 0) {
        fprintf(stderr, "[%d] checking failed\n", rank);
#ifdef OUTPUT_FAIL_DETAIL
        fprintf(stderr, "[%d] locbuf:\n");
        for (i = 0; i < nop * nprocs; i++) {
            fprintf(stderr, "%.1lf ", locbuf[i]);
        }
        fprintf(stderr, "\n");
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

    locbuf = calloc(NUM_OPS * nprocs, sizeof(double));
    for (i = 0; i < NUM_OPS * nprocs; i++) {
        locbuf[i] = 1.0 * i;
    }

    /* size in byte */
    MPI_Win_allocate(sizeof(double) * NUM_OPS, sizeof(double), MPI_INFO_NULL,
                     MPI_COMM_WORLD, &winbuf, &win);

    /*
     * P0: 0 + [0:NOPS-1] * nprocs
     * P1: 1 + [0:NOPS-1] * nprocs
     * ...
     */
    for (i = 0; i < NUM_OPS; i++) {
        winbuf[i] = 0.0;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    errs = run_test1(size);
    if (errs)
        goto exit;

  exit:
    if (rank == 0) {
        fprintf(stdout, "%d errors\n", errs);
    }

    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    if (locbuf)
        free(locbuf);

    MPI_Finalize();

    return 0;
}

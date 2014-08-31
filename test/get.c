/*
 * get.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>

#define SLEEP_TIME 100  /* 100us */
#define NUM_OPS 5
#define CHECK
#define OUTPUT_FAIL_DETAIL

double *winbuf = NULL;
double *locbuf = NULL;
int rank, nprocs;
int comp_size = 1;
MPI_Win win = MPI_WIN_NULL;
int ITER = 2;

static int run_test1(int nop)
{
    int i, x, errs = 0, errs_total = 0;
    int dst;

    MPI_Win_lock_all(0, win);

    fprintf(stdout, "[%d]-----check lock_all/get[0 - %d] & flush_all + "
            "get[0 - %d] & flush_all/unlock_all \n", rank, nprocs - 1, nprocs - 1);

    for (x = 0; x < ITER; x++) {
        for (dst = 0; dst < nprocs; dst++) {
            MPI_Get(&locbuf[dst], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, win);
        }
        MPI_Win_flush_all(win);

        for (dst = 0; dst < nprocs; dst++) {
            for (i = 1; i < nop; i++) {
                MPI_Get(&locbuf[dst + i * nprocs], 1, MPI_DOUBLE, dst, i, 1, MPI_DOUBLE, win);
#ifdef MVA
                MPI_Win_flush(dst, win);        /* use it to poke progress in order to finish local CQEs */
#endif
            }
        }
        MPI_Win_flush_all(win);

        /* check in every iteration */
        for (i = 0; i < nop; i++) {
            for (dst = 0; dst < nprocs; dst++) {
                if (locbuf[dst + i * nprocs] != (1.0 * i + dst * nprocs)) {
                    fprintf(stderr, "[%d] locbuf[%d] %.1lf != %.1lf\n", rank, dst + i * nprocs,
                            locbuf[dst + i * nprocs], 1.0 * i + dst * nprocs);
                    errs++;
                }
            }
        }
    }

    MPI_Win_unlock_all(win);

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

static int run_test2(int nop)
{
    int i, x, errs = 0, errs_total = 0;
    int dst;

    MPI_Win_lock_all(0, win);

    fprintf(stdout, "[%d]-----check lock_all/%d * get[0 - %d] & flush_all/unlock_all \n",
            rank, nop, nprocs - 1);

    for (x = 0; x < ITER; x++) {

        /* only do load balancing when force lock enabled */
        for (dst = 0; dst < nprocs; dst++) {
            for (i = 0; i < nop; i++) {
                MPI_Get(&locbuf[dst + i * nprocs], 1, MPI_DOUBLE, dst, i, 1, MPI_DOUBLE, win);
#ifdef MVA
                MPI_Win_flush(dst, win);        /* use it to poke progress in order to finish local CQEs */
#endif
            }
        }
        MPI_Win_flush_all(win);

        /* check in every iteration */
        for (i = 0; i < nop; i++) {
            for (dst = 0; dst < nprocs; dst++) {
                if (locbuf[dst + i * nprocs] != (1.0 * i + dst * nprocs)) {
                    fprintf(stderr, "[%d] locbuf[%d] %.1lf != %.1lf\n", rank, dst + i * nprocs,
                            locbuf[dst + i * nprocs], 1.0 * i + dst * nprocs);
                    errs++;
                }
            }
        }
    }

    MPI_Win_unlock_all(win);

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

    comp_size = SLEEP_TIME;
    locbuf = calloc(NUM_OPS * nprocs, sizeof(double));

    /* size in byte */
    MPI_Win_allocate(sizeof(double) * NUM_OPS, sizeof(double), MPI_INFO_NULL,
                     MPI_COMM_WORLD, &winbuf, &win);

    /*
     * P0: 0,1,...,NUM_OPS-1
     * P1: NUM_OPS,...,2NUM_OPS-1
     * ...
     */
    for (i = 0; i < NUM_OPS; i++) {
        winbuf[i] = 1.0 * i + rank * nprocs;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    errs = run_test1(size);
    if (errs)
        goto exit;

    MPI_Barrier(MPI_COMM_WORLD);
    errs = run_test2(size);
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

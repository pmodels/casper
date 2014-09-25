/*
 * win_create_acc.c
 *  <FILE_DESC>
 * 	
 * 	Check whether normal window can work correctly with manticore.
 *
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mpi.h>

#define NUM_OPS 5
#define CHECK
#define OUTPUT_FAIL_DETAIL

double *winbuf = NULL;
double locbuf[NUM_OPS], checkbuf[NUM_OPS];
int rank, nprocs;
MPI_Win win = MPI_WIN_NULL;
int ITER = 10;

static void reset_win()
{
    int i;
    for (i = 0; i < NUM_OPS; i++) {
        winbuf[i] = 0.0;
    }
}

static void change_data(int nop, int x)
{
    int i;
    for (i = 0; i < nop; i++) {
        locbuf[i] = 1.0 * (x + 1) * (i + 1);
    }
}

static int check_data(int nop, int x, int dst)
{
    int errs = 0;
    /* note that it is in an epoch */
    int i;

    memset(checkbuf, 0, NUM_OPS * sizeof(double));

    MPI_Get(checkbuf, nop, MPI_DOUBLE, dst, 0, nop, MPI_DOUBLE, win);
    MPI_Win_flush(dst, win);

    for (i = 0; i < nop; i++) {
        if (checkbuf[i] != locbuf[i]) {
            fprintf(stderr, "[%d] winbuf[%d] %.1lf != %.1lf\n", dst, i, checkbuf[i], locbuf[i]);
            errs++;
        }
    }

#ifdef OUTPUT_FAIL_DETAIL
    if (errs > 0) {
        fprintf(stderr, "[%d] locbuf:\n", rank);
        for (i = 0; i < nop; i++) {
            fprintf(stderr, "%.1lf ", locbuf[i]);
        }
        fprintf(stderr, "\n");

        fprintf(stderr, "[%d] winbuf:\n", rank);
        for (i = 0; i < nop; i++) {
            fprintf(stderr, "%.1lf ", checkbuf[i]);
        }
        fprintf(stderr, "\n");
    }
#endif

    return errs;
}

static int run_test1(int nop)
{
    int i, x, errs = 0;
    int dst;

    if (rank == 0) {
        dst = (rank + 1) % nprocs;

        fprintf(stdout, "[%d]-----check init_thread lock/acc(%d) & flush + "
                "%d*acc(%d) & flush/unlock \n", rank, dst, nop, dst);

        for (x = 0; x < ITER; x++) {
            change_data(nop, x);

            MPI_Win_lock(MPI_LOCK_EXCLUSIVE, dst, 0, win);

            /* enable load balancing */
            MPI_Accumulate(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, MPI_MAX, win);

            /* flush local so that I can change local buffers */
            MPI_Win_flush_local(dst, win);

            change_data(nop, x + ITER);

            for (i = 0; i < nop; i++) {
                MPI_Accumulate(&locbuf[i], 1, MPI_DOUBLE, dst, i, 1, MPI_DOUBLE, MPI_MAX, win);
            }

            /* still need flush before checking result on the target side */
            MPI_Win_flush(dst, win);

            errs += check_data(nop, x, dst);

            MPI_Win_unlock(dst, win);
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Bcast(&errs, 1, MPI_INT, 0, MPI_COMM_WORLD);

    return errs;
}

int main(int argc, char *argv[])
{
    int size = NUM_OPS;
    int i, errs = 0;
    int provided = 0;

    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided != MPI_THREAD_MULTIPLE) {
        printf("This test requires MPI_THREAD_MULTIPLE, but %d\n", provided);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (nprocs < 2) {
        fprintf(stderr, "Please run using at least 2 processes\n");
        goto exit;
    }

    /* size in byte */
    MPI_Win_allocate(sizeof(double), sizeof(double), MPI_INFO_NULL, MPI_COMM_WORLD, &winbuf, &win);

    reset_win();
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
    MPI_Finalize();

    return 0;
}

/*
 * no_conflict_epoch.c
 *  <FILE_DESC>
 *
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mpi.h>

#define NUM_OPS 5
#define CHECK
#define OUTPUT_FAIL_DETAIL

double *winbuf = NULL;
double *locbuf = NULL;
double *checkbuf = NULL;
int rank, nprocs;
MPI_Win win = MPI_WIN_NULL;
int ITER = 2;

static void change_data(int nop, int x)
{
    int dst, i;
    for (dst = 0; dst < nprocs; dst++) {
        for (i = 0; i < nop; i++) {
            locbuf[dst * nop + i] = 1.0 * (x + 1) * (i + 1) + nop * dst;
        }
    }
}

static int check_data_all(int nop, int x)
{
    int errs = 0;
    /* note that it is in an epoch */
    int dst, i;

    memset(checkbuf, 0, NUM_OPS * nprocs * sizeof(double));

    for (dst = 0; dst < nprocs; dst++) {
        MPI_Get(&checkbuf[dst * nop], nop, MPI_DOUBLE, dst, 0, nop, MPI_DOUBLE, win);
    }
    MPI_Win_flush_all(win);

    for (dst = 0; dst < nprocs; dst++) {
        for (i = 0; i < nop; i++) {
            if (checkbuf[dst * nop + i] != locbuf[dst * nop + i]) {
                fprintf(stderr, "[%d] winbuf[%d] %.1lf != %.1lf\n", dst, i,
                        checkbuf[dst * nop + i], locbuf[dst * nop + i]);
                errs++;
            }
        }
    }

#ifdef OUTPUT_FAIL_DETAIL
    if (errs > 0) {
        fprintf(stderr, "[%d] locbuf:\n", rank);
        for (i = 0; i < nop * nprocs; i++) {
            fprintf(stderr, "%.1lf ", locbuf[i]);
        }
        fprintf(stderr, "\n");

        fprintf(stderr, "[%d] winbuf:\n", rank);
        for (i = 0; i < nop * nprocs; i++) {
            fprintf(stderr, "%.1lf ", checkbuf[i]);
        }
        fprintf(stderr, "\n");
    }
#endif

    return errs;
}

int check_data(int nop, int x, int dst)
{
    int errs = 0;
    /* note that it is in an epoch */
    int i;

    memset(checkbuf, 0, NUM_OPS * nprocs * sizeof(double));

    MPI_Get(&checkbuf[dst * nop], nop, MPI_DOUBLE, dst, 0, nop, MPI_DOUBLE, win);
    MPI_Win_flush(dst, win);

    for (i = 0; i < nop; i++) {
        if (checkbuf[dst * nop + i] != locbuf[dst * nop + i]) {
            fprintf(stderr, "[%d] winbuf[%d] %.1lf != %.1lf\n", dst, i,
                    checkbuf[dst * nop + i], locbuf[dst * nop + i]);
            errs++;
        }
    }
#ifdef OUTPUT_FAIL_DETAIL
    if (errs > 0) {
        fprintf(stderr, "[%d] locbuf:\n", rank);
        for (i = 0; i < nop; i++) {
            fprintf(stderr, "%.1lf ", locbuf[dst * nop + i]);
        }
        fprintf(stderr, "\n");

        fprintf(stderr, "[%d] winbuf:\n", rank);
        for (i = 0; i < nop; i++) {
            fprintf(stderr, "%.1lf ", checkbuf[dst * nop + i]);
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
        fprintf(stdout, "[%d]-----check lock_all/put[0 - %d] & flush_all + "
                "%d * put[0 - %d] & flush_all/unlock_all\n", rank, nprocs, nop - 1, nprocs);

        for (x = 0; x < ITER; x++) {
            MPI_Win_lock_all(MPI_MODE_NOCHECK, win);

            /* change date in every interation */
            change_data(nop, x);

            for (dst = 0; dst < nprocs; dst++) {
                MPI_Put(&locbuf[dst * nop], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, win);
            }
            MPI_Win_flush_all(win);

            for (dst = 0; dst < nprocs; dst++) {
                for (i = 1; i < nop; i++) {
                    MPI_Put(&locbuf[dst * nop + i], 1, MPI_DOUBLE, dst, i, 1, MPI_DOUBLE, win);
#ifdef MVA
                    MPI_Win_flush(dst, win);    /* use it to poke progress in order to finish local CQEs */
#endif
                }
            }
            MPI_Win_flush_all(win);

            /* check in every iteration */
            errs += check_data_all(nop, x);

            MPI_Win_unlock_all(win);
        }
    }

    MPI_Bcast(&errs, 1, MPI_INT, 0, MPI_COMM_WORLD);

    return errs;
}

static int run_test2(int nop)
{
    int i, x, errs = 0;
    int dst;

    if (rank == 0) {
        fprintf(stdout, "[%d]-----check lock_all/%d * put[0 - %d] & flush_all/unlock_all\n",
                rank, nop, nprocs);

        for (x = 0; x < ITER; x++) {
            MPI_Win_lock_all(MPI_MODE_NOCHECK, win);

            /* change date in every interation */
            change_data(nop, x);

            for (dst = 0; dst < nprocs; dst++) {
                for (i = 0; i < nop; i++) {
                    MPI_Put(&locbuf[dst * nop + i], 1, MPI_DOUBLE, dst, i, 1, MPI_DOUBLE, win);
#ifdef MVA
                    MPI_Win_flush(dst, win);    /* use it to poke progress in order to finish local CQEs */
#endif
                }
            }
            MPI_Win_flush_all(win);

            /* check in every iteration */
            errs += check_data_all(nop, x);

            MPI_Win_unlock_all(win);
        }
    }

    MPI_Bcast(&errs, 1, MPI_INT, 0, MPI_COMM_WORLD);

    return errs;
}

/* following tests only run over more than 4 processes */
static int run_test3(int nop)
{
    int i, x, errs = 0, errs_total = 0;
    int dst_nprocs = nprocs / 2;
    int dst = rank + 1;

    if (nprocs < 4) {
        return errs;
    }

    if (rank == 0 || rank == dst_nprocs) {
        fprintf(stdout, "[%d]-----check lock(%d)/%d * put(%d) & flush/unlock(%d)\n",
                rank, dst, nop, dst, dst);

        for (x = 0; x < ITER; x++) {
            MPI_Win_lock(MPI_LOCK_EXCLUSIVE, dst, MPI_MODE_NOCHECK, win);

            /* change date in every iteration */
            change_data(nop, x);

            for (i = 0; i < nop; i++) {
                MPI_Put(&locbuf[dst * nop + i], 1, MPI_DOUBLE, dst, i, 1, MPI_DOUBLE, win);
            }

            MPI_Win_flush(dst, win);

            /* check in every iteration */
            errs += check_data(nop, x, dst);

            MPI_Win_unlock(dst, win);
        }
    }

    MPI_Allreduce(&errs, &errs_total, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    return errs_total;
}

static int run_test4(int nop)
{
    int i, x, errs = 0, errs_total = 0;
    int dst_nprocs = nprocs / 2;
    int dst = rank + 1;

    if (nprocs < 4) {
        return errs;
    }

    if (rank == 0 || rank == dst_nprocs) {
        fprintf(stdout,
                "[%d]-----check lock(%d)/put(%d) & flush + %d * put(%d) & flush/unlock(%d)\n", rank,
                dst, dst, nop - 1, dst, dst);

        for (x = 0; x < ITER; x++) {
            MPI_Win_lock(MPI_LOCK_EXCLUSIVE, dst, MPI_MODE_NOCHECK, win);

            /* change date in every iteration */
            change_data(nop, x);

            MPI_Put(&locbuf[dst * nop], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, win);
            MPI_Win_flush(dst, win);

            for (i = 1; i < nop; i++) {
                MPI_Put(&locbuf[dst * nop + i], 1, MPI_DOUBLE, dst, i, 1, MPI_DOUBLE, win);
            }
            MPI_Win_flush(dst, win);

            /* check in every iteration */
            errs += check_data(nop, x, dst);

            MPI_Win_unlock(dst, win);
        }
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
    checkbuf = calloc(NUM_OPS * nprocs, sizeof(double));
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

    MPI_Barrier(MPI_COMM_WORLD);
    errs = run_test2(size);
    if (errs)
        goto exit;

    MPI_Barrier(MPI_COMM_WORLD);
    errs = run_test3(size);
    if (errs)
        goto exit;

    MPI_Barrier(MPI_COMM_WORLD);
    errs = run_test4(size);
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
    if (checkbuf)
        free(checkbuf);

    MPI_Finalize();

    return 0;
}

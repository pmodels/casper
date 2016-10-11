/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>
#include "ctest.h"

/*
 * This test checks acc with pscw.
 */

#define NUM_OPS 2
#define TOTAL_NUM_OPS (NUM_OPS * 2)
#define CHECK
#define OUTPUT_FAIL_DETAIL

double *winbuf = NULL;
double *locbuf = NULL;
int rank, nprocs;
MPI_Win win = MPI_WIN_NULL;
int ITER = 2;
double max_result = 0.0;
double sum_result = 0.0;

static int run_test(int nop)
{
    int i, x, errs = 0, errs_total = 0;
    int dst, org;
    MPI_Group post_group = MPI_GROUP_NULL;
    MPI_Group start_group = MPI_GROUP_NULL;
    MPI_Group world_group = MPI_GROUP_NULL;
    int source = 0, skip = 0;

    MPI_Comm_group(MPI_COMM_WORLD, &world_group);

    /* check post/wait on even rank, start/NOP * acc/complete on odd rank. */
    if (rank % 2) {
        source = 1;
        dst = (rank + 1) % nprocs;
        MPI_Group_incl(world_group, 1, &dst, &start_group);
    }
    else {
        source = 0;
        org = (rank + nprocs - 1) % nprocs;
        MPI_Group_incl(world_group, 1, &org, &post_group);
    }

    /* if the number of processes is odd, no matching origin process for rank 0. */
    if (nprocs % 2 && rank == 0)
        skip = 1;

    for (x = 0; x < ITER; x++) {
        /* change date */
        for (i = 0; i < NUM_OPS * nprocs; i++) {
            locbuf[i] = x + i;
        }

        sum_result = 0.0;
        for (i = 0; i < nop; i++) {
            sum_result += locbuf[nop * rank + i];
        }
        max_result = locbuf[nop * rank + nop - 1];

        if (skip) {
            MPI_Barrier(MPI_COMM_WORLD);
        }
        else if (source) {
            MPI_Win_start(start_group, 0, win);

            for (i = 0; i < nop; i++) {
                MPI_Accumulate(&locbuf[i + dst * nop], 1, MPI_DOUBLE, dst, 0, 1,
                               MPI_DOUBLE, MPI_SUM, win);
                MPI_Accumulate(&locbuf[i + dst * nop], 1, MPI_DOUBLE, dst, 1, 1,
                               MPI_DOUBLE, MPI_MAX, win);
            }

            MPI_Win_complete(win);
            MPI_Barrier(MPI_COMM_WORLD);        /* will not start next epoch before target finish its check */
        }
        else {
            MPI_Win_post(post_group, 0, win);
            MPI_Win_wait(win);

            /* source may start next start before this lock */

            /* check result */
            MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win);
            if (CTEST_double_diff(winbuf[0], sum_result)) {
                fprintf(stderr, "[%d]winbuf[%d] %.1lf != %.1lf, iter %d\n", rank, 0,
                        winbuf[0], sum_result, x);
                errs++;
            }
            if (CTEST_double_diff(winbuf[1], max_result)) {
                fprintf(stderr, "[%d]winbuf[%d] %.1lf != %.1lf, iter %d\n", rank, 1, winbuf[1],
                        max_result, x);
                errs++;
            }

            if (errs > 0) {
                fprintf(stderr, "winbuf: %.1lf %.1lf\n", winbuf[0], winbuf[1]);
            }

            /* reset my window */
            for (i = 0; i < 2; i++) {
                winbuf[i] = 0.0;
            }
            MPI_Win_unlock(rank, win);

            MPI_Barrier(MPI_COMM_WORLD);
        }
    }

    MPI_Allreduce(&errs, &errs_total, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    if (post_group != MPI_GROUP_NULL)
        MPI_Group_free(&post_group);
    if (start_group != MPI_GROUP_NULL)
        MPI_Group_free(&start_group);
    if (world_group != MPI_GROUP_NULL)
        MPI_Group_free(&world_group);

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

    /* size in byte */
    MPI_Win_allocate(sizeof(double) * 2, sizeof(double), MPI_INFO_NULL,
                     MPI_COMM_WORLD, &winbuf, &win);

    /*
     * P0: SUM{0:NOPS-1}, MAX{0:NOPS-1}
     * P1: SUM{NOPS:2NOPS-1}, MAX{NOPS:2NOPS-1}
     * ...
     */

    /* reset window */
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win);
    for (i = 0; i < 2; i++) {
        winbuf[i] = 0.0;
    }
    MPI_Win_unlock(rank, win);

    MPI_Barrier(MPI_COMM_WORLD);
    errs = run_test(size);

  exit:
    if (rank == 0)
        CTEST_report_result(errs);

    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    if (locbuf)
        free(locbuf);

    MPI_Finalize();

    return 0;
}

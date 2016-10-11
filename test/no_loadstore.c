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
 * This test checks window with no_local_load_store info.
 */

#define CHECK

double *winbuf = NULL;
double locbuf[1];
int rank, nprocs;
MPI_Win win = MPI_WIN_NULL;

static int run_test()
{
    int errs = 0, errs_total = 0;
    int dst;
    double buf = 0;

    /* Check Lock_all */
    if (rank == 0) {
        locbuf[0] = 1.0;
        MPI_Win_lock_all(0, win);
        for (dst = 0; dst < nprocs; dst++) {
            MPI_Put(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, win);
        }
        MPI_Win_flush_all(win);
        MPI_Win_unlock_all(win);
    }

    MPI_Barrier(MPI_COMM_WORLD);        /* rank 1 may get lock before rank 0 */
    MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win);
    buf = 0;
    MPI_Get(&buf, 1, MPI_DOUBLE, rank, 0, 1, MPI_DOUBLE, win);
    MPI_Win_unlock(rank, win);
    if (CTEST_precise_double_diff(buf, 1.0)) {
        fprintf(stderr, "[%d] lock_all: winbuf %.1lf != %.1lf\n", rank, buf, 1.0);
        errs++;
    }
    MPI_Barrier(MPI_COMM_WORLD);

    /* Check Lock */
    if (rank == 0) {
        locbuf[0] = 2.0;
        dst = 1;

        MPI_Win_lock(MPI_LOCK_SHARED, dst, 0, win);
        MPI_Put(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, win);
        MPI_Win_flush(dst, win);
        MPI_Win_unlock(dst, win);
    }

    MPI_Barrier(MPI_COMM_WORLD);        /* rank 1 may get lock before rank 0 */
    if (rank == 1) {
        MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win);
        buf = 0;
        MPI_Get(&buf, 1, MPI_DOUBLE, rank, 0, 1, MPI_DOUBLE, win);
        MPI_Win_unlock(rank, win);
        if (CTEST_precise_double_diff(buf, 2.0)) {
            fprintf(stderr, "[%d] lock(1): winbuf %.1lf != %.1lf\n", rank, buf, 2.0);
            errs++;
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);

    /* Check Lock(self-target) */
    if (rank == 0) {
        locbuf[0] = 3.0;
        dst = 0;

        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, dst, 0, win);
        MPI_Put(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, win);
        MPI_Win_flush(dst, win);
        MPI_Win_unlock(dst, win);
    }

    if (rank == 0) {
        buf = 0;

        /* It is wrong to load/store local winbuf with no_local_load_store,
         * also mpich does not wait for target completion in exclusive lock*/
        MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win);
        MPI_Get(&buf, 1, MPI_DOUBLE, rank, 0, 1, MPI_DOUBLE, win);
        MPI_Win_unlock(rank, win);
        if (CTEST_precise_double_diff(buf, 3.0)) {
            fprintf(stderr, "[%d] lock(0): winbuf %.1lf != %.1lf\n", rank, buf, 3.0);
            errs++;
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Allreduce(&errs, &errs_total, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    return errs_total;
}

int main(int argc, char *argv[])
{
    int errs = 0;
    MPI_Init(&argc, &argv);
    MPI_Info win_info = MPI_INFO_NULL;

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (nprocs < 2) {
        fprintf(stderr, "Please run using at least 2 processes\n");
        goto exit;
    }

    MPI_Info_create(&win_info);
    MPI_Info_set(win_info, (char *) "no_local_load_store", (char *) "true");

    locbuf[0] = (rank + 1) * 1.0;

    /* size in byte */
    MPI_Win_allocate(sizeof(double), sizeof(double), win_info, MPI_COMM_WORLD, &winbuf, &win);

    /* reset window */
    MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win);
    winbuf[0] = 0.0;
    MPI_Win_unlock(rank, win);

    MPI_Barrier(MPI_COMM_WORLD);
    errs = run_test();

  exit:
    if (rank == 0)
        CTEST_report_result(errs);

    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    MPI_Info_free(&win_info);

    MPI_Finalize();

    return 0;
}

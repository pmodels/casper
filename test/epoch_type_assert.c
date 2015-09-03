/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mpi.h>

/*
 * This test checks lock with "lockall|fence" epochs_used info.
 * It expects to report assert error.
 */

#define NUM_OPS 5
#define CHECK
#define OUTPUT_FAIL_DETAIL

double *winbuf = NULL;
double *locbuf = NULL;
double *checkbuf = NULL;
int rank, nprocs;
MPI_Win win = MPI_WIN_NULL;
int ITER = 2;

static int run_test1()
{
    int errs = 0;
    int dst;
    MPI_Info win_info = MPI_INFO_NULL;

    MPI_Info_create(&win_info);
    MPI_Info_set(win_info, (char *) "epochs_used", (char *) "lockall|fence");

    /* size in byte */
    MPI_Win_allocate(sizeof(double) * NUM_OPS, sizeof(double), win_info,
                     MPI_COMM_WORLD, &winbuf, &win);

    if (rank == 0) {
        dst = (rank + 1) % nprocs;

        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, dst, 0, win);
        /* expect assert error */
        MPI_Win_unlock(dst, win);
    }

    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    if (win_info != MPI_INFO_NULL)
        MPI_Info_free(&win_info);
    return errs;
}

int main(int argc, char *argv[])
{
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

    MPI_Barrier(MPI_COMM_WORLD);
    errs = run_test1();
    if (errs)
        goto exit;

  exit:
    if (locbuf)
        free(locbuf);
    if (checkbuf)
        free(checkbuf);

    MPI_Finalize();

    return 0;
}

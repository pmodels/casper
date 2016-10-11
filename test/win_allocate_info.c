/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>
#include "ctest.h"

/*
 *  This test checks win_allocate with predefined info hints.
 */

#define ITER 50
int rank, nprocs;
int size = 16;

/* check win_allocate with alloc_shm info. */
static int run_test1()
{
    int err = 0;
    MPI_Info info = MPI_INFO_NULL;
    double *buf1 = NULL, *buf2 = NULL;
    MPI_Win win1 = MPI_WIN_NULL, win2 = MPI_WIN_NULL;

    MPI_Info_create(&info);
    MPI_Info_set(info, "alloc_shm", "true");

    MPI_Win_allocate(sizeof(double) * size, sizeof(double), info, MPI_COMM_WORLD, &buf1, &win1);

    if (buf1 == NULL) {
        fprintf(stderr, "MPI_Win_allocate(alloc_shm=true) failed: " "winbuf is NULL\n");
    }
    if (win1 == MPI_WIN_NULL) {
        fprintf(stderr, "MPI_Win_allocate(alloc_shm=true) failed: " "win is MPI_WIN_NULL\n");
    }

    MPI_Info_set(info, "alloc_shm", "false");

    MPI_Win_allocate(sizeof(double) * size, sizeof(double), info, MPI_COMM_WORLD, &buf2, &win2);

    if (buf2 == NULL) {
        fprintf(stderr, "MPI_Win_allocate(alloc_shm=false) failed: " "winbuf is NULL\n");
        err = 1;
    }
    if (win2 == MPI_WIN_NULL) {
        fprintf(stderr, "MPI_Win_allocate(alloc_shm=false) failed: " "win is MPI_WIN_NULL\n");
        err = 1;
    }

    MPI_Win_free(&win1);
    MPI_Win_free(&win2);

    MPI_Info_free(&info);

    return err;
}

/* check win_allocate with same_size info. */
static int run_test2()
{
    int err = 0;
    MPI_Info info = MPI_INFO_NULL;
    double *buf1 = NULL, *buf2 = NULL;
    MPI_Win win1 = MPI_WIN_NULL, win2 = MPI_WIN_NULL;

    MPI_Info_create(&info);
    MPI_Info_set(info, "same_size", "true");

    MPI_Win_allocate(sizeof(double) * size, sizeof(double), info, MPI_COMM_WORLD, &buf1, &win1);

    if (buf1 == NULL) {
        fprintf(stderr, "MPI_Win_allocate(same_size=true) failed: " "winbuf is NULL\n");
    }
    if (win1 == MPI_WIN_NULL) {
        fprintf(stderr, "MPI_Win_allocate(same_size=true) failed: " "win is MPI_WIN_NULL\n");
    }

    MPI_Info_set(info, "same_size", "false");

    MPI_Win_allocate(sizeof(double) * size, sizeof(double), info, MPI_COMM_WORLD, &buf2, &win2);

    if (buf2 == NULL) {
        fprintf(stderr, "MPI_Win_allocate(same_size=false) failed: " "winbuf is NULL\n");
        err = 1;
    }
    if (win2 == MPI_WIN_NULL) {
        fprintf(stderr, "MPI_Win_allocate(same_size=false) failed: " "win is MPI_WIN_NULL\n");
        err = 1;
    }

    MPI_Win_free(&win1);
    MPI_Win_free(&win2);

    MPI_Info_free(&info);

    return err;
}

/* check win_allocate with alloc_shared_noncontig info. */
static int run_test3()
{
    int err = 0;
    MPI_Info info = MPI_INFO_NULL;
    double *buf1 = NULL, *buf2 = NULL;
    MPI_Win win1 = MPI_WIN_NULL, win2 = MPI_WIN_NULL;

    MPI_Info_create(&info);
    MPI_Info_set(info, "alloc_shared_noncontig", "true");

    MPI_Win_allocate(sizeof(double) * size, sizeof(double), info, MPI_COMM_WORLD, &buf1, &win1);

    if (buf1 == NULL) {
        fprintf(stderr, "MPI_Win_allocate(alloc_shared_noncontig=true) failed: "
                "winbuf is NULL\n");
        err = 1;
    }
    if (win1 == MPI_WIN_NULL) {
        fprintf(stderr, "MPI_Win_allocate(alloc_shared_noncontig=true) failed: "
                "win is MPI_WIN_NULL\n");
        err = 1;
    }

    MPI_Info_set(info, "alloc_shared_noncontig", "false");

    MPI_Win_allocate(sizeof(double) * size, sizeof(double), info, MPI_COMM_WORLD, &buf2, &win2);

    if (buf2 == NULL) {
        fprintf(stderr, "MPI_Win_allocate(alloc_shared_noncontig=false) failed: "
                "winbuf is NULL\n");
    }
    if (win2 == MPI_WIN_NULL) {
        fprintf(stderr, "MPI_Win_allocate(alloc_shared_noncontig=false) failed: "
                "win is MPI_WIN_NULL\n");
    }

    MPI_Win_free(&win1);
    MPI_Win_free(&win2);

    MPI_Info_free(&info);

    return err;
}

int main(int argc, char *argv[])
{
    int errs = 0;
    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (nprocs < 2) {
        fprintf(stderr, "Please run using at least 2 processes\n");
        goto exit;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    errs += run_test1();

    MPI_Barrier(MPI_COMM_WORLD);
    errs += run_test2();

    MPI_Barrier(MPI_COMM_WORLD);
    errs += run_test3();

  exit:
    if (rank == 0)
        CTEST_report_result(errs);

    MPI_Finalize();

    return 0;
}

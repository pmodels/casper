/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
int ITER = 100000;

/* This benchmark measures the bandwidth of lock-put-unlock on
 * win_allocate_shared window. */

int main(int argc, char *argv[])
{
    int rank, nprocs, shm_nprocs;
    MPI_Win win;
    int dst;
    char *winbuf = NULL;
    double t0, t, avgt;
    int size = 16, i, x;
    char *sbuf = NULL;
    MPI_Comm shm_comm = MPI_COMM_NULL;

    if (argc > 1) {
        size = atoi(argv[1]);
    }

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &shm_comm);
    MPI_Comm_size(shm_comm, &shm_nprocs);

    if (nprocs % 2 || shm_nprocs != shm_nprocs) {
        if (rank == 0) {
            fprintf(stderr, "Requires N pair of local processes, %d/shm %d\n", nprocs, shm_nprocs);
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    sbuf = calloc(size, sizeof(char));

    MPI_Win_allocate_shared(size, sizeof(char), MPI_INFO_NULL, MPI_COMM_WORLD, &winbuf, &win);
    MPI_Barrier(MPI_COMM_WORLD);
    dst = (rank + 1) % nprocs;

    t0 = MPI_Wtime();
    for (x = 0; x < ITER; x++) {
        MPI_Win_lock(MPI_LOCK_SHARED, dst, 0, win);

        if (rank % 2 == 0) {
            MPI_Put(sbuf, size, MPI_CHAR, dst, 0, size, MPI_CHAR, win);
        }

        MPI_Win_unlock(dst, win);
    }

    t = MPI_Wtime() - t0;

    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Reduce(&t, &avgt, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    avgt = avgt / nprocs * 2 * 1000 * 1000 / ITER;      /* us */

    if (rank == 0) {
        printf("np %d size %d time %.2f\n", nprocs, size, avgt);
    }

    MPI_Win_free(&win);
    if (shm_comm)
        MPI_Comm_free(&shm_comm);
    free(sbuf);

    MPI_Finalize();

    return 0;

}

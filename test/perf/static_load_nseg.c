/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>

/* This benchmark measures the load balancing in static segment-binding mode
 * with uneven window size.*/

/* #define DEBUG */
#define CHECK

#define ITER_S 10000
#define ITER_L 5000
#define ITER_LL 500
#define ITER_LLL 200

#define SKIP 1
#define NPROCS_M 16

double *winbuf = NULL;
double locbuf[1];
int rank, nprocs;
int shm_rank = 0;
MPI_Win win = MPI_WIN_NULL;
int ITER = ITER_S;
#ifdef ENABLE_CSP
#include <casper.h>
int CSP_NUM_G = 1;
#endif

int NOP_MAX = 1, NOP_MIN = 1, NOP = 1, NOP_ITER = 2;    /* us */
unsigned long SLEEP_TIME = 100;
int *target_shm_ranks = NULL;

static int target_computation()
{
    double start = MPI_Wtime() * 1000 * 1000;
    while (MPI_Wtime() * 1000 * 1000 - start < SLEEP_TIME);
    return 0;
}

static int run_test()
{
    int i, x, errs = 0;
    int dst;
    double t0, avg_total_time = 0.0, t_total = 0.0;

    if (nprocs <= NPROCS_M) {
        ITER = ITER_S;
    }
    else if (nprocs > NPROCS_M && SLEEP_TIME < 50) {
        ITER = ITER_L;
    }
    else {
        ITER = ITER_LL;
    }

    for (x = 0; x < SKIP; x++) {
        MPI_Win_lock_all(0, win);
        for (dst = 0; dst < nprocs; dst++) {
            MPI_Put(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, win);
        }
        MPI_Win_unlock_all(win);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    t0 = MPI_Wtime();
    for (x = 0; x < ITER; x++) {
        MPI_Win_lock_all(0, win);

        for (dst = 0; dst < nprocs; dst++) {
            /* local rank 0 receives more operations */
            if (target_shm_ranks[dst] == 0) {
                for (i = 0; i < NOP; i++) {
                    int idx = NOP_MAX / NOP * i;
                    MPI_Put(&locbuf[0], 1, MPI_DOUBLE, dst, idx, 1, MPI_DOUBLE, win);
                }
            }
            else {
                for (i = 0; i < NOP_MIN; i++) {
                    MPI_Put(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, win);
                }
            }
        }

        target_computation();

        MPI_Win_unlock_all(win);
    }

    t_total = (MPI_Wtime() - t0) * 1000 * 1000; /*us */
    t_total /= ITER;

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Reduce(&t_total, &avg_total_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        avg_total_time = avg_total_time / nprocs;       /* us */

#ifdef ENABLE_CSP
        const char *lock_mtd = getenv("CSP_LOCK_METHOD");
        fprintf(stdout,
                "casper-%s: iter %d comp_size %lu num_op %d %d nprocs %d nh %d total_time %.2lf\n",
                lock_mtd, ITER, SLEEP_TIME, NOP_MIN, NOP, nprocs, CSP_NUM_G, avg_total_time);
#else
        fprintf(stdout, "orig: iter %d comp_size %lu num_op %d %d nprocs %d total_time %.2lf\n",
                ITER, SLEEP_TIME, NOP_MIN, NOP, nprocs, avg_total_time);
#endif
    }

    return errs;
}

int main(int argc, char *argv[])
{
    int win_size = 0;
    MPI_Comm shm_comm = MPI_COMM_NULL;
    MPI_Info win_info = MPI_INFO_NULL;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &shm_comm);
    MPI_Comm_rank(shm_comm, &shm_rank);
#ifdef ENABLE_CSP
    CSP_ghost_size(&CSP_NUM_G);
#endif

    if (nprocs < 2) {
        fprintf(stderr, "Please run using at least 2 processes\n");
        goto exit;
    }

    if (argc >= 4) {
        NOP_MIN = atoi(argv[1]);
        NOP_MAX = atoi(argv[2]);
        NOP_ITER = atoi(argv[3]);
    }
    if (argc >= 5) {
        SLEEP_TIME = atoi(argv[4]);
    }

    target_shm_ranks = calloc(nprocs, sizeof(int));

    target_shm_ranks[rank] = shm_rank;
    MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, target_shm_ranks, 1, MPI_INT, MPI_COMM_WORLD);

    if (shm_rank == 0) {
        win_size = NOP_MAX;
    }
    else {
        win_size = 2;
    }

    locbuf[0] = (rank + 1) * 1.0;

    MPI_Info_create(&win_info);
    MPI_Info_set(win_info, (char *) "epoch_type", (char *) "lockall");

    MPI_Win_allocate(win_size * sizeof(double), sizeof(double), win_info, MPI_COMM_WORLD,
                     &winbuf, &win);

    for (NOP = NOP_MIN; NOP <= NOP_MAX; NOP *= NOP_ITER) {
        run_test();
    }

  exit:
    if (win_info != MPI_INFO_NULL)
        MPI_Info_free(&win_info);
    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    if (target_shm_ranks)
        free(target_shm_ranks);
    if (shm_comm)
        MPI_Comm_free(&shm_comm);

    MPI_Finalize();

    return 0;
}

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>

/* This benchmark evaluates asynchronous progress with fence. Rank 0 performs
 * fence-accumulate-fence, and all the other processes perform
 * fence-compute-fence.*/

#define D_SLEEP_TIME 100        // 100us

//#define DEBUG
//#define CHECK
#define ITER_S 10000
#define ITER_M 5000
#define ITER_L 1000
#define NPROCS_M 6

#ifdef DEBUG
#define debug_printf(str,...) {fprintf(stdout, str, ## __VA_ARGS__);fflush(stdout);}
#else
#define debug_printf(str,...) {}
#endif

#ifdef ENABLE_CSP
#include <casper.h>
int CSP_NUM_G = 1;
#endif

double *winbuf = NULL;
double *locbuf = NULL;
int rank, nprocs, nprocs_local;
MPI_Win win = MPI_WIN_NULL;
int ITER = ITER_S;
int NOP = 100;

static int usleep_by_count(unsigned long us)
{
    double start = MPI_Wtime() * 1000 * 1000;
    while (MPI_Wtime() * 1000 * 1000 - start < (double) us);
    return 0;
}

static int run_test(int time)
{
    int i, x, errs_total = 0;
    int dst;
    double t0, avg_total_time = 0.0, t_total = 0.0;

    if (nprocs < NPROCS_M) {
        ITER = ITER_S;
    }
    else if (nprocs >= NPROCS_M && nprocs < NPROCS_M * 2) {
        ITER = ITER_M;
    }
    else {
        ITER = ITER_L;
    }

    t0 = MPI_Wtime();
    if (rank == 0) {
        for (x = 0; x < ITER; x++) {
            MPI_Win_fence(MPI_MODE_NOPRECEDE, win);

            for (dst = 0; dst < nprocs; dst++) {
                for (i = 1; i < NOP; i++) {
                    MPI_Accumulate(&locbuf[i], 1, MPI_DOUBLE, dst, rank, 1, MPI_DOUBLE, MPI_SUM,
                                   win);
                }
            }

            MPI_Win_fence(MPI_MODE_NOSUCCEED, win);
        }
    }
    else {
        for (x = 0; x < ITER; x++) {
            MPI_Win_fence(MPI_MODE_NOPRECEDE, win);

            if (time > 0)
                usleep_by_count(time);

            MPI_Win_fence(MPI_MODE_NOSUCCEED, win);
        }
    }
    t_total = MPI_Wtime() - t0;
    t_total /= ITER;

    if (rank == 0) {
        avg_total_time = t_total / nprocs * 1000 * 1000;
#ifdef ENABLE_CSP
        fprintf(stdout,
                "casper: iter %d comp_size %d num_op %d nprocs %d nh %d total_time %.2lf\n",
                ITER, time, NOP, nprocs, CSP_NUM_G, avg_total_time);
#else
        fprintf(stdout,
                "orig: iter %d comp_size %d num_op %d nprocs %d total_time %.2lf\n",
                ITER, time, NOP, nprocs, avg_total_time);
#endif
    }

    return errs_total;
}

int main(int argc, char *argv[])
{
    int i, errs;
    int min_time = D_SLEEP_TIME, max_time = D_SLEEP_TIME, iter_time = 2, time;
    MPI_Info win_info = MPI_INFO_NULL;

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#ifdef ENABLE_CSP
    CSP_ghost_size(&CSP_NUM_G);
#endif

    debug_printf("[%d]init done, %d/%d\n", rank, rank, nprocs);

    if (nprocs < 2) {
        fprintf(stderr, "Please run using at least 2 processes\n");
        goto exit;
    }

    if (argc >= 4) {
        min_time = atoi(argv[1]);
        max_time = atoi(argv[2]);
        iter_time = atoi(argv[3]);
    }
    if (argc >= 5) {
        NOP = atoi(argv[4]);
    }

    locbuf = malloc(sizeof(double) * NOP);
    for (i = 0; i < NOP; i++) {
        locbuf[i] = 1.0;
    }

    MPI_Info_create(&win_info);
    MPI_Info_set(win_info, (char *) "epochs_used", (char *) "fence");

    // size in byte
    MPI_Win_allocate(sizeof(double) * nprocs, sizeof(double), win_info,
                     MPI_COMM_WORLD, &winbuf, &win);
    debug_printf("[%d]win_allocate done\n", rank);

    for (time = min_time; time <= max_time; time *= iter_time) {
        /* reset window */
        MPI_Win_fence(MPI_MODE_NOPRECEDE, win);
        for (i = 0; i < nprocs; i++) {
            winbuf[i] = 0.0;
        }
        MPI_Win_fence(MPI_MODE_NOSUCCEED, win);

        MPI_Barrier(MPI_COMM_WORLD);

        errs = run_test(time);
        if (errs > 0)
            break;

        if (time == 0)
            break;
    }

  exit:
    if (win_info != MPI_INFO_NULL)
        MPI_Info_free(&win_info);
    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    if (locbuf)
        free(locbuf);

    MPI_Finalize();

    return 0;
}

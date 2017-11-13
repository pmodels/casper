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

/* This benchmark evaluates asynchronous progress in lockall epoch.
 * Every process performs lockall-RMA-compute-RMA-unlockall.*/

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
const char *OP_TYPE_NM[3] = { "ACC", "PUT", "GET" };

enum {
    OP_ACC,
    OP_PUT,
    OP_GET
};
int OP_TYPE = OP_ACC;

static int usleep_by_count(unsigned long us)
{
    double start = MPI_Wtime() * 1000 * 1000;
    while (MPI_Wtime() * 1000 * 1000 - start < (double) us);
    return 0;
}

static void DO_OP_LOOP(int time, int iter)
{
    int i, x, dst;

    switch (OP_TYPE) {
    case OP_ACC:
        for (x = 0; x < iter; x++) {
            MPI_Win_lock_all(0, win);

            for (dst = 0; dst < nprocs; dst++) {
                MPI_Accumulate(&locbuf[0], 1, MPI_DOUBLE, dst, rank, 1, MPI_DOUBLE, MPI_SUM, win);
            }
            MPI_Win_flush_all(win);

            usleep_by_count(time);

            for (dst = 0; dst < nprocs; dst++) {
                for (i = 0; i < NOP; i++) {
                    MPI_Accumulate(&locbuf[i], 1, MPI_DOUBLE, dst, rank, 1, MPI_DOUBLE, MPI_SUM,
                                   win);
                }
            }
            MPI_Win_unlock_all(win);
        }
        break;
    case OP_PUT:
        for (x = 0; x < iter; x++) {
            MPI_Win_lock_all(0, win);
            for (dst = 0; dst < nprocs; dst++) {
                MPI_Put(&locbuf[0], 1, MPI_DOUBLE, dst, rank, 1, MPI_DOUBLE, win);
            }
            MPI_Win_flush_all(win);

            usleep_by_count(time);

            for (dst = 0; dst < nprocs; dst++) {
                for (i = 0; i < NOP; i++) {
                    MPI_Put(&locbuf[i], 1, MPI_DOUBLE, dst, rank, 1, MPI_DOUBLE, win);
                }
            }
            usleep_by_count(time);
            MPI_Win_unlock_all(win);
        }
        break;
    case OP_GET:
        for (x = 0; x < iter; x++) {
            MPI_Win_lock_all(0, win);
            for (dst = 0; dst < nprocs; dst++) {
                MPI_Get(&locbuf[0], 1, MPI_DOUBLE, dst, rank, 1, MPI_DOUBLE, win);
            }
            MPI_Win_flush_all(win);

            usleep_by_count(time);

            for (dst = 0; dst < nprocs; dst++) {
                for (i = 0; i < NOP; i++) {
                    MPI_Get(&locbuf[i], 1, MPI_DOUBLE, dst, rank, 1, MPI_DOUBLE, win);
                }
            }
            usleep_by_count(time);
            MPI_Win_unlock_all(win);
        }
        break;
    }
}

static int run_test(int time)
{
    int errs_total = 0;
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

    DO_OP_LOOP(time, ITER);

    t_total = MPI_Wtime() - t0;
    t_total /= ITER;

    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Reduce(&t_total, &avg_total_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        avg_total_time = avg_total_time / nprocs * 1000 * 1000;
#ifdef ENABLE_CSP
        fprintf(stdout,
                "casper: %s iter %d comp_size %d num_op %d nprocs %d nh %d total_time %.2lf\n",
                OP_TYPE_NM[OP_TYPE], ITER, time, NOP, nprocs, CSP_NUM_G, avg_total_time);
#else
        const char *async_th = getenv("MPIR_CVAR_ASYNC_PROGRESS");
        int async_th_val = 0;
        if (async_th && strlen(async_th)) {
            async_th_val = atoi(async_th);
        }
        fprintf(stdout, "orig%s: %s iter %d comp_size %d num_op %d nprocs %d total_time %.2lf\n",
                ((async_th_val == 1) ? "-th" : ""), OP_TYPE_NM[OP_TYPE], ITER, time, NOP, nprocs,
                avg_total_time);
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
    if (argc >= 6) {
        OP_TYPE = atoi(argv[5]);
    }

    if ((OP_TYPE != OP_ACC) && (OP_TYPE != OP_PUT) && (OP_TYPE != OP_GET)) {
        if (rank == 0)
            fprintf(stderr, "Wrong op type %d\n", OP_TYPE);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    locbuf = malloc(sizeof(double) * NOP);
    for (i = 0; i < NOP; i++) {
        locbuf[i] = 1.0;
    }

    MPI_Info_create(&win_info);
    MPI_Info_set(win_info, (char *) "epochs_used", (char *) "lockall");

    // size in byte
    MPI_Win_allocate(sizeof(double) * nprocs, sizeof(double), win_info,
                     MPI_COMM_WORLD, &winbuf, &win);
    debug_printf("[%d]win_allocate done\n", rank);

    for (time = min_time; time <= max_time; time *= iter_time) {
        /* reset window */
        MPI_Win_lock_all(0, win);
        for (i = 0; i < nprocs; i++) {
            winbuf[i] = 0.0;
        }
        MPI_Win_unlock_all(win);

        MPI_Barrier(MPI_COMM_WORLD);

        errs = run_test(time);
        if (errs > 0)
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

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>


#define D_SLEEP_TIME 100        // 100us

//#define DEBUG
//#define CHECK
#define ITER_S 100000
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
    int dst, org;
    double t0, avg_total_time = 0.0, t_total = 0.0;
    MPI_Group post_group = MPI_GROUP_NULL;
    MPI_Group start_group = MPI_GROUP_NULL;
    MPI_Group world_group = MPI_GROUP_NULL;
    int source = 0;

    MPI_Comm_group(MPI_COMM_WORLD, &world_group);

    if (rank == 0) {
        source = 1;
        dst = 1;
        MPI_Group_incl(world_group, 1, &dst, &start_group);
//        printf("%d start(%d); nop=%d, time=%d\n", rank, dst, NOP, time);
    }
    else {
        source = 0;
        org = 0;
        MPI_Group_incl(world_group, 1, &org, &post_group);
//        printf("%d post(%d); nop=%d, time=%d\n", rank, org, NOP, time);
    }

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

    for (x = 0; x < ITER; x++) {
        if (source) {
            MPI_Win_start(start_group, 0, win);

            for (i = 0; i < NOP; i++) {
                MPI_Accumulate(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, MPI_SUM, win);
            }

            MPI_Win_complete(win);
        }
        else {
            MPI_Win_post(post_group, 0, win);

            if (time > 0)
                usleep_by_count(time);

            MPI_Win_wait(win);
        }
    }

    t_total = MPI_Wtime() - t0;
    t_total /= ITER;

    if (post_group != MPI_GROUP_NULL)
        MPI_Group_free(&post_group);
    if (start_group != MPI_GROUP_NULL)
        MPI_Group_free(&start_group);
    if (world_group != MPI_GROUP_NULL)
        MPI_Group_free(&world_group);

    if (rank == 0) {
        avg_total_time = t_total * 1000 * 1000;
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

    // size in byte
    MPI_Win_allocate(sizeof(double) * nprocs, sizeof(double), MPI_INFO_NULL,
                     MPI_COMM_WORLD, &winbuf, &win);
    debug_printf("[%d]win_allocate done\n", rank);

    for (time = min_time; time <= max_time; time *= iter_time) {
        /* reset window */
        MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win);
        for (i = 0; i < nprocs; i++) {
            winbuf[i] = 0.0;
        }
        MPI_Win_unlock(rank, win);

        MPI_Barrier(MPI_COMM_WORLD);

        errs = run_test(time);
        if (errs > 0)
            break;

        if (time == 0)
            break;
    }

  exit:

    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    if (locbuf)
        free(locbuf);

    MPI_Finalize();

    return 0;
}

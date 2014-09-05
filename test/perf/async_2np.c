/*
 * benchmark-asp-win-test-2pe.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>

#define SIZE 4
#define SLEEP_TIME 100        //us

//#define DEBUG
#define CHECK
#define ITER 10000

#ifdef DEBUG
#define debug_printf(str...) {fprintf(stdout, str);fflush(stdout);}
#else
#define debug_printf(str...) {}
#endif

#ifdef MTCORE
extern int MTCORE_NUM_H;
#endif

MPI_Win win;
double *winbuf, locbuf[SIZE];
int rank, nprocs;
int NOP = 1;

static void usleep_by_count(unsigned long us)
{
    double start = MPI_Wtime() * 1000 * 1000;
    while (MPI_Wtime() * 1000 * 1000 - start < us);
    return;
}

static int run_test(int time)
{
    int i, x, errs = 0;
    MPI_Status stat;
    int dst, src;
    int winbuf_offset = 0;
    double local_param, sum = 0.0;
    double t0, avg_total_time = 0.0, t_total = 0.0;
    MPI_Request request;
    MPI_Status status;
    int buf[1];
    int flag = 0;

    if (rank == 0) {
        dst = 1;
        buf[0] = 99;
        MPI_Win_lock_all(0, win);
    }
    else {
        src = 0;
        buf[0] = 0;
        MPI_Irecv(buf, 1, MPI_INT, src, 0, MPI_COMM_WORLD, &request);
    }

    t0 = MPI_Wtime();
    for (x = 0; x < ITER; x++) {

        // rank 0 does RMA communication
        if (rank == 0) {
            for (i = 0; i < NOP; i++)
                MPI_Accumulate(&locbuf[0], 1, MPI_DOUBLE, dst, 0, 1, MPI_DOUBLE, MPI_SUM, win);
            MPI_Win_flush_all(win);
        }
        // rank 1 does sleep and test
        else {
            usleep_by_count(time);
            MPI_Test(&request, &flag, &status);
        }
    }

    t_total += MPI_Wtime() - t0;
    t_total /= ITER;

    if (rank == 0) {
        MPI_Win_unlock_all(win);
        MPI_Send(buf, 1, MPI_INT, dst, 0, MPI_COMM_WORLD);
    }
    else {
        if (!flag)
            MPI_Wait(&request, &status);
        if (buf[0] != 99) {
            fprintf(stderr, "[%d]error: recv data %d != %d\n", rank, buf[0], 99);
            return errs;
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
#ifdef MTCORE
        fprintf(stdout,
                "mtcore: comp_size %d num_op %d nprocs %d total_time %.2lf\n",
                time, NOP, nprocs, t_total * 1000 * 1000);
#else
        fprintf(stdout,
                "orig: comp_size %d num_op %d nprocs %d total_time %.2lf\n",
                time, NOP, nprocs, t_total * 1000 * 1000);
#endif
    }

    return errs;
}

int main(int argc, char *argv[])
{
    int size;
    int i;
    int min_time = SLEEP_TIME, max_time = SLEEP_TIME, iter_time = 2, time;

    MPI_Init(&argc, &argv);
    debug_printf("[%d]init done\n", rank);


#ifdef MTCORE
    /* first argv is nh */
    if (argc >= 5) {
        min_time = atoi(argv[2]);
        max_time = atoi(argv[3]);
        iter_time = atoi(argv[4]);
    }
    if (argc >= 6) {
        NOP = atoi(argv[5]);
    }
#else
    if (argc >= 4) {
        min_time = atoi(argv[1]);
        max_time = atoi(argv[2]);
        iter_time = atoi(argv[3]);
        NOP = atoi(argv[4]);
    }
    if (argc >= 5) {
        NOP = atoi(argv[4]);
    }
#endif

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    debug_printf("[%d]comm_size done\n", rank);

    if (2 != nprocs) {
        if (rank == 0)
            fprintf(stderr, "Please run using 2 processes\n");
        goto exit;
    }

    for (i = 0; i < SIZE; i++) {
        locbuf[i] = (i + 1) * 0.5;
    }

    // size in byte
    MPI_Win_allocate(sizeof(double), sizeof(double), MPI_INFO_NULL, MPI_COMM_WORLD, &winbuf, &win);
    winbuf[0] = 0.0;
    debug_printf("[%d]win_allocate done\n", rank);

    for (time = min_time; time <= max_time; time *= iter_time) {
        run_test(time);
    }

    MPI_Win_free(&win);

  exit:

    MPI_Finalize();

    return 0;
}

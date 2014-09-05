#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>


#define D_SLEEP_TIME 100  // 100us

//#define DEBUG
//#define CHECK
#define ITER_S 10000
#define ITER_M 5000
#define ITER_L 1000
#define NPROCS_M 6

#ifdef DEBUG
#define debug_printf(str...) {fprintf(stdout, str);fflush(stdout);}
#else
#define debug_printf(str...) {}
#endif

#ifdef MTCORE
extern int MTCORE_NUM_H;
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
    while (MPI_Wtime() * 1000 * 1000 - start < (double)us);
    return 0;
}

static int run_test(int time)
{
    int i, x, errs = 0, errs_total = 0;
    MPI_Status stat;
    int dst;
    int winbuf_offset = 0;
    double t0, avg_total_time = 0.0, t_total = 0.0;
    double sum = 0.0;

    if (nprocs < NPROCS_M) {
        ITER = ITER_S;
    }
    else if (nprocs >= NPROCS_M && nprocs < NPROCS_M * 2) {
        ITER = ITER_M;
    }
    else {
        ITER = ITER_L;
    }

    MPI_Win_lock_all(0, win);

    t0 = MPI_Wtime();
    for (x = 0; x < ITER; x++) {

        // send to all the left processes in a ring style
        for (dst = (rank + 1) % nprocs; dst != rank; dst = (dst + 1) % nprocs) {
            MPI_Accumulate(&locbuf[0], 1, MPI_DOUBLE, dst, rank, 1, MPI_DOUBLE, MPI_SUM, win);
        }
        MPI_Win_flush_all(win);

        usleep_by_count(time);

        for (dst = (rank + 1) % nprocs; dst != rank; dst = (dst + 1) % nprocs) {
            for (i = 1; i < NOP; i++) {
                MPI_Accumulate(&locbuf[i], 1, MPI_DOUBLE, dst, rank, 1, MPI_DOUBLE, MPI_SUM, win);
            }
        }
        MPI_Win_flush_all(win);

        debug_printf("[%d]MPI_Win_flush all done\n", x);
    }
    t_total = MPI_Wtime() - t0;
    t_total /= ITER;

    MPI_Win_unlock_all(win);
    MPI_Barrier(MPI_COMM_WORLD);

#ifdef CHECK
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win);
    sum = 0.0;
    for (i = 0; i < NOP; i++) {
        sum += locbuf[i];
    }
    sum *= ITER;
    for (i = 0; i < nprocs; i++) {
        if (i == rank)
            continue;
        if (winbuf[i] != sum) {
            fprintf(stderr,
                    "[%d]computation error : winbuf[%d] %.2lf != %.2lf, nop %d\n",
                    rank, i, winbuf[i], sum, nop);
            errs += 1;
        }
    }
    MPI_Win_unlock(rank, win);
#endif

    MPI_Reduce(&t_total, &avg_total_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Allreduce(&errs, &errs_total, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    if (rank == 0) {
        avg_total_time = avg_total_time / nprocs * 1000 * 1000;
#ifdef MTCORE
        fprintf(stdout,
                "mtcore: iter %d comp_size %d num_op %d nprocs %d nh %d total_time %.2lf\n",
                ITER, time, NOP, nprocs, MTCORE_NUM_H, avg_total_time);
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

    debug_printf("[%d]init done, %d/%d\n", rank, rank, nprocs);

    if (nprocs < 2) {
        fprintf(stderr, "Please run using at least 2 processes\n");
        goto exit;
    }

#ifdef MTCORE
    /* first argv is nh */
    if (argc >= 5) {
        min_time = atoi(argv[2]);
        max_time = atoi(argv[3]);
        iter_time = atoi(argv[4]);
    }
    if(argc >= 6){
        NOP = atoi(argv[5]);
    }
#else
    if (argc >= 4) {
        min_time = atoi(argv[1]);
        max_time = atoi(argv[2]);
        iter_time = atoi(argv[3]);
    }
    if(argc >= 5){
        NOP = atoi(argv[4]);
    }
#endif

    locbuf = malloc(sizeof(double) * NOP);
    for (i = 0; i < NOP; i++) {
        locbuf[i] = 1.0;
    }

    // size in byte
    MPI_Win_allocate(sizeof(double) * nprocs, sizeof(double), MPI_INFO_NULL,
                     MPI_COMM_WORLD, &winbuf, &win);
    debug_printf("[%d]win_allocate done\n", rank);

    for (time = min_time; time <= max_time; time *= iter_time) {
        for (i = 0; i < nprocs; i++) {
            winbuf[i] = 0.0;
        }
        MPI_Barrier(MPI_COMM_WORLD);

        errs = run_test(time);
        if (errs > 0)
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

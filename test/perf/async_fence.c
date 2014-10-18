#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>


#define D_SLEEP_TIME 100        // 100us

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
    while (MPI_Wtime() * 1000 * 1000 - start < (double) us);
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

//    MPI_Reduce(&t_total, &avg_total_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
//    MPI_Allreduce(&errs, &errs_total, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    if (rank == 0) {
        avg_total_time = t_total / nprocs * 1000 * 1000;
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
    MPI_Info win_info = MPI_INFO_NULL;

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
    if (argc >= 6) {
        NOP = atoi(argv[5]);
    }
#else
    if (argc >= 4) {
        min_time = atoi(argv[1]);
        max_time = atoi(argv[2]);
        iter_time = atoi(argv[3]);
    }
    if (argc >= 5) {
        NOP = atoi(argv[4]);
    }
#endif

    locbuf = malloc(sizeof(double) * NOP);
    for (i = 0; i < NOP; i++) {
        locbuf[i] = 1.0;
    }

    MPI_Info_create(&win_info);
    MPI_Info_set(win_info, (char *) "epoch_type", (char *) "fence");

    // size in byte
    MPI_Win_allocate(sizeof(double) * nprocs, sizeof(double), win_info,
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

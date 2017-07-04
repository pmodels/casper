#define BENCHMARK "OSU MPI Multiple Bandwidth / Message Rate Test"
/*
 * Copyright (C) 2002-2016 the Network-Based Computing Laboratory
 * (NBCL), The Ohio State University.
 *
 * Contact: Dr. D. K. Panda (panda@cse.ohio-state.edu)
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level OMB directory.
 */

#include <mpi.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#define DEFAULT_WINDOW       (64)

#define ITERS_SMALL          (100)
#define ITERS_LARGE          (20)
#define SKIP_RATE            (0.1)
#define LARGE_THRESHOLD      (8192)

#define MAX_MSG_SIZE         (1<<22)

#ifdef STEP_TIME
#define STEP_TIME_START(i, skip, xt0) do { \
        if (i > skip) xt0 = MPI_Wtime();   \
} while (0)

#define STEP_TIME_STOP(i, skip, xt0, sumt) do {    \
        if (i > skip) sumt += MPI_Wtime() - xt0;   \
} while (0)

double sum_post_time = 0, sum_wait_time = 0, sum_sync_time = 0;
#else
#define STEP_TIME_START(i, skip, xt0) do {} while (0)
#define STEP_TIME_STOP(i, skip, xt0, sumt) do {} while (0)
#endif

MPI_Request *request;
MPI_Status *reqstat;
static MPI_Comm comm_world = MPI_COMM_NULL;

double calc_bw(int rank, int size, int num_pairs, int window_size, char *s_buf, char *r_buf);
void usage();

static int loop;
static int skip;
static int loop_override;
static int skip_override;
static int computation;
static double sum_time = 0;
static char testname[128] = { 0 };

static void set_testname(void)
{
    char *val = getenv("TEST_NAME");
    if (val && strlen(val) > 0) {
        strncpy(testname, val, 128);
    }
}

int main(int argc, char *argv[])
{
    char *s_buf, *r_buf;
    unsigned long align_size = sysconf(_SC_PAGESIZE);
    int numprocs, rank;
    int pairs;
    int window_size;
    int c, curr_size;
    MPI_Win win = MPI_WIN_NULL;
    MPI_Info info = MPI_INFO_NULL;
    MPI_Comm shm_comm = MPI_COMM_NULL;

    loop_override = 0;
    skip_override = 0;

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* default values */
    pairs = numprocs / 2;
    window_size = DEFAULT_WINDOW;
    computation = 0;

    while ((c = getopt(argc, argv, "p:w:r:x:i:c:vh")) != -1) {
        switch (c) {
        case 'i':
            loop = atoi(optarg);
            loop_override = 1;
            break;
        case 'x':
            skip = atoi(optarg);
            skip_override = 1;
            break;
        case 'p':
            pairs = atoi(optarg);

            if (pairs > (numprocs / 2)) {
                if (0 == rank) {
                    usage();
                }

                goto error;
            }

            break;

        case 'w':
            window_size = atoi(optarg);
            break;

        case 'c':
            computation = atoi(optarg);
            break;

        default:
            if (0 == rank) {
                usage();
            }

            goto error;
        }
    }

    set_testname();

    MPI_Info_create(&info);
    /* Register as shared buffer in Casper. */
    MPI_Info_set(info, (char *) "shmbuf_regist", (char *) "true");

    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, rank, info, &shm_comm);
    MPI_Win_allocate_shared((MAX_MSG_SIZE + align_size) * 2, 1, MPI_INFO_NULL,
                            shm_comm, &s_buf, &win);

    r_buf = s_buf + MAX_MSG_SIZE + align_size;

    s_buf += (align_size - ((uint64_t) s_buf % align_size));
    r_buf += (align_size - ((uint64_t) r_buf % align_size));

#if defined(USE_DUPCOMM)
    MPI_Info_set(info, (char *) "wildcard_used", (char *) "anytag_notag");
#else
    MPI_Info_set(info, (char *) "wildcard_used", (char *) "none");
#endif
    MPI_Comm_dup_with_info(MPI_COMM_WORLD, info, &comm_world);

    if (numprocs < 2) {
        if (rank == 0) {
            fprintf(stderr, "This test requires at least two processes\n");
        }

        MPI_Finalize();

        return EXIT_FAILURE;
    }

    if (rank == 0) {
        fprintf(stdout, "# [ pairs: %d ] [ window size: %d ]\n", pairs, window_size);
        fprintf(stdout, "sbuf=%p, rbuf=%p\n", s_buf, r_buf);
        fprintf(stdout, "%s, %s, %s, %s, %s, %s\n", "# Size", "Iters", "Computation (us)", "MB/s",
                "Messages/s", "Time (us)");
        fflush(stdout);
    }

    /* Just one window size */
    request = (MPI_Request *) malloc(sizeof(MPI_Request) * window_size);
    reqstat = (MPI_Status *) malloc(sizeof(MPI_Status) * window_size);

    for (curr_size = 1; curr_size <= MAX_MSG_SIZE; curr_size *= 2) {
        double bw, rate;

        bw = calc_bw(rank, curr_size, pairs, window_size, s_buf, r_buf);

        if (rank == 0) {
            rate = 1e6 * bw / curr_size;
            fprintf(stdout, "%s %d, %d, %d, %d, %.2f, %.2f, %.2f"
#ifdef STEP_TIME
                    ", %.2f, %.2f, %.2f"
#endif
                    "\n", testname, numprocs, curr_size, loop, computation, bw, rate, sum_time
#ifdef STEP_TIME
                    , sum_post_time, sum_wait_time, sum_sync_time
#endif
);
            fflush(stdout);
        }
    }

  error:
    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    if (info != MPI_INFO_NULL)
        MPI_Info_free(&info);
    if (shm_comm != MPI_COMM_NULL)
        MPI_Comm_free(&shm_comm);
    if (comm_world != MPI_COMM_NULL)
        MPI_Comm_free(&comm_world);

    MPI_Finalize();

    return EXIT_SUCCESS;
}

void usage()
{
    printf("Options:\n");
    printf("  -p=<pairs>       Number of pairs involved (default np / 2)\n");
    printf("  -w=<window>      Number of messages sent before acknowledgement (64, 10)\n");
    printf("  -i               number of iterations\n");
    printf("  -x               number of skiped iterations\n");
    printf("  -c               computation time (us)\n");
    printf("  -h               Print this help\n");
    printf("\n");
    printf("  Note: This benchmark relies on block ordering of the ranks.  Please see\n");
    printf("        the README for more information.\n");
    fflush(stdout);
}

static void delay(void)
{
    double start, end;

    if (computation == 0)
        return;

    start = MPI_Wtime();
    do {
        end = MPI_Wtime();
    } while ((1e6 * (end - start)) < computation);
}

double calc_bw(int rank, int size, int num_pairs, int window_size, char *s_buf, char *r_buf)
{
    double t_start = 0, t_end = 0, t = 0, bw = 0;
    int i, j, target;
    int mult = (DEFAULT_WINDOW / window_size) > 0 ? (DEFAULT_WINDOW / window_size) : 1;

#ifdef STEP_TIME
    double pt0 = 0, post_time = 0;
    double wt0 = 0, wait_time = 0;
    double st0 = 0, sync_time = 0;
#endif

    for (i = 0; i < size; i++) {
        s_buf[i] = 'a';
        r_buf[i] = 'b';
    }

    if (!loop_override) {
        if (size > LARGE_THRESHOLD)
            loop = ITERS_LARGE * mult;
        else
            loop = ITERS_SMALL * mult;
    }

    if (!skip_override)
        skip = loop * (SKIP_RATE);

    MPI_Barrier(comm_world);

    if (rank < num_pairs) {
        target = rank + num_pairs;

        for (i = 0; i < loop + skip; i++) {
            if (i == skip) {
                MPI_Barrier(comm_world);
                t_start = MPI_Wtime();
            }

            STEP_TIME_START(i, skip, pt0);
            for (j = 0; j < window_size; j++) {
                MPI_Isend(s_buf, size, MPI_CHAR, target, 100, comm_world, request + j);
            }
            STEP_TIME_STOP(i, skip, pt0, post_time);

            delay();

            STEP_TIME_START(i, skip, wt0);
            MPI_Waitall(window_size, request, reqstat);
            STEP_TIME_STOP(i, skip, wt0, wait_time);

            STEP_TIME_START(i, skip, st0);
            MPI_Recv(r_buf, 4, MPI_CHAR, target, 101, comm_world, &reqstat[0]);
            STEP_TIME_STOP(i, skip, st0, sync_time);
        }

        t_end = MPI_Wtime();
        t = t_end - t_start;
    }

    else if (rank < num_pairs * 2) {
        target = rank - num_pairs;

        for (i = 0; i < loop + skip; i++) {
            if (i == skip) {
                MPI_Barrier(comm_world);
            }

            for (j = 0; j < window_size; j++) {
                MPI_Irecv(r_buf, size, MPI_CHAR, target, 100, comm_world, request + j);
            }

            MPI_Waitall(window_size, request, reqstat);
            MPI_Send(s_buf, 4, MPI_CHAR, target, 101, comm_world);
        }
    }

    else {
        MPI_Barrier(comm_world);
    }

    MPI_Reduce(&t, &sum_time, 1, MPI_DOUBLE, MPI_SUM, 0, comm_world);
#ifdef STEP_TIME
    MPI_Reduce(&post_time, &sum_post_time, 1, MPI_DOUBLE, MPI_SUM, 0, comm_world);
    MPI_Reduce(&wait_time, &sum_wait_time, 1, MPI_DOUBLE, MPI_SUM, 0, comm_world);
    MPI_Reduce(&sync_time, &sum_sync_time, 1, MPI_DOUBLE, MPI_SUM, 0, comm_world);

    sum_post_time *= 1e6 / num_pairs / loop / window_size;
    sum_wait_time *= 1e6 / num_pairs / loop / window_size;
    sum_sync_time *= 1e6 / num_pairs / loop / window_size;
#endif
    if (rank == 0) {
        sum_time = sum_time * 1e6 / num_pairs / loop / window_size;     /* us */
        bw = size / sum_time;   /* MB/s */

        return bw;
    }

    return 0;
}

/* vi: set sw=4 sts=4 tw=80: */

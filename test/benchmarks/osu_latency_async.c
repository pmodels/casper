#define BENCHMARK "OSU MPI%s Latency Test"
/*
 * Copyright (C) 2002-2015 the Network-Based Computing Laboratory
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

#define ITERS_SMALL          (100)
#define ITERS_LARGE          (20)
#define SKIP_RATE            (0.1)      /* 10% of ITERS */
#define LARGE_THRESHOLD      (8192)

#define MAX_MSG_SIZE         (1<<22)


static int computation = 0;     /* in usec */

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

static void usage()
{
    printf("Options:\n");
    printf("  -i               number of iterations\n");
    printf("  -x               number of skiped iterations\n");
    printf("  -c               computation time (us)\n");
    printf("  -h               Print this help\n");
    printf("\n");
    printf("  Note: This benchmark relies on block ordering of the ranks.  Please see\n");
    printf("        the README for more information.\n");
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    int rank, numprocs, i, target;
    int size;
    int loop, skip, loop_override = 0, skip_override = 0;
    MPI_Status stats[2];
    MPI_Request reqs[2];
    char *sbuf, *rbuf;
    double t_start = 0.0, t_end = 0.0;
    unsigned long align_size = sysconf(_SC_PAGESIZE);
    int c;
    MPI_Info info = MPI_INFO_NULL;
    MPI_Comm shm_comm = MPI_COMM_NULL;
    MPI_Win win = MPI_WIN_NULL;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (numprocs != 2) {
        if (rank == 0) {
            fprintf(stderr, "This test requires exactly two processes\n");
        }

        MPI_Finalize();
        exit(EXIT_FAILURE);
    }

    /* default values */
    computation = 0;

    while ((c = getopt(argc, argv, "i:x:c:h:")) != -1) {
        switch (c) {
        case 'i':
            loop = atoi(optarg);
            loop_override = 1;
            break;
        case 'x':
            skip = atoi(optarg);
            skip_override = 1;
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

    MPI_Info_create(&info);
    /* Register as shared buffer in Casper. */
    MPI_Info_set(info, (char *) "shmbuf_regist", (char *) "true");

    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, rank, MPI_INFO_NULL, &shm_comm);
    MPI_Win_allocate_shared((MAX_MSG_SIZE + align_size) * 2, 1, info, MPI_COMM_WORLD, &sbuf, &win);
    rbuf = sbuf + MAX_MSG_SIZE + align_size;

    sbuf += (align_size - ((uint64_t) sbuf % align_size));
    rbuf += (align_size - ((uint64_t) rbuf % align_size));

    /* Print header */
    if (0 == rank) {
        printf("sbuf=%p, rbuf=%p\n", sbuf, rbuf);
        printf("%s, %s, %s, %s\n", "# Size", "Iters", "Computation (us)", "Latency (us)");
        fflush(stdout);
    }

    target = (rank == 0) ? 1 : 0;

    /* Latency test */
    for (size = 0; size <= MAX_MSG_SIZE; size = (size ? size * 2 : 1)) {
#ifdef STEP_TIME
        double pt0 = 0, post_time = 0;
        double wt0 = 0, wait_time = 0;
#endif

        memset(sbuf, 'a', size);
        memset(rbuf, 'b', size);

        if (!loop_override) {
            if (size > LARGE_THRESHOLD)
                loop = ITERS_LARGE;
            else
                loop = ITERS_SMALL;
        }

        if (!skip_override)
            skip = loop * (SKIP_RATE);

        MPI_Barrier(MPI_COMM_WORLD);

        for (i = 0; i < loop + skip; i++) {
            if (i == skip)
                t_start = MPI_Wtime();

#ifdef STEP_TIME
            if (i > skip)
                pt0 = MPI_Wtime();
#endif
            MPI_Isend(sbuf, size, MPI_CHAR, target, i, MPI_COMM_WORLD, &reqs[0]);
            MPI_Irecv(rbuf, size, MPI_CHAR, target, i, MPI_COMM_WORLD, &reqs[1]);
#ifdef STEP_TIME
            if (i > skip)
                post_time += MPI_Wtime() - pt0;
#endif
            delay();

#ifdef STEP_TIME
            if (i > skip)
                wt0 = MPI_Wtime();
#endif
            MPI_Wait(&reqs[0], &stats[0]);
            MPI_Wait(&reqs[1], &stats[1]);
#ifdef STEP_TIME
            if (i > skip)
                wait_time += MPI_Wtime() - wt0;
#endif
        }
        t_end = MPI_Wtime();

        if (rank == 0) {
            double latency = (t_end - t_start) * 1e6 / (2.0 * loop);
#ifdef STEP_TIME
            post_time = post_time * 1e6 / (loop * 2.0);
            wait_time = wait_time * 1e6 / (loop * 2.0);
#endif

            fprintf(stdout, "%d, %d, %d, %.2f"
#ifdef STEP_TIME
                    ", %.2f, %.2f"
#endif
                    "\n", size, loop, computation, latency
#ifdef STEP_TIME
                    , post_time, wait_time
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
    MPI_Finalize();

    return EXIT_SUCCESS;
}

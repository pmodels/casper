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

/*
 * Modified by Argonne National Laboratory.
 * (C) 2017 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#include <mpi.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

/* This benchmark evaluates point to point latency with computing delay.*/

#define ITERS_SMALL          (100)
#define ITERS_LARGE          (20)
#define SKIP_RATE            (0.1)      /* 10% of ITERS */
#define LARGE_THRESHOLD      (8192)

#define MAX_MSG_SIZE         (1<<22)
#define MAX_FNAME_LEN        (128)
#define MAX_FLINE_LEN        (256)
#define NMSG_SIZE            (22+2)

static char compf_name[MAX_FNAME_LEN] = { 0 };

static int compf_set_flag = 0;
static int compf_sz_comps[NMSG_SIZE] = { 0 };

static int computation = 0;     /* in usec */
static MPI_Comm comm_world = MPI_COMM_NULL;
static char testname[128] = { 0 };

static int pairs = 0;

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

static void usage(void)
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

static void set_testname(void)
{
    char *val = getenv("TEST_NAME");
    if (val && strlen(val) > 0) {
        strncpy(testname, val, 128);
    }
}

static void read_comp(void)
{
    FILE *comp_fp = NULL;
    char line[MAX_FLINE_LEN];
    int nsz, sz, lat = 0;
    int rank = 0;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0) {
        comp_fp = fopen(compf_name, "r");
        if (!comp_fp) {
            fprintf(stderr, "Cannot open file %s\n", compf_name);
            fflush(stderr);
            MPI_Abort(MPI_COMM_WORLD, -1);
        }

        /* Read computation time for each size. */
        nsz = 0;
        while (fgets(line, sizeof(line), comp_fp) && nsz < NMSG_SIZE) {
            sscanf(line, "%d %d", &sz, &lat);
            compf_sz_comps[nsz++] = lat;
        }

        fclose(comp_fp);
    }

    MPI_Bcast(compf_sz_comps, NMSG_SIZE, MPI_INT, 0, MPI_COMM_WORLD);
}

int main(int argc, char *argv[])
{
    int rank, numprocs, i, target;
    int size, nsz;
    int loop = ITERS_SMALL, skip = ITERS_SMALL * SKIP_RATE;
    int loop_override = 0, skip_override = 0;
    MPI_Status stats[2];
    MPI_Request reqs[2];
    char *sbuf, *rbuf;
    double t_start = 0.0, time = 0.0, avg_time = 0.0;
    unsigned long align_size = sysconf(_SC_PAGESIZE);
    int c;
    MPI_Info info = MPI_INFO_NULL;
    MPI_Comm shm_comm = MPI_COMM_NULL;
    MPI_Win win = MPI_WIN_NULL;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (numprocs < 2 || numprocs % 2) {
        if (rank == 0) {
            fprintf(stderr, "This test requires power of two processes\n");
        }

        MPI_Finalize();
        exit(EXIT_FAILURE);
    }

    /* default values */
    computation = 0;

    while ((c = getopt(argc, argv, "i:x:c:h:f:p:")) != -1) {
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
        case 'f':
            if (strlen(optarg) > 0) {
                strncpy(compf_name, optarg, MAX_FNAME_LEN);
                compf_set_flag = 1;
            }
            break;

        default:
            if (0 == rank) {
                usage();
            }

            goto error;
        }
    }

    set_testname();

    if (compf_set_flag)
        read_comp();

    MPI_Info_create(&info);
    /* Register as shared buffer in Casper. */
    MPI_Info_set(info, (char *) "shmbuf_regist", (char *) "true");

    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, rank, info, &shm_comm);
    MPI_Win_allocate_shared((MAX_MSG_SIZE + align_size) * 2, 1, MPI_INFO_NULL, shm_comm, &sbuf,
                            &win);
    rbuf = sbuf + MAX_MSG_SIZE + align_size;

    sbuf += (align_size - ((uint64_t) sbuf % align_size));
    rbuf += (align_size - ((uint64_t) rbuf % align_size));

    MPI_Info_set(info, (char *) "shmbuf_regist", (char *) "false");
#if defined(USE_DUPCOMM)
    MPI_Info_set(info, (char *) "wildcard_used", (char *) "anytag_notag");
#else
    MPI_Info_set(info, (char *) "wildcard_used", (char *) "none");
#endif
    /* By default disable offloading threshold for overhead measurement */
#if !defined(USE_OFFLOAD_MIN)
    MPI_Info_set(info, (char *) "offload_min_msgsz", (char *) "0");
#endif
    MPI_Comm_dup_with_info(MPI_COMM_WORLD, info, &comm_world);

    /* Print header */
    if (0 == rank) {
        printf("sbuf=%p, rbuf=%p\n", sbuf, rbuf);
        printf("%s, %s, %s, %s\n", "# Size", "Iters", "Computation (us)", "Latency (us)");
        fflush(stdout);
    }

    pairs = numprocs / 2;
    if (rank < pairs) {
        target = rank + pairs;
    }
    else {
        target = rank - pairs;
    }
    printf("rank %d, target %d\n", rank, target);
    fflush(stdout);
    /* Latency test */
    nsz = 0;
    for (size = 0; size <= MAX_MSG_SIZE; size = (size ? size * 2 : 1)) {
#ifdef STEP_TIME
        double pt0 = 0, post_time = 0, avg_post_time = 0;
        double wt0 = 0, wait_time = 0, avg_wait_time = 0;
#endif

        memset(sbuf, 'a', size);
        memset(rbuf, 'b', size);

        if (compf_set_flag)
            computation = compf_sz_comps[nsz++];

        if (!loop_override) {
            if (size > LARGE_THRESHOLD)
                loop = ITERS_LARGE;
            else
                loop = ITERS_SMALL;
        }

        if (!skip_override)
            skip = loop * (SKIP_RATE);

        MPI_Barrier(comm_world);

        for (i = 0; i < loop + skip; i++) {
            if (i == skip)
                t_start = MPI_Wtime();

#ifdef STEP_TIME
            if (i > skip)
                pt0 = MPI_Wtime();
#endif
            MPI_Isend(sbuf, size, MPI_CHAR, target, i, comm_world, &reqs[0]);
            MPI_Irecv(rbuf, size, MPI_CHAR, target, i, comm_world, &reqs[1]);
#ifdef STEP_TIME
            if (i > skip)
                post_time += MPI_Wtime() - pt0;
#endif
            delay();

#ifdef STEP_TIME
            if (i > skip)
                wt0 = MPI_Wtime();
#endif
            MPI_Waitall(2, reqs, stats);
#ifdef STEP_TIME
            if (i > skip)
                wait_time += MPI_Wtime() - wt0;
#endif
        }
        time = MPI_Wtime() - t_start;


        MPI_Reduce(&time, &avg_time, 1, MPI_DOUBLE, MPI_SUM, 0, comm_world);
        avg_time /= numprocs;

#ifdef STEP_TIME
        MPI_Reduce(&post_time, &avg_post_time, 1, MPI_DOUBLE, MPI_SUM, 0, comm_world);
        avg_post_time /= numprocs;
        MPI_Reduce(&wait_time, &avg_wait_time, 1, MPI_DOUBLE, MPI_SUM, 0, comm_world);
        avg_wait_time /= numprocs;
#endif

        if (rank == 0) {
            double latency = avg_time * 1e6 / (loop);
#ifdef STEP_TIME
            avg_post_time = avg_post_time * 1e6 / (loop);
            avg_wait_time = avg_wait_time * 1e6 / (loop);
#endif

            fprintf(stdout, "%s %d, %d, %d, %d, %.2f"
#ifdef STEP_TIME
                    ", %.2f, %.2f"
#endif
                    "\n", testname, numprocs, size, loop, computation, latency
#ifdef STEP_TIME
                    , avg_post_time, avg_wait_time
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

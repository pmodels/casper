/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2017 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mpi.h"

/* This benchmark evaluates overhead of various communicator creation functions.*/

#define DEFAULT_ITERS  (1024)

static int iters = DEFAULT_ITERS;
static MPI_Comm *comm;
static int comm_rank, comm_size;
static char testname[128] = { 0 };

static void usage(void)
{
    printf("./a.out\n");
    printf("     --iters [iterations; default %d]\n", DEFAULT_ITERS);
    exit(1);
}

static void set_testname(void)
{
    char *val = getenv("TEST_NAME");
    if (val && strlen(val) > 0) {
        strncpy(testname, val, 128);
    }
}

static void run_split(void)
{
    double start, end, local_time, avg_time;
    int i;

    start = MPI_Wtime();
    for (i = 0; i < iters; i++)
        MPI_Comm_split(MPI_COMM_WORLD, 0, 0, &comm[i]);
    end = MPI_Wtime();

    local_time = end - start;
    MPI_Reduce(&local_time, &avg_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    avg_time /= comm_size;

    if (comm_rank == 0)
        printf("%s comm_split: %.3f us\n", testname, 1e6 * (end - start) / iters);
}

static void run_dup(void)
{
    double start, end, local_time, avg_time;
    int i;

    start = MPI_Wtime();
    for (i = 0; i < iters; i++)
        MPI_Comm_dup(MPI_COMM_WORLD, &comm[i]);
    end = MPI_Wtime();

    local_time = end - start;
    MPI_Reduce(&local_time, &avg_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    avg_time /= comm_size;

    if (comm_rank == 0)
        printf("%s comm_dup: %.3f us\n", testname, 1e6 * (end - start) / iters);
}

static void run_create(void)
{
    double start, end, local_time, avg_time;
    int i;
    MPI_Group group;

    MPI_Comm_group(MPI_COMM_WORLD, &group);

    start = MPI_Wtime();
    for (i = 0; i < iters; i++)
        MPI_Comm_create(MPI_COMM_WORLD, group, &comm[i]);
    end = MPI_Wtime();

    MPI_Group_free(&group);

    local_time = end - start;
    MPI_Reduce(&local_time, &avg_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    avg_time /= comm_size;

    if (comm_rank == 0)
        printf("%s comm_create: %.3f us\n", testname, 1e6 * (end - start) / iters);
}

static void run_free(const char *creat_type)
{
    double start, end, local_time, avg_time;
    int i;

    /* free time */
    start = MPI_Wtime();
    for (i = 0; i < iters; i++)
        MPI_Comm_free(&comm[i]);
    end = MPI_Wtime();

    local_time = end - start;
    MPI_Reduce(&local_time, &avg_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    avg_time /= comm_size;

    if (comm_rank == 0)
        printf("%s %s_free %.3f us\n", testname, creat_type, 1e6 * (end - start) / iters);
}

int main(int argc, char **argv)
{
#if defined(USE_DUPCOMM) || defined(USE_TAGTRANS)
    MPI_Info info = MPI_INFO_NULL;
#endif
    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);

    while (--argc && ++argv) {
        if (!strcmp(*argv, "--iters")) {
            --argc;
            ++argv;
            iters = atoi(*argv);
        }
        else {
            usage();
        }
    }

    set_testname();

#if defined(USE_DUPCOMM)
    MPI_Info_create(&info);
    MPI_Info_set(info, (char *) "wildcard_used", (char *) "anysrc|anytag_notag");
    MPI_Comm_set_info(MPI_COMM_WORLD, info);
    MPI_Info_free(&info);
#elif defined(USE_TAGTRANS)
    MPI_Info_create(&info);
    MPI_Info_set(info, (char *) "wildcard_used", (char *) "none");
    MPI_Comm_set_info(MPI_COMM_WORLD, info);
    MPI_Info_free(&info);
#endif

    comm = (MPI_Comm *) malloc(iters * sizeof(MPI_Comm));

    run_split();
    run_free("comm_split");

    run_create();
    run_free("comm_create");

    run_dup();
    run_free("comm_dup");

    free(comm);

    MPI_Finalize();

    return 0;
}

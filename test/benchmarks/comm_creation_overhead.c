#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mpi.h"

#define DEFAULT_ITERS  (1024)

static int iters = DEFAULT_ITERS;
static MPI_Comm *comm;

static void usage(void)
{
    printf("./a.out\n");
    printf("     --iters [iterations; default %d]\n", DEFAULT_ITERS);
    exit(1);
}

int main(int argc, char **argv)
{
    int i;
    double start, end, local_time, avg_time;
    int comm_rank, comm_size;

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

    comm = (MPI_Comm *) malloc(iters * sizeof(MPI_Comm));


    /* split time */
    start = MPI_Wtime();
    for (i = 0; i < iters; i++)
        MPI_Comm_split(MPI_COMM_WORLD, 0, 0, &comm[i]);
    end = MPI_Wtime();

    local_time = end - start;
    MPI_Reduce(&local_time, &avg_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    avg_time /= comm_size;

    if (comm_rank == 0)
        printf("comm_split: %.3f us\n", 1e6 * (end - start) / iters);


    /* free time */
    start = MPI_Wtime();
    for (i = 0; i < iters; i++)
        MPI_Comm_free(&comm[i]);
    end = MPI_Wtime();

    local_time = end - start;
    MPI_Reduce(&local_time, &avg_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    avg_time /= comm_size;

    if (comm_rank == 0)
        printf("comm_free for split comm: %.3f us\n", 1e6 * (end - start) / iters);


    /* create time */
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
        printf("comm_create: %.3f us\n", 1e6 * (end - start) / iters);


    /* free time */
    start = MPI_Wtime();
    for (i = 0; i < iters; i++)
        MPI_Comm_free(&comm[i]);
    end = MPI_Wtime();

    local_time = end - start;
    MPI_Reduce(&local_time, &avg_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    avg_time /= comm_size;

    if (comm_rank == 0)
        printf("comm_free for created comm: %.3f us\n", 1e6 * (end - start) / iters);


    free(comm);

    MPI_Finalize();

    return 0;
}

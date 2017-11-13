#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "mpi.h"

/* This benchmark evaluates 2D halo exchange with cart and basic datatype.
 * It also sets info hints to enable Casper message offloading. */

#define DEFAULT_ITERS  (1024)
#define DEFAULT_DIM    (1024)

static int iters = DEFAULT_ITERS;
static int dim = DEFAULT_DIM;
static char testname[128] = { 0 };

static int computation = 0;

static void usage(void)
{
    printf("./a.out\n");
    printf("     --iters [iterations; default %d] --dim [dimension size %d] "
           " -c [computing delay %d]\n", DEFAULT_ITERS, DEFAULT_DIM, 0);
    exit(1);
}

static void set_testname(void)
{
    char *val = getenv("TEST_NAME");
    if (val && strlen(val) > 0) {
        strncpy(testname, val, 128);
    }
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

#define north_pack_ind(off) (off)
#define south_pack_ind(off) (dim + (off))
#define west_pack_ind(off) (dim * 2 + off)
#define east_pack_ind(off) (dim * 3 + (off))

int main(int argc, char **argv)
{
    int i;
    double start, end, local_time, avg_time;
    int comm_rank, comm_size;
    int dims[2] = { 0, 0 }, periods[2] = {
    1, 1};
    int north, south, east, west;
    double *packrbuf, *packsbuf, *winbuf = NULL;
    MPI_Comm comm, shm_comm;
    MPI_Request req[8];
    MPI_Datatype type;
    MPI_Info info = MPI_INFO_NULL;
    MPI_Aint packbuf_sz = 0;
    MPI_Win shm_win;

    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);

    set_testname();

    while (--argc && ++argv) {
        if (!strcmp(*argv, "--iters")) {
            --argc;
            ++argv;
            iters = atoi(*argv);
        }
        else if (!strcmp(*argv, "--dim")) {
            --argc;
            ++argv;
            dim = atoi(*argv);
        }
        else if (!strcmp(*argv, "-c")) {
            --argc;
            ++argv;
            computation = atoi(*argv);
        }
        else {
            usage();
        }
    }

    /* Allocate and register shared buffer. */
    MPI_Info_create(&info);
    MPI_Info_set(info, (char *) "shmbuf_regist", (char *) "true");

    packbuf_sz = dim * 4;       /* four boundaries */
    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, comm_rank, info, &shm_comm);
    MPI_Win_allocate_shared(packbuf_sz * sizeof(double) * 2, 1, MPI_INFO_NULL,
                            shm_comm, &winbuf, &shm_win);
    MPI_Info_free(&info);

    packsbuf = winbuf;
    packrbuf = winbuf + packbuf_sz;

    MPI_Type_vector(dim, 1, dim + 2, MPI_DOUBLE, &type);
    MPI_Type_commit(&type);

    MPI_Dims_create(comm_size, 2, dims);

    /* Enable asynchronous progress. */
    MPI_Info_create(&info);
    MPI_Info_set(info, (char *) "wildcard_used", (char *) "none");
    MPI_Info_set(info, (char *) "datatype_used", (char *) "predefined");
    MPI_Comm_set_info(MPI_COMM_WORLD, info);
    MPI_Info_free(&info);

    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 1, &comm);
    assert(comm != MPI_COMM_NULL);

    MPI_Cart_shift(comm, 0, 1, &north, &south);
    MPI_Cart_shift(comm, 1, 1, &west, &east);

    if (comm_rank == 0) {
        printf
            (">>>>> winbuf=%p, packsbuf=%p (off 0x%lx), packrbuf=%p (off 0x%lx), packbuf_sz=%ld\n",
             winbuf, packsbuf, (MPI_Aint) ((char *) packsbuf - (char *) winbuf), packrbuf,
             (MPI_Aint) ((char *) packrbuf - (char *) winbuf), packbuf_sz);
        fflush(stdout);
        printf("rank %d: north %d, south %d, west %d, east %d\n",
               comm_rank, north, south, west, east);
        fflush(stdout);
    }

#ifdef STEP_TIME
    double cp_t0 = 0, cp_time = 0, avg_cp_time = 0;
    double w_t0 = 0, wait_time = 0, avg_wait_time = 0;
    double p_t0 = 0, post_time = 0, avg_post_time = 0;
#endif

    MPI_Barrier(MPI_COMM_WORLD);

    start = MPI_Wtime();
    for (i = 0; i < iters; i++) {
        int nreqs = 0;

#ifdef STEP_TIME
        p_t0 = MPI_Wtime();
#endif
        /* Boundary exchange */
        MPI_Irecv(&packrbuf[north_pack_ind(0)], dim, MPI_DOUBLE, north, i, comm, &req[nreqs++]);
        MPI_Isend(&packsbuf[north_pack_ind(0)], dim, MPI_DOUBLE, north, i, comm, &req[nreqs++]);

        MPI_Irecv(&packrbuf[south_pack_ind(0)], dim, MPI_DOUBLE, south, i, comm, &req[nreqs++]);
        MPI_Isend(&packsbuf[south_pack_ind(0)], dim, MPI_DOUBLE, south, i, comm, &req[nreqs++]);

        MPI_Irecv(&packrbuf[west_pack_ind(0)], dim, MPI_DOUBLE, west, i, comm, &req[nreqs++]);
        MPI_Isend(&packsbuf[west_pack_ind(0)], dim, MPI_DOUBLE, west, i, comm, &req[nreqs++]);

        MPI_Irecv(&packrbuf[east_pack_ind(0)], dim, MPI_DOUBLE, east, i, comm, &req[nreqs++]);
        MPI_Isend(&packsbuf[east_pack_ind(0)], dim, MPI_DOUBLE, east, i, comm, &req[nreqs++]);

#ifdef STEP_TIME
        post_time += MPI_Wtime() - p_t0;
        cp_t0 = MPI_Wtime();
#endif

        delay();

#ifdef STEP_TIME
        cp_time += MPI_Wtime() - cp_t0;
        w_t0 = MPI_Wtime();
#endif
        MPI_Waitall(nreqs, req, MPI_STATUSES_IGNORE);
#ifdef STEP_TIME
        wait_time += MPI_Wtime() - w_t0;
#endif
    }
    end = MPI_Wtime();

    local_time = end - start;
    MPI_Reduce(&local_time, &avg_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    avg_time /= comm_size;

#ifdef STEP_TIME
    MPI_Reduce(&cp_time, &avg_cp_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    avg_cp_time /= comm_size;
    MPI_Reduce(&wait_time, &avg_wait_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    avg_wait_time /= comm_size;
    MPI_Reduce(&post_time, &avg_post_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    avg_post_time /= comm_size;
#endif

    if (comm_rank == 0) {
        printf("%s dim %d nproc %d average_time %.3f us "
#ifdef STEP_TIME
               "compute_time %.3f us post_time %.3f us wait_time %.3f us"
#endif
               "\n", testname, dim, comm_size, 1e6 * avg_time / iters
#ifdef STEP_TIME
               , 1e6 * avg_cp_time / iters, 1e6 * avg_post_time / iters, 1e6 * avg_wait_time / iters
#endif
);
    }

    MPI_Win_free(&shm_win);
    MPI_Comm_free(&shm_comm);

    MPI_Comm_free(&comm);
    MPI_Type_free(&type);

    MPI_Finalize();

    return 0;
}

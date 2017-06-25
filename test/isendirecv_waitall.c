/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mpi.h>
#include "ctest.h"

/*
 * This test checks round-trip isend and irecv with waitall.
 */

#define NUM_OPS 100
#define COUNT 100

double *sbuf = NULL, *rbuf = NULL;
int rank, nprocs;
MPI_Win sbuf_win = MPI_WIN_NULL, rbuf_win = MPI_WIN_NULL;
MPI_Comm comm_world = MPI_COMM_NULL;
int ITER = 10;

static int check_stat(MPI_Status stat, int peer, int tag)
{
    int errs = 0;

    if (stat.MPI_TAG != tag) {
        fprintf(stderr, "[%d] stat.MPI_TAG %d != %d\n", rank, stat.MPI_TAG, tag);
        fflush(stderr);
        errs++;
    }
    if (stat.MPI_SOURCE != peer) {
        fprintf(stderr, "[%d] stat.MPI_SOURCE %d != %d\n", rank, stat.MPI_SOURCE, peer);
        fflush(stderr);
        errs++;
    }
    if (stat.MPI_ERROR != MPI_SUCCESS) {
        fprintf(stderr, "[%d] stat.MPI_ERROR 0x%x != MPI_SUCCESS 0x%x\n",
                rank, stat.MPI_ERROR, MPI_SUCCESS);
        fflush(stderr);
        errs++;
    }

    return errs;
}

static int run_test(void)
{
    int i, x, c, errs = 0, errs_total = 0;
    int peer;
    MPI_Request reqs[NUM_OPS * 2];
    MPI_Status stats[NUM_OPS * 2];
    int ncmpl = 0, cmpl[NUM_OPS];

    if (rank % 2)
        peer = (rank - 1 + nprocs) % nprocs;
    else
        peer = (rank + 1) % nprocs;

    for (x = 0; x < ITER; x++) {
        ncmpl = 0;

        for (i = 0; i < NUM_OPS; i++) {
            MPI_Isend(&sbuf[i * COUNT], COUNT, MPI_DOUBLE, peer, i, comm_world, &reqs[i * 2]);
            MPI_Irecv(&rbuf[i * COUNT], COUNT, MPI_DOUBLE, peer, i, comm_world, &reqs[i * 2 + 1]);
        }

        memset(stats, 0, sizeof(stats));
        MPI_Waitall(NUM_OPS * 2, reqs, stats);

        /* check completed receive */
        for (i = 0; i < NUM_OPS; i++) {
            for (c = 0; c < COUNT; c++) {
                if (CTEST_double_diff(rbuf[i * COUNT + c], 1.0 * i * COUNT + c + peer)) {
                    fprintf(stderr,
                            "[%d] rbuf[%d] %.1lf != %.1lf\n",
                            rank, i * COUNT + c, rbuf[i * COUNT + c], 1.0 * i * COUNT + c + peer);
                    fflush(stderr);
                    errs++;
                }
            }
            errs += check_stat(stats[i * 2 + 1], peer, i);
        }
    }


    MPI_Allreduce(&errs, &errs_total, 1, MPI_INT, MPI_SUM, comm_world);
    return errs_total;
}

int main(int argc, char *argv[])
{
    int size = NUM_OPS;
    int i, errs = 0;
    MPI_Info info = MPI_INFO_NULL;
    MPI_Comm shm_comm = MPI_COMM_NULL;

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (nprocs < 2 || nprocs % 2) {
        fprintf(stderr, "Please run using power of two number of processes\n");
        goto exit;
    }

    MPI_Info_create(&info);

    /* Register as shared buffer in Casper. */
    MPI_Info_set(info, (char *) "shmbuf_regist", (char *) "true");
    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, rank, info, &shm_comm);

    MPI_Win_allocate_shared(sizeof(double) * NUM_OPS * COUNT, sizeof(double),
                            MPI_INFO_NULL, shm_comm, &sbuf, &sbuf_win);
    MPI_Win_allocate_shared(sizeof(double) * NUM_OPS * COUNT, sizeof(double),
                            MPI_INFO_NULL, shm_comm, &rbuf, &rbuf_win);

    for (i = 0; i < NUM_OPS * COUNT; i++) {
        sbuf[i] = 1.0 * i + rank;
        rbuf[i] = sbuf[i] * -1;
    }

    MPI_Info_set(info, (char *) "no_any_src_spec_tag", (char *) "true");
    MPI_Comm_dup_with_info(MPI_COMM_WORLD, info, &comm_world);

    MPI_Barrier(comm_world);
    errs = run_test();

  exit:
    if (rank == 0)
        CTEST_report_result(errs);

    if (info != MPI_INFO_NULL)
        MPI_Info_free(&info);
    if (sbuf_win != MPI_WIN_NULL)
        MPI_Win_free(&sbuf_win);
    if (rbuf_win != MPI_WIN_NULL)
        MPI_Win_free(&rbuf_win);
    if (shm_comm != MPI_COMM_NULL)
        MPI_Comm_free(&shm_comm);
    if (comm_world != MPI_COMM_NULL)
        MPI_Comm_free(&comm_world);

    MPI_Finalize();

    return 0;
}

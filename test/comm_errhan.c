/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *
 *  (C) 2016 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include <stdio.h>
#include "mpi.h"
#include "ctest.h"

/*
 *  This test checks communicator-related error handling.
 *  Includes following tests:
 *  - non-RMA calls in non-object/COMM_WORLD/othercomm related calls
 *  - RMA calls:
 *    (1) error in window allocation/creation over COMM_WORLD/othercomm/COMM_NULL
 *    (2) error in RMA sync calls without specified window error handler
 */

#define BUFSZ 2

static int comm_errhan_ncalls = 0, exp_comm_errhan_ncalls = 0;
static MPI_Comm exp_comm = MPI_COMM_WORLD;

static int errs = 0;
static int exp_err_class = 0;   /* Expected error class. Set to 0 if skip check
                                 * (e.g., an MPI implementation may return different
                                 * error code for an internal error.)*/

static int rank, nprocs;
static MPI_Comm errhan_comm = MPI_COMM_NULL;

#define err_printf(str,...) do {          \
    fprintf(stderr, str, ## __VA_ARGS__); \
    fflush(stderr);                       \
} while (0)

#ifdef DEBUG
#define debug_printf(str,...) do {        \
    fprintf(stdout, str, ## __VA_ARGS__); \
    fflush(stdout);                       \
} while (0)
#else
#define debug_printf(str,...)
#endif

#define CHECK_COMM_ERR_FUNC(err_class, ret_class, comm, fnc_stmt)  do {       \
    int mpi_errno = MPI_SUCCESS;                                              \
    exp_err_class = err_class;                                                \
    exp_comm = comm;                                                          \
    exp_comm_errhan_ncalls++;                                                 \
    mpi_errno = fnc_stmt;        /* execute */                                \
    if (ret_class != 0 && compare_err_class(ret_class, mpi_errno)) {          \
        /* return error check */                                              \
        err_printf("Return error class does not match\n");     \
        errs++;                                                \
    }                                                          \
    /* error handler check */                                  \
    if (comm_errhan_ncalls != exp_comm_errhan_ncalls) {        \
        err_printf("Communicator error handler not called\n"); \
        errs++;                                                \
        comm_errhan_ncalls = exp_comm_errhan_ncalls;           \
    }                                                          \
} while (0)

static int compare_err_class(int comp_err_class, int mpi_errno)
{
    int errclass = 0, strlen = 0;
    char errstr[MPI_MAX_ERROR_STRING];

    MPI_Error_class(mpi_errno, &errclass);
    MPI_Error_string(mpi_errno, errstr, &strlen);

    if (comp_err_class != errclass) {
        err_printf("Unexpected error class=%d (%s), expected=%d\n",
                   errclass, errstr, comp_err_class);
        return 1;
    }

    return 0;
}

static void comm_errhan_fnc(MPI_Comm * errcomm, int *err, ...)
{
    if (exp_err_class != 0 && compare_err_class(exp_err_class, *err) != 0) {
        errs++;
        err_printf("In %s: error class does not match\n", __FUNCTION__);
    }

    if (*errcomm != exp_comm) {
        errs++;
        err_printf("In %s: unexpected communicator (got %x expected %x)\n",
                   __FUNCTION__, (int) *errcomm, (int) exp_comm);
    }
    comm_errhan_ncalls++;
    debug_printf("%s called, comm_errhan_ncalls=%d, exp_comm_errhan_ncalls=%d\n",
                 __FUNCTION__, comm_errhan_ncalls, exp_comm_errhan_ncalls);
}

static void check_non_rma(void)
{
    int sbuf[2];
    int err_rank = -1;

    err_rank = CTEST_gen_errrank();

    debug_printf("checking Internal Error with info_create(NULL)...\n");
    CHECK_COMM_ERR_FUNC(0 /* do not check error class */ , 0 /* do no check return code */ ,
                        MPI_COMM_WORLD, (MPI_Info_create(NULL)));

    debug_printf("checking MPI_ERR_RANK with send(MPI_COMM_WORLD)...\n");
    CHECK_COMM_ERR_FUNC(MPI_ERR_RANK, MPI_ERR_RANK, MPI_COMM_WORLD,
                        (MPI_Send(sbuf, 2, MPI_INT, err_rank, 0, MPI_COMM_WORLD)));

    debug_printf("checking MPI_ERR_RANK with send(errhan_comm)...\n");
    CHECK_COMM_ERR_FUNC(MPI_ERR_RANK, MPI_ERR_RANK, errhan_comm,
                        (MPI_Send(sbuf, 2, MPI_INT, err_rank, 0, errhan_comm)));

    debug_printf("checking MPI_ERR_OTHER with call_errhandler(MPI_COMM_WORLD)...\n");
    CHECK_COMM_ERR_FUNC(MPI_ERR_OTHER, MPI_SUCCESS, MPI_COMM_WORLD,
                        (MPI_Comm_call_errhandler(MPI_COMM_WORLD, MPI_ERR_OTHER)));

    debug_printf("checking MPI_ERR_OTHER with call_errhandler(errhan_comm)...\n");
    CHECK_COMM_ERR_FUNC(MPI_ERR_OTHER, MPI_SUCCESS, errhan_comm,
                        (MPI_Comm_call_errhandler(errhan_comm, MPI_ERR_OTHER)));
}

static void check_rma(void)
{
    void *base_ptr = NULL;
    int buf[2];
    MPI_Aint size = 2 * sizeof(int);
    int disp = sizeof(int);
    MPI_Win win = MPI_WIN_NULL;

    debug_printf("checking MPI_ERR_SIZE with win_allocate(MPI_COMM_WORLD)...\n");
    CHECK_COMM_ERR_FUNC(MPI_ERR_SIZE, MPI_ERR_SIZE, MPI_COMM_WORLD,
                        (MPI_Win_allocate(-1, disp, MPI_INFO_NULL,
                                          MPI_COMM_WORLD, &base_ptr, &win)));

    debug_printf("checking MPI_ERR_SIZE with win_create(MPI_COMM_WORLD)...\n");
    CHECK_COMM_ERR_FUNC(MPI_ERR_SIZE, MPI_ERR_SIZE, MPI_COMM_WORLD,
                        (MPI_Win_create(buf, -1, disp, MPI_INFO_NULL, MPI_COMM_WORLD, &win)));

    debug_printf("checking MPI_ERR_SIZE with win_allocate(errhan_comm)...\n");
    CHECK_COMM_ERR_FUNC(MPI_ERR_SIZE, MPI_ERR_SIZE, errhan_comm,
                        (MPI_Win_allocate(-1, disp, MPI_INFO_NULL, errhan_comm, &base_ptr, &win)));

    debug_printf("checking MPI_ERR_SIZE with win_create(errhan_comm)...\n");
    CHECK_COMM_ERR_FUNC(MPI_ERR_SIZE, MPI_ERR_SIZE, errhan_comm,
                        (MPI_Win_create(buf, -1, disp, MPI_INFO_NULL, errhan_comm, &win)));

    debug_printf("checking Internal Error with win_allocate(MPI_COMM_NULL)...\n");
    CHECK_COMM_ERR_FUNC(0 /* do not check error class */ , 0 /* do no check return code */ ,
                        MPI_COMM_WORLD, (MPI_Win_allocate
                                         (size, disp, MPI_INFO_NULL, MPI_COMM_NULL, &base_ptr,
                                          &win)));

    debug_printf("checking Internal Error with win_create(MPI_COMM_NULL)...\n");
    CHECK_COMM_ERR_FUNC(0 /* do not check error class */ , 0 /* do no check return code */ ,
                        MPI_COMM_WORLD, (MPI_Win_create
                                         (buf, size, disp, MPI_INFO_NULL, MPI_COMM_NULL, &win)));

    /* MPI standard does not define whether the COMM_WORLD's error handler is invoked if
     * the user does not specify handler to the error window. Therefore, we should not
     * expect any RMA sync/operation error handled on COMM_WORLD.*/
}

int main(int argc, char *argv[])
{
    MPI_Errhandler errhan;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    MPI_Comm_dup(MPI_COMM_WORLD, &errhan_comm);

    MPI_Comm_create_errhandler(comm_errhan_fnc, &errhan);

    MPI_Comm_set_errhandler(MPI_COMM_WORLD, errhan);
    MPI_Comm_set_errhandler(errhan_comm, errhan);

    check_non_rma();
    check_rma();

    MPI_Errhandler_free(&errhan);
    MPI_Comm_free(&errhan_comm);

    if (rank == 0)
        CTEST_report_result(errs);

    MPI_Finalize();

    return 0;
}

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
 *  This test checks window-related error handling.
 */

#define BUFSZ 2

static int win_errhan_ncalls = 0, exp_win_errhan_ncalls = 0;
static int exp_err_class = MPI_ERR_OTHER;

static int errs = 0;
static MPI_Win win;
static int *baseptr = NULL;
static int rank, nprocs;

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

#define CHECK_WIN_ERR_FUNC(err_class, ret_class, fnc_stmt)  do {     \
    int mpi_errno = MPI_SUCCESS;                                     \
    exp_err_class = err_class;                                       \
    exp_win_errhan_ncalls++;                                         \
    mpi_errno = fnc_stmt;        /* execute */                       \
    if (ret_class != 0 && compare_err_class(ret_class, mpi_errno)) { \
        /* return error check */                                     \
        err_printf("Return error class does not match\n");           \
        errs++;                                                      \
    }                                                          \
    /* error handler check */                                  \
    if (win_errhan_ncalls != exp_win_errhan_ncalls) {          \
        err_printf("Window error handler not called\n");       \
        errs++;                                                \
        win_errhan_ncalls = exp_win_errhan_ncalls;             \
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


static void win_errhan_fnc(MPI_Win * errwin, int *err, ...)
{
    if (exp_err_class != 0 && compare_err_class(exp_err_class, *err) != 0) {
        errs++;
        err_printf("In %s: error class does not match\n", __FUNCTION__);
    }

    if (*errwin != win) {
        errs++;
        err_printf("In %s: unexpected window (got %x expected %x)\n",
                   __FUNCTION__, (int) *errwin, (int) win);
    }
    win_errhan_ncalls++;
}

static void check_rma_sync(void)
{
    int origin_rank = 0, target_rank = 1;
    int assert = 0;
    int lock_type = MPI_LOCK_SHARED;
    int err_rank = -1;
    MPI_Group pscw_group = MPI_GROUP_NULL;

    MPI_Comm_group(MPI_COMM_WORLD, &pscw_group);

    err_rank = CTEST_gen_errrank();

    if (rank == origin_rank) {
        /* --- Checking invalid parameters --- */
#ifdef CHECK_ALL
        debug_printf("checking ERR_LOCKTYPE in RMA SYNC...\n");
        CHECK_WIN_ERR_FUNC(MPI_ERR_LOCKTYPE, MPI_ERR_LOCKTYPE,
                           (MPI_Win_lock(-1, target_rank, assert, win)));
#endif
        debug_printf("checking MPI_ERR_RANK in RMA SYNC...\n");
        CHECK_WIN_ERR_FUNC(MPI_ERR_RANK, MPI_ERR_RANK,
                           (MPI_Win_lock(lock_type, err_rank, assert, win)));

        /* - skip MPI_ERR_ASSERT check because it is implementation dependent. */
        /* - skip MPI_ERR_WIN check because it is not handled by window error handler. */

        /* - MPI_ERR_RMA_SYNC */
        /* -- nested access epochs */
        debug_printf("checking MPI_ERR_RMA_SYNC in RMA SYNC (nested access epoch)...\n");
        MPI_Win_lock_all(assert, win);  /* open epoch */
        CHECK_WIN_ERR_FUNC(MPI_ERR_RMA_SYNC, MPI_ERR_RMA_SYNC, (MPI_Win_lock_all(assert, win)));
        MPI_Win_unlock_all(win);        /* close epoch */

        MPI_Win_lock_all(assert, win);  /* open epoch */
        CHECK_WIN_ERR_FUNC(MPI_ERR_RMA_SYNC, MPI_ERR_RMA_SYNC,
                           (MPI_Win_lock(lock_type, target_rank, assert, win)));
        MPI_Win_unlock_all(win);        /* close epoch */

        MPI_Win_lock_all(assert, win);  /* open epoch */
        CHECK_WIN_ERR_FUNC(MPI_ERR_RMA_SYNC, MPI_ERR_RMA_SYNC,
                           (MPI_Win_start(pscw_group, assert, win)));
        MPI_Win_unlock_all(win);        /* close epoch */

        MPI_Win_lock_all(assert, win);  /* open epoch */
        CHECK_WIN_ERR_FUNC(MPI_ERR_RMA_SYNC, MPI_ERR_RMA_SYNC, (MPI_Win_fence(assert, win)));
        MPI_Win_unlock_all(win);        /* close epoch */

        /* -- no opening access epochs */
        debug_printf("checking MPI_ERR_RMA_SYNC in RMA SYNC (no opening access epoch)...\n");
        CHECK_WIN_ERR_FUNC(MPI_ERR_RMA_SYNC, MPI_ERR_RMA_SYNC, (MPI_Win_complete(win)));
        CHECK_WIN_ERR_FUNC(MPI_ERR_RMA_SYNC, MPI_ERR_RMA_SYNC, (MPI_Win_unlock(target_rank, win)));
        CHECK_WIN_ERR_FUNC(MPI_ERR_RMA_SYNC, MPI_ERR_RMA_SYNC, (MPI_Win_unlock_all(win)));
        CHECK_WIN_ERR_FUNC(MPI_ERR_RMA_SYNC, MPI_ERR_RMA_SYNC, (MPI_Win_flush(target_rank, win)));
        CHECK_WIN_ERR_FUNC(MPI_ERR_RMA_SYNC, MPI_ERR_RMA_SYNC,
                           (MPI_Win_flush_local(target_rank, win)));
        CHECK_WIN_ERR_FUNC(MPI_ERR_RMA_SYNC, MPI_ERR_RMA_SYNC, (MPI_Win_flush_all(win)));
        CHECK_WIN_ERR_FUNC(MPI_ERR_RMA_SYNC, MPI_ERR_RMA_SYNC, (MPI_Win_flush_local_all(win)));

        /* -- no opening exposure epochs */
        debug_printf("checking MPI_ERR_RMA_SYNC in RMA SYNC (no opening exposure epoch)...\n");
        CHECK_WIN_ERR_FUNC(MPI_ERR_RMA_SYNC, MPI_ERR_RMA_SYNC, (MPI_Win_wait(win)));
    }

    MPI_Group_free(&pscw_group);
}

static void check_rma_op(void)
{
    int origin_rank = 0, target_rank = 1;
    int locbuf[BUFSZ], resbuf[BUFSZ], compbuf[BUFSZ];
    MPI_Aint target_disp = 0;
    int count = BUFSZ;
    MPI_Op op = MPI_SUM;
    int err_rank = -1;

    err_rank = CTEST_gen_errrank();

    MPI_Win_fence(0, win);      /* open epoch */

    if (rank == origin_rank) {
        debug_printf("checking MPI_ERR_RANK in RMA OP...\n");
        CHECK_WIN_ERR_FUNC(MPI_ERR_RANK, MPI_ERR_RANK,
                           (MPI_Put
                            (locbuf, count, MPI_INT, err_rank, target_disp, count, MPI_INT, win)));
        CHECK_WIN_ERR_FUNC(MPI_ERR_RANK, MPI_ERR_RANK,
                           (MPI_Get
                            (locbuf, count, MPI_INT, err_rank, target_disp, count, MPI_INT, win)));
        CHECK_WIN_ERR_FUNC(MPI_ERR_RANK, MPI_ERR_RANK,
                           (MPI_Accumulate
                            (locbuf, count, MPI_INT, err_rank, target_disp, count, MPI_INT, op,
                             win)));
        CHECK_WIN_ERR_FUNC(MPI_ERR_RANK, MPI_ERR_RANK,
                           (MPI_Get_accumulate
                            (locbuf, count, MPI_INT, resbuf, count, MPI_INT, err_rank, target_disp,
                             count, MPI_INT, op, win)));
        CHECK_WIN_ERR_FUNC(MPI_ERR_RANK, MPI_ERR_RANK,
                           (MPI_Fetch_and_op
                            (locbuf, resbuf, MPI_INT, err_rank, target_disp, op, win)));
        CHECK_WIN_ERR_FUNC(MPI_ERR_RANK, MPI_ERR_RANK,
                           (MPI_Compare_and_swap
                            (locbuf, compbuf, resbuf, MPI_INT, err_rank, target_disp, win)));

        /* - skip MPI_ERR_OP check because it is hard to find the value of an invalid op. */

        debug_printf("checking MPI_ERR_COUNT in RMA OP...\n");
        CHECK_WIN_ERR_FUNC(MPI_ERR_COUNT, MPI_ERR_COUNT,
                           (MPI_Put(locbuf, -1, MPI_INT, target_rank, target_disp, -1,
                                    MPI_INT, win)));
        CHECK_WIN_ERR_FUNC(MPI_ERR_COUNT, MPI_ERR_COUNT,
                           (MPI_Get(locbuf, -1, MPI_INT, target_rank, target_disp, -1,
                                    MPI_INT, win)));
        CHECK_WIN_ERR_FUNC(MPI_ERR_COUNT, MPI_ERR_COUNT,
                           (MPI_Accumulate(locbuf, -1, MPI_INT, target_rank, target_disp, -1,
                                           MPI_INT, op, win)));
        CHECK_WIN_ERR_FUNC(MPI_ERR_COUNT, MPI_ERR_COUNT,
                           (MPI_Get_accumulate(locbuf, -1, MPI_INT, resbuf, -1,
                                               MPI_INT, target_rank, target_disp, -1,
                                               MPI_INT, op, win)));

        debug_printf("checking MPI_ERR_DISP in RMA OP...\n");
        CHECK_WIN_ERR_FUNC(MPI_ERR_DISP, MPI_ERR_DISP,
                           (MPI_Put(locbuf, count, MPI_INT, target_rank, -1, count, MPI_INT, win)));
        CHECK_WIN_ERR_FUNC(MPI_ERR_DISP, MPI_ERR_DISP,
                           (MPI_Get(locbuf, count, MPI_INT, target_rank, -1, count, MPI_INT, win)));
        CHECK_WIN_ERR_FUNC(MPI_ERR_DISP, MPI_ERR_DISP,
                           (MPI_Accumulate(locbuf, count, MPI_INT,
                                           target_rank, -1, count, MPI_INT, op, win)));
        CHECK_WIN_ERR_FUNC(MPI_ERR_DISP, MPI_ERR_DISP,
                           (MPI_Get_accumulate(locbuf, count, MPI_INT, resbuf, count, MPI_INT,
                                               target_rank, -1, count, MPI_INT, op, win)));
        CHECK_WIN_ERR_FUNC(MPI_ERR_DISP, MPI_ERR_DISP,
                           (MPI_Fetch_and_op(locbuf, resbuf, MPI_INT, target_rank, -1, op, win)));
        CHECK_WIN_ERR_FUNC(MPI_ERR_DISP, MPI_ERR_DISP,
                           (MPI_Compare_and_swap(locbuf, compbuf, resbuf, MPI_INT,
                                                 target_rank, -1, win)));
    }

    MPI_Win_fence(0, win);      /* close epoch */
}

static void check_call_errhan(void)
{
    debug_printf("checking call_errhandler...\n");
    CHECK_WIN_ERR_FUNC(MPI_ERR_OTHER, MPI_SUCCESS, (MPI_Win_call_errhandler(win, MPI_ERR_OTHER)));
}

int main(int argc, char *argv[])
{
    MPI_Errhandler win_errhan = MPI_ERRHANDLER_NULL;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    if (nprocs < 2) {
        err_printf("Please run using at least 2 processes\n");
        goto exit;
    }

    MPI_Win_allocate(BUFSZ * sizeof(int), sizeof(int), MPI_INFO_NULL, MPI_COMM_WORLD, &baseptr,
                     &win);
    MPI_Win_create_errhandler(win_errhan_fnc, &win_errhan);
    MPI_Win_set_errhandler(win, win_errhan);

    check_rma_sync();
    MPI_Barrier(MPI_COMM_WORLD);

    check_rma_op();
    MPI_Barrier(MPI_COMM_WORLD);

    check_call_errhan();
    MPI_Barrier(MPI_COMM_WORLD);

  exit:
    if (rank == 0)
        CTEST_report_result(errs);

    MPI_Win_free(&win);
    MPI_Errhandler_free(&win_errhan);
    MPI_Finalize();

    return 0;
}

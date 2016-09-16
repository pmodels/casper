/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

static int fetch_and_op_impl(const void *origin_addr, void *result_addr,
                             MPI_Datatype datatype, int target_rank, MPI_Aint target_disp,
                             MPI_Op op, CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint ug_target_disp = 0;
    int rank;
    CSPU_win_target_t *target = NULL;

    /* If target is MPI_PROC_NULL, operation succeeds and returns as soon as possible. */
    if (target_rank == MPI_PROC_NULL)
        goto fn_exit;

    CSPU_TARGET_CHECK_RANK(target_rank, ug_win);

    PMPI_Comm_rank(ug_win->user_comm, &rank);
    target = &(ug_win->targets[target_rank]);

    CSPU_TARGET_CHECK_OP_EPOCH(target, ug_win);

    /* Should not do local RMA in accumulate because of atomicity issue */

    /* Redirect operation to ghost process.
     * (See discussion of optimization for intra-node operations in csp.h.) */
    int target_g_rank_in_ug = -1;
    int data_size CSP_ATTRIBUTE((unused)) = 0;
    MPI_Aint target_g_offset = 0;
    MPI_Win *win_ptr = NULL;

    CSPU_TARGET_GET_EPOCH_WIN(target, ug_win, win_ptr);

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    if (CSP_ENV.load_opt == CSP_LOAD_BYTE_COUNTING) {
        PMPI_Type_size(datatype, &data_size);
    }
#endif
    mpi_errno = CSPU_target_get_ghost(target_rank, 1, data_size, ug_win,
                                      &target_g_rank_in_ug, &target_g_offset);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    ug_target_disp = target_g_offset + target->disp_unit * target_disp;

    /* Issue operation to the ghost process in corresponding ug-window of target process. */
    mpi_errno = PMPI_Fetch_and_op(origin_addr, result_addr, datatype, target_g_rank_in_ug,
                                  ug_target_disp, op, *win_ptr);

    CSP_DBG_PRINT("CASPER Fetch_and_op to (ghost %d, win 0x%x [%s]) instead of "
                  "target %d, 0x%lx(0x%lx + %d * %ld)\n",
                  target_g_rank_in_ug, *win_ptr,
                  CSPU_TARGET_GET_EPOCH_STAT_NAME(target, ug_win),
                  target_rank, ug_target_disp, target_g_offset, target->disp_unit, target_disp);

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Fetch_and_op(const void *origin_addr, void *result_addr,
                     MPI_Datatype datatype, int target_rank, MPI_Aint target_disp,
                     MPI_Op op, MPI_Win win)
{
    int mpi_errno = MPI_SUCCESS;
    CSPU_win_t *ug_win;

    CSPU_fetch_ug_win_from_cache(win, &ug_win);

    if (ug_win) {
        /* casper window */
        mpi_errno = fetch_and_op_impl(origin_addr, result_addr, datatype, target_rank,
                                      target_disp, op, ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
    else {
        /* normal window */
        return PMPI_Fetch_and_op(origin_addr, result_addr, datatype, target_rank,
                                 target_disp, op, win);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    CSPU_WIN_ERROR_RETURN(ug_win, &mpi_errno);
    goto fn_exit;
}

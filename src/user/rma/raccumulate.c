/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

static inline int raccumualte_proc_null_impl(const void *origin_addr, int origin_count,
                                             MPI_Datatype origin_datatype,
                                             int target_rank, MPI_Aint target_disp,
                                             int target_count, MPI_Datatype target_datatype,
                                             MPI_Op op, CSPU_win_t * ug_win, MPI_Request * request)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Win *win_ptr = NULL;
    CSPU_win_target_t *target = NULL;

    target = &(ug_win->targets[target_rank]);

    /* We cannot create MPI_Request and complete it here, thus we simply pass to MPI
     * through an window owned by a random target.*/
    CSPU_TARGET_GET_EPOCH_WIN(target, ug_win, win_ptr);

    CSP_CALLMPI(NOSTMT, PMPI_Raccumulate(origin_addr, origin_count, origin_datatype,
                                         target_rank, target_disp, target_count,
                                         target_datatype, op, *win_ptr, request));
    return mpi_errno;
}

static int raccumulate_impl(const void *origin_addr, int origin_count,
                            MPI_Datatype origin_datatype,
                            int target_rank, MPI_Aint target_disp, int target_count,
                            MPI_Datatype target_datatype, MPI_Op op,
                            CSPU_win_t * ug_win, MPI_Request * request)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint ug_target_disp = 0;
    int target_g_rank_in_ug = -1;
    int data_size CSP_ATTRIBUTE((unused)) = 0;
    MPI_Aint target_g_offset = 0;
    int rank;
    CSPU_win_target_t *target = NULL;
    MPI_Win *win_ptr = NULL;

    if (target_rank == MPI_PROC_NULL) {
        mpi_errno = raccumualte_proc_null_impl(origin_addr, origin_count,
                                               origin_datatype, target_rank, target_disp,
                                               target_count, target_datatype, op, ug_win, request);
        goto fn_exit;
    }

    CSPU_TARGET_CHECK_RANK(target_rank, ug_win);

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->user_comm, &rank));
    target = &(ug_win->targets[target_rank]);

    CSPU_TARGET_CHECK_OP_EPOCH(target, ug_win);
    CSPU_TARGET_CHECK_OP_DISP(target_disp, target);

    /* Should not do local RMA in accumulate because of atomicity issue */

    /* Redirect operation to ghost process.
     * (See discussion of optimization for intra-node operations in csp.h.) */

    CSPU_TARGET_GET_EPOCH_WIN(target, ug_win, win_ptr);

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    if (CSP_ENV.load_opt == CSP_LOAD_BYTE_COUNTING) {
        CSP_CALLMPI(JUMP, PMPI_Type_size(origin_datatype, &data_size));
        data_size *= origin_count;
    }
#endif
    mpi_errno = CSPU_target_get_ghost(target_rank, 1, data_size, ug_win,
                                      &target_g_rank_in_ug, &target_g_offset);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    ug_target_disp = target_g_offset + target->disp_unit * target_disp;

    /* Issue operation to the ghost process in corresponding ug-window of target process. */
    CSP_CALLMPI(JUMP, PMPI_Raccumulate(origin_addr, origin_count, origin_datatype,
                                       target_g_rank_in_ug, ug_target_disp,
                                       target_count, target_datatype, op, *win_ptr, request));

    CSP_DBG_PRINT("CASPER Raccumulate to (ghost %d, win 0x%x [%s]) instead of "
                  "target %d, 0x%lx(0x%lx + %d * %ld)\n",
                  target_g_rank_in_ug, *win_ptr,
                  CSPU_TARGET_GET_EPOCH_STAT_NAME(target, ug_win),
                  target_rank, ug_target_disp, target_g_offset, target->disp_unit, target_disp);


  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Raccumulate(const void *origin_addr, int origin_count,
                    MPI_Datatype origin_datatype,
                    int target_rank, MPI_Aint target_disp,
                    int target_count, MPI_Datatype target_datatype, MPI_Op op,
                    MPI_Win win, MPI_Request * request)
{
    int mpi_errno = MPI_SUCCESS;
    CSPU_win_t *ug_win;

    CSPU_fetch_ug_win_from_cache(win, &ug_win);

    if (ug_win) {
        /* casper window */
        CSPU_THREAD_OBJ_CS_LOCAL_DCL();
        CSPU_THREAD_ENTER_OBJ_CS(ug_win);
        mpi_errno = raccumulate_impl(origin_addr, origin_count,
                                     origin_datatype, target_rank, target_disp, target_count,
                                     target_datatype, op, ug_win, request);
        CSPU_THREAD_EXIT_OBJ_CS(ug_win);

        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }
    else {
        /* normal window */
        return PMPI_Raccumulate(origin_addr, origin_count,
                                origin_datatype, target_rank, target_disp, target_count,
                                target_datatype, op, win, request);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    CSPU_WIN_ERROR_RETURN(ug_win, &mpi_errno);
    goto fn_exit;
}

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

static int compare_and_swap_impl(const void *origin_addr, const void *compare_addr,
                                 void *result_addr, MPI_Datatype datatype, int target_rank,
                                 MPI_Aint target_disp, CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint ug_target_disp = 0;
    int target_g_rank_in_ug = -1;
    int data_size CSP_ATTRIBUTE((unused)) = 0;
    MPI_Aint target_g_offset = 0;
    int rank;
    CSPU_win_target_t *target = NULL;
    MPI_Win *win_ptr = NULL;

    if (target_rank == MPI_PROC_NULL)
        goto fn_exit;

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
        CSP_CALLMPI(JUMP, PMPI_Type_size(datatype, &data_size));
    }
#endif
    mpi_errno = CSPU_target_get_ghost(target_rank, 1, data_size, ug_win,
                                      &target_g_rank_in_ug, &target_g_offset);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    ug_target_disp = target_g_offset + target->disp_unit * target_disp;

    /* Issue operation to the ghost process in corresponding ug-window of target process. */
    CSP_CALLMPI(JUMP, PMPI_Compare_and_swap(origin_addr, compare_addr, result_addr,
                                            datatype, target_g_rank_in_ug, ug_target_disp,
                                            *win_ptr));

    CSP_DBG_PRINT("CASPER Compare_and_swap to (ghost %d, win 0x%x [%s]) instead of "
                  "target %d, 0x%lx(0x%lx + %d * %ld)\n",
                  target_g_rank_in_ug, *win_ptr,
                  CSPU_TARGET_GET_EPOCH_STAT_NAME(target, ug_win),
                  target_rank, ug_target_disp, target_g_offset, target->disp_unit, target_disp);

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Compare_and_swap(const void *origin_addr, const void *compare_addr,
                         void *result_addr, MPI_Datatype datatype, int target_rank,
                         MPI_Aint target_disp, MPI_Win win)
{
    int mpi_errno = MPI_SUCCESS;
    CSPU_win_t *ug_win;

    CSPU_ERRHAN_EXTOBJ_LOCAL_DCL();
    CSPU_WIN_ERRHAN_SET_EXTOBJ();

    CSPU_fetch_ug_win_from_cache(win, &ug_win);

    if (ug_win) {
        /* casper window */
        CSPU_THREAD_OBJ_CS_LOCAL_DCL();
        CSPU_THREAD_ENTER_OBJ_CS(ug_win);
        mpi_errno = compare_and_swap_impl(origin_addr, compare_addr, result_addr,
                                          datatype, target_rank, target_disp, ug_win);
        CSPU_THREAD_EXIT_OBJ_CS(ug_win);

        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }
    else {
        /* normal window */
        CSPU_ERRHAN_RESET_EXTOBJ();     /* reset before calling original MPI */
        return PMPI_Compare_and_swap(origin_addr, compare_addr, result_addr,
                                     datatype, target_rank, target_disp, win);
    }

  fn_exit:
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before return */
    return mpi_errno;

  fn_fail:
    CSPU_ERRHAN_RESET_EXTOBJ(); /* reset before error handling */
    CSPU_WIN_ERRHANLDING(win, &mpi_errno);
    goto fn_exit;
}

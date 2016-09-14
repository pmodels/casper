/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

#ifdef CSP_ENABLE_LOCAL_RMA_OP_OPT
static int put_shared_impl(const void *origin_addr, int origin_count,
                           MPI_Datatype origin_datatype,
                           int target_rank, MPI_Aint target_disp,
                           int target_count, MPI_Datatype target_datatype, CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Win *win_ptr = NULL;
    CSPU_win_target_t *target = NULL;

    target = &(ug_win->targets[target_rank]);
    CSPU_TARGET_GET_EPOCH_WIN(target, ug_win, win_ptr);

    /* Issue operation to the target through local shared window, because shared
     * communication is fully handled by local process.
     */
    mpi_errno = PMPI_Put(origin_addr, origin_count, origin_datatype,
                         ug_win->my_rank_in_ug_comm, target_disp,
                         target_count, target_datatype, *win_ptr);
    CSP_DBG_PRINT("CASPER PUT to self(%d, in local win 0x%x)\n",
                  ug_win->my_rank_in_ug_comm, *win_ptr);

    return mpi_errno;
}
#endif

static int put_impl(const void *origin_addr, int origin_count,
                    MPI_Datatype origin_datatype,
                    int target_rank, MPI_Aint target_disp,
                    int target_count, MPI_Datatype target_datatype, CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint ug_target_disp = 0;
    int rank;
    CSPU_win_target_t *target = NULL;

    /* If target is MPI_PROC_NULL, operation succeeds and returns as soon as possible. */
    if (target_rank == MPI_PROC_NULL)
        goto fn_exit;

    PMPI_Comm_rank(ug_win->user_comm, &rank);
    target = &(ug_win->targets[target_rank]);

#ifdef CSP_ENABLE_EPOCH_STAT_CHECK
    CSPU_TARGET_CHECK_EPOCH_PER_OP(target, ug_win);
#endif

#ifdef CSP_ENABLE_LOCAL_RMA_OP_OPT
    if (target_rank == rank && ug_win->is_self_locked) {
        mpi_errno = put_shared_impl(origin_addr, origin_count,
                                    origin_datatype, target_rank, target_disp, target_count,
                                    target_datatype, ug_win);
        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;
    }
    else
#endif
    {
        /* Redirect operation to ghost process.
         * (See discussion of optimization for intra-node operations in csp.h.) */
        int target_g_rank_in_ug = -1;
        int data_size CSP_ATTRIBUTE((unused)) = 0;
        MPI_Aint target_g_offset = 0;
        MPI_Win *win_ptr = NULL;

        CSPU_TARGET_GET_EPOCH_WIN(target, ug_win, win_ptr);

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
        if (CSP_ENV.load_opt == CSP_LOAD_BYTE_COUNTING) {
            PMPI_Type_size(origin_datatype, &data_size);
            data_size *= origin_count;
        }
#endif
        mpi_errno = CSPU_target_get_ghost(target_rank, 0, data_size, ug_win,
                                          &target_g_rank_in_ug, &target_g_offset);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        ug_target_disp = target_g_offset + target->disp_unit * target_disp;

        /* Issue operation to the ghost process in corresponding ug-window of target process. */
        mpi_errno = PMPI_Put(origin_addr, origin_count, origin_datatype,
                             target_g_rank_in_ug, ug_target_disp,
                             target_count, target_datatype, *win_ptr);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        CSP_DBG_PRINT("CASPER Put to (ghost %d, win 0x%x [%s]) instead of "
                      "target %d, 0x%lx(0x%lx + %d * %ld)\n",
                      target_g_rank_in_ug, *win_ptr,
                      CSPU_TARGET_GET_EPOCH_STAT_NAME(target, ug_win),
                      target_rank, ug_target_disp, target_g_offset, target->disp_unit, target_disp);
    }
  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Put(const void *origin_addr, int origin_count,
            MPI_Datatype origin_datatype,
            int target_rank, MPI_Aint target_disp,
            int target_count, MPI_Datatype target_datatype, MPI_Win win)
{
    int mpi_errno = MPI_SUCCESS;
    CSPU_win_t *ug_win;

    CSPU_fetch_ug_win_from_cache(win, &ug_win);

    if (ug_win) {
        /* casper window */
        mpi_errno = put_impl(origin_addr, origin_count,
                             origin_datatype, target_rank, target_disp, target_count,
                             target_datatype, ug_win);
    }
    else {
        /* normal window */
        mpi_errno = PMPI_Put(origin_addr, origin_count, origin_datatype, target_rank,
                             target_disp, target_count, target_datatype, win);
    }

    return mpi_errno;
}

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

static int CSP_raccumulate_segment_impl(const void *origin_addr, int origin_count,
                                        MPI_Datatype origin_datatype, int target_rank,
                                        MPI_Aint target_disp,
                                        int target_count, MPI_Datatype target_datatype,
                                        MPI_Op op, CSP_win * ug_win, MPI_Request * request)
{
    int mpi_errno = MPI_SUCCESS;
    int num_segs = 0, i;
    CSP_op_segment *decoded_ops = NULL;
    CSP_win_target *target = NULL;

    target = &(ug_win->targets[target_rank]);

    /* TODO : Eliminate operation division for some special cases, see pptx */
    mpi_errno = CSP_op_segments_decode(origin_addr, origin_count,
                                       origin_datatype, target_rank, target_disp, target_count,
                                       target_datatype, ug_win, &decoded_ops, &num_segs);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    CSP_DBG_PRINT("CASPER Raccumulate to target %d, num_segs=%d\n", target_rank, num_segs);

    for (i = 0; i < num_segs; i++) {
        int target_g_rank_in_ug = -1;
        MPI_Aint target_g_offset = 0;
        MPI_Aint ug_target_disp = 0;
        int seg_off = decoded_ops[i].target_seg_off;
        MPI_Win seg_ug_win = target->segs[seg_off].ug_win;

        mpi_errno = CSP_target_get_ghost(target_rank, seg_off, 1, decoded_ops[i].target_dtsize,
                                         ug_win, &target_g_rank_in_ug, &target_g_offset);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        ug_target_disp = target_g_offset + target->disp_unit * decoded_ops[i].target_disp;

        /* FIXME: handle multiple requests.
         * Even accumulates are ordered, but divided operations may be issued to different ghosts,
         * thus still need to handle multiple requests.*/
        CSP_assert(i > 1);

        /* Issue operation to the ghost process in corresponding ug-window of target process. */
        mpi_errno = PMPI_Raccumulate(decoded_ops[i].origin_addr, decoded_ops[i].origin_count,
                                     decoded_ops[i].origin_datatype, target_g_rank_in_ug,
                                     ug_target_disp, decoded_ops[i].target_count,
                                     decoded_ops[i].target_datatype, op, seg_ug_win, request);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        CSP_DBG_PRINT("CASPER Raccumulate to (ghost %d, win 0x%x) instead of "
                      "target %d, seg %d \n"
                      "(origin.addr %p, count %d, datatype 0x%x, "
                      "target.disp 0x%lx(0x%lx + %d * %ld), count %d, datatype 0x%x)\n",
                      target_g_rank_in_ug, seg_ug_win, target_rank, seg_off,
                      decoded_ops[i].origin_addr, decoded_ops[i].origin_count,
                      decoded_ops[i].origin_datatype, ug_target_disp, target_g_offset,
                      target->disp_unit, decoded_ops[i].target_disp,
                      decoded_ops[i].target_count, decoded_ops[i].target_datatype);
    }

  fn_exit:
    CSP_op_segments_destroy(&decoded_ops);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static inline int CSP_proc_null_raccumualte_impl(const void *origin_addr, int origin_count,
                                                 MPI_Datatype origin_datatype,
                                                 int target_rank, MPI_Aint target_disp,
                                                 int target_count, MPI_Datatype target_datatype,
                                                 MPI_Op op, CSP_win * ug_win, MPI_Request * request)
{
    MPI_Win *win_ptr = NULL;
    CSP_win_target *target = NULL;

    target = &(ug_win->targets[target_rank]);

    /* We cannot create MPI_Request and complete it here, thus we simply pass to MPI
     * through an window owned by a random target.*/
    CSP_target_get_epoch_win(0, target, ug_win, win_ptr);

    return PMPI_Raccumulate(origin_addr, origin_count, origin_datatype,
                            target_rank, target_disp, target_count,
                            target_datatype, op, *win_ptr, request);
}

static int CSP_raccumulate_impl(const void *origin_addr, int origin_count,
                                MPI_Datatype origin_datatype,
                                int target_rank, MPI_Aint target_disp, int target_count,
                                MPI_Datatype target_datatype, MPI_Op op,
                                CSP_win * ug_win, MPI_Request * request)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint ug_target_disp = 0;
    int rank;
    CSP_win_target *target = NULL;

    if (target_rank == MPI_PROC_NULL) {
        mpi_errno = CSP_proc_null_raccumualte_impl(origin_addr, origin_count,
                                                   origin_datatype, target_rank, target_disp,
                                                   target_count, target_datatype, op,
                                                   ug_win, request);
        goto fn_exit;
    }

    PMPI_Comm_rank(ug_win->user_comm, &rank);
    target = &(ug_win->targets[target_rank]);

#ifdef CSP_ENABLE_EPOCH_STAT_CHECK
    CSP_target_check_epoch_per_op(target, ug_win);
#endif

    /* Should not do local RMA in accumulate because of atomicity issue */

    /* TODO: Do we need segment load balancing in fence ?
     * 1. No lock issue.
     * 2. overhead of data range checking and division */
    if (CSP_ENV.lock_binding == CSP_LOCK_BINDING_SEGMENT &&
        target->num_segs > 1 && (ug_win->epoch_stat == CSP_WIN_EPOCH_LOCK_ALL ||
                                 target->epoch_stat == CSP_TARGET_EPOCH_LOCK)) {
        mpi_errno = CSP_raccumulate_segment_impl(origin_addr, origin_count,
                                                 origin_datatype, target_rank, target_disp,
                                                 target_count, target_datatype, op,
                                                 ug_win, request);
        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;
    }
    else {
        /* Translation for intra/inter-node operations.
         *
         * We do not use force flush + shared window for optimizing operations to local targets.
         * Because: 1) we lose lock optimization on force flush; 2) Although most implementation
         * does shared-communication for operations on shared windows, MPI standard doesn’t
         * require it. Some implementation may use network even for shared targets for
         * shorter CPU occupancy.
         */
        int target_g_rank_in_ug = -1;
        int data_size CSP_ATTRIBUTE((unused)) = 0;
        MPI_Aint target_g_offset = 0;
        MPI_Win *win_ptr = NULL;

        CSP_target_get_epoch_win(0, target, ug_win, win_ptr);

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
        if (CSP_ENV.load_opt == CSP_LOAD_BYTE_COUNTING) {
            PMPI_Type_size(origin_datatype, &data_size);
            data_size *= origin_count;
        }
#endif
        mpi_errno = CSP_target_get_ghost(target_rank, 0, 1, data_size, ug_win,
                                         &target_g_rank_in_ug, &target_g_offset);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        ug_target_disp = target_g_offset + target->disp_unit * target_disp;

        /* Issue operation to the ghost process in corresponding ug-window of target process. */
        mpi_errno = PMPI_Raccumulate(origin_addr, origin_count, origin_datatype,
                                     target_g_rank_in_ug, ug_target_disp,
                                     target_count, target_datatype, op, *win_ptr, request);

        CSP_DBG_PRINT("CASPER Raccumulate to (ghost %d, win 0x%x [%s]) instead of "
                      "target %d, 0x%lx(0x%lx + %d * %ld)\n",
                      target_g_rank_in_ug, *win_ptr,
                      CSP_target_get_epoch_stat_name(target, ug_win),
                      target_rank, ug_target_disp, target_g_offset, target->disp_unit, target_disp);
    }

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
    CSP_win *ug_win;

    CSP_DBG_PRINT_FCNAME();

    CSP_fetch_ug_win_from_cache(win, ug_win);

    if (ug_win) {
        /* casper window */
        mpi_errno = CSP_raccumulate_impl(origin_addr, origin_count,
                                         origin_datatype, target_rank, target_disp, target_count,
                                         target_datatype, op, ug_win, request);
    }
    else {
        /* normal window */
        mpi_errno = PMPI_Raccumulate(origin_addr, origin_count,
                                     origin_datatype, target_rank, target_disp, target_count,
                                     target_datatype, op, win, request);
    }

    return mpi_errno;
}

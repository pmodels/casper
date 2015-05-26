#include <stdio.h>
#include <stdlib.h>
#include "csp.h"

static int CSP_Fetch_and_op_segment_impl(const void *origin_addr, void *result_addr,
                                         MPI_Datatype datatype, int target_rank,
                                         MPI_Aint target_disp, MPI_Op op, MPI_Win win,
                                         CSP_Win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int num_segs = 0;
    CSP_OP_Segment *decoded_ops = NULL;

    /* TODO : Eliminate operation division for some special cases, see pptx */
    /* Fetch_and_op only allows predefined datatype. */
    mpi_errno = CSP_Op_segments_decode_basic_datatype(origin_addr, 1,
                                                      datatype, target_rank, target_disp, 1,
                                                      datatype, ug_win, &decoded_ops, &num_segs);

    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    CSP_DBG_PRINT("CASPER Fetch_and_op to target %d, num_segs=%d\n", target_rank, num_segs);

    int target_g_rank_in_ug = -1;
    MPI_Aint target_g_offset = 0;
    MPI_Aint ug_target_disp = 0;
    int seg_off = decoded_ops[0].target_seg_off;
    MPI_Win seg_ug_win = ug_win->targets[target_rank].segs[seg_off].ug_win;

    mpi_errno = CSP_Get_gp_rank(target_rank, seg_off, 1, decoded_ops[0].target_dtsize,
                                ug_win, &target_g_rank_in_ug, &target_g_offset);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Fetch_and_op only allows one predefined element which must be contained by
     * a single segmetn, thus we only need translate target displacement according to
     * its segment id. */
    ug_target_disp = target_g_offset
        + ug_win->targets[target_rank].disp_unit * decoded_ops[0].target_disp;

    mpi_errno = PMPI_Fetch_and_op(origin_addr, result_addr, datatype,
                                  target_g_rank_in_ug, ug_target_disp, op, seg_ug_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    CSP_DBG_PRINT("CASPER Fetch_and_op to (ghost %d, win 0x%x) instead of "
                  "target %d, seg %d \n"
                  "(origin.addr %p, count %d, datatype 0x%x, "
                  "target.disp 0x%lx(0x%lx + %d * %ld), count %d, datatype 0x%x)\n",
                  target_g_rank_in_ug, seg_ug_win, target_rank, seg_off,
                  decoded_ops[0].origin_addr, decoded_ops[0].origin_count,
                  decoded_ops[0].origin_datatype, ug_target_disp, target_g_offset,
                  ug_win->targets[target_rank].disp_unit, decoded_ops[0].target_disp,
                  decoded_ops[0].target_count, decoded_ops[0].target_datatype);

  fn_exit:
    CSP_Op_segments_destroy(&decoded_ops);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}


static int CSP_Fetch_and_op_impl(const void *origin_addr, void *result_addr,
                                 MPI_Datatype datatype, int target_rank, MPI_Aint target_disp,
                                 MPI_Op op, MPI_Win win, CSP_Win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint ug_target_disp = 0;
    int rank;

    PMPI_Comm_rank(ug_win->user_comm, &rank);

    /* Should not do local RMA in accumulate because of atomicity issue */

    /* TODO: Do we need segment load balancing in fence ?
     * 1. No lock issue.
     * 2. overhead of data range checking and division */

    /* Although only one predefined datatype element is transferred in such
     * operation, we still need call segmentation routine to get its the segment
     * number if target is divided to multiple segments. */
    if (CSP_ENV.lock_binding == CSP_LOCK_BINDING_SEGMENT &&
        ug_win->targets[target_rank].num_segs > 1 && ug_win->epoch_stat == CSP_WIN_EPOCH_LOCK) {
        mpi_errno = CSP_Fetch_and_op_segment_impl(origin_addr, result_addr,
                                                  datatype, target_rank, target_disp, op,
                                                  win, ug_win);
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
        int data_size ATTRIBUTE((unused)) = 0;
        MPI_Aint target_g_offset = 0;
        MPI_Win *win_ptr = NULL;

        CSP_Get_epoch_win(target_rank, 0, ug_win, win_ptr);

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
        if (CSP_ENV.load_opt == CSP_LOAD_BYTE_COUNTING) {
            PMPI_Type_size(datatype, &data_size);
        }
#endif
        mpi_errno = CSP_Get_gp_rank(target_rank, 0, 1, data_size, ug_win,
                                    &target_g_rank_in_ug, &target_g_offset);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        ug_target_disp = target_g_offset + ug_win->targets[target_rank].disp_unit * target_disp;

        /* Issue operation to the ghost process in corresponding ug-window of target process. */
        mpi_errno = PMPI_Fetch_and_op(origin_addr, result_addr, datatype, target_g_rank_in_ug,
                                      ug_target_disp, op, *win_ptr);

        CSP_DBG_PRINT("CASPER Fetch_and_op to (ghost %d, win 0x%x [%s]) instead of "
                      "target %d, 0x%lx(0x%lx + %d * %ld)\n",
                      target_g_rank_in_ug, *win_ptr,
                      CSP_Win_epoch_stat_name[ug_win->epoch_stat],
                      target_rank, ug_target_disp, target_g_offset,
                      ug_win->targets[target_rank].disp_unit, target_disp);
    }

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
    CSP_Win *ug_win;

    CSP_DBG_PRINT_FCNAME();

    CSP_Fetch_ug_win_from_cache(win, ug_win);

    if (ug_win) {
        /* casper window */
        mpi_errno = CSP_Fetch_and_op_impl(origin_addr, result_addr, datatype, target_rank,
                                          target_disp, op, win, ug_win);
    }
    else {
        /* normal window */
        mpi_errno = PMPI_Fetch_and_op(origin_addr, result_addr, datatype, target_rank,
                                      target_disp, op, win);
    }

    return mpi_errno;
}

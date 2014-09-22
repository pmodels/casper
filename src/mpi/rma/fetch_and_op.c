#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

static int MTCORE_Fetch_and_op_shared_impl(const void *origin_addr, void *result_addr,
                                           MPI_Datatype datatype, int target_rank,
                                           MPI_Aint target_disp, MPI_Op op, MPI_Win win,
                                           MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;

    /* Issue operation to the target through local shared window, because shared
     * communication is fully handled by local process.
     */
    mpi_errno = PMPI_Fetch_and_op(origin_addr, result_addr, datatype,
                                  uh_win->local_uh_rank, target_disp, op, uh_win->local_uh_win);
    MTCORE_DBG_PRINT("MTCORE Fetch_and_op to self(in local_uh) %d in shared_comm\n",
                     uh_win->local_uh_rank);

    goto fn_exit;

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}


static int MTCORE_Fetch_and_op_segment_impl(const void *origin_addr, void *result_addr,
                                            MPI_Datatype datatype, int target_rank,
                                            MPI_Aint target_disp, MPI_Op op, MPI_Win win,
                                            MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int num_segs = 0;
    MTCORE_OP_Segment *decoded_ops = NULL;

    /* TODO : Eliminate operation division for some special cases, see pptx */
    /* Fetch_and_op only allows predefined datatype. */
    mpi_errno = MTCORE_Op_segments_decode_basic_datatype(origin_addr, 1,
                                                         datatype, target_rank, target_disp, 1,
                                                         datatype, uh_win, &decoded_ops, &num_segs);

    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    MTCORE_DBG_PRINT("MTCORE Fetch_and_op to target %d, num_segs=%d\n", target_rank, num_segs);

    int target_h_rank_in_uh = -1;
    int data_size = 0;
    MPI_Aint target_h_offset = 0;
    MPI_Aint uh_target_disp = 0;
    int seg_off = decoded_ops[0].target_seg_off;
    MPI_Win seg_uh_win = uh_win->targets[target_rank].segs[seg_off].uh_win;

    mpi_errno = MTCORE_Get_helper_rank(target_rank, seg_off, 1, decoded_ops[0].target_dtsize,
                                       uh_win, &target_h_rank_in_uh, &target_h_offset);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Fetch_and_op only allows one predefined element which must be contained by
     * a single segmetn, thus we only need translate target displacement according to
     * its segment id. */
    uh_target_disp = target_h_offset
        + uh_win->targets[target_rank].disp_unit * decoded_ops[0].target_disp;

    mpi_errno = PMPI_Fetch_and_op(origin_addr, result_addr, datatype,
                                  target_h_rank_in_uh, uh_target_disp, op, seg_uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    MTCORE_DBG_PRINT("MTCORE Fetch_and_op to (helper %d, win 0x%x) instead of "
                     "target %d, seg %d \n"
                     "(origin.addr %p, count %d, datatype 0x%x, "
                     "target.disp 0x%lx(0x%lx + %d * %ld), count %d, datatype 0x%x)\n",
                     target_h_rank_in_uh, seg_uh_win, target_rank, seg_off,
                     decoded_ops[0].origin_addr, decoded_ops[0].origin_count,
                     decoded_ops[0].origin_datatype, uh_target_disp, target_h_offset,
                     uh_win->targets[target_rank].disp_unit, decoded_ops[0].target_disp,
                     decoded_ops[0].target_count, decoded_ops[0].target_datatype);

  fn_exit:
    MTCORE_Op_segments_destroy(&decoded_ops);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}


static int MTCORE_Fetch_and_op_impl(const void *origin_addr, void *result_addr,
                                    MPI_Datatype datatype, int target_rank, MPI_Aint target_disp,
                                    MPI_Op op, MPI_Win win, MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint uh_target_disp = 0;
    int is_shared = 0;

    int rank;

    PMPI_Comm_rank(uh_win->user_comm, &rank);
#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
    if (target_rank == rank && uh_win->is_self_locked) {
        /* If target is itself, we do not need translate it to any Helpers because
         * win_lock(self) will force lock(helper) to be granted so that it is safe
         * to send operations to the real target.
         */
        mpi_errno = MTCORE_Fetch_and_op_shared_impl(origin_addr, result_addr,
                                                    datatype, target_rank, target_disp, op, win,
                                                    uh_win);
        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;
    }
    else
#endif
    {
        /* TODO: Do we need segment load balancing in fence ?
         * 1. No lock issue.
         * 2. overhead of data range checking and division */

        /* Although only one predefined datatype element is transferred in such
         * operation, we still need call segmentation routine to get its the segment
         * number if target is divided to multiple segments. */
        if (MTCORE_ENV.lock_binding == MTCORE_LOCK_BINDING_SEGMENT &&
            uh_win->targets[target_rank].num_segs > 1 &&
            uh_win->epoch_stat == MTCORE_WIN_EPOCH_LOCK) {
            mpi_errno = MTCORE_Fetch_and_op_segment_impl(origin_addr, result_addr,
                                                         datatype, target_rank, target_disp, op,
                                                         win, uh_win);
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
            int target_h_rank_in_uh = -1;
            int data_size = 0;
            MPI_Aint target_h_offset = 0;
            MPI_Win *win_ptr = NULL;

            if (uh_win->epoch_stat == MTCORE_WIN_EPOCH_FENCE) {
                win_ptr = &uh_win->fence_win;
            }
            else {
                win_ptr = &uh_win->targets[target_rank].segs[0].uh_win;
            }

#if defined(MTCORE_ENABLE_RUNTIME_LOAD_OPT)
            if (MTCORE_ENV.load_opt == MTCORE_LOAD_BYTE_COUNTING) {
                PMPI_Type_size(datatype, &data_size);
            }
#endif
            mpi_errno = MTCORE_Get_helper_rank(target_rank, 0, 1, data_size, uh_win,
                                               &target_h_rank_in_uh, &target_h_offset);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;

            uh_target_disp = target_h_offset + uh_win->targets[target_rank].disp_unit * target_disp;

            /* Issue operation to the helper process in corresponding uh-window of target process. */
            mpi_errno = PMPI_Fetch_and_op(origin_addr, result_addr, datatype, target_h_rank_in_uh,
                                          uh_target_disp, op, *win_ptr);

            MTCORE_DBG_PRINT("MTCORE Fetch_and_op to (helper %d, win 0x%x [%s]) instead of "
                             "target %d, 0x%lx(0x%lx + %d * %ld)\n",
                             target_h_rank_in_uh, *win_ptr,
                             (uh_win->epoch_stat == MTCORE_WIN_EPOCH_FENCE) ? "FENCE" : "LOCK",
                             target_rank, uh_target_disp, target_h_offset,
                             uh_win->targets[target_rank].disp_unit, target_disp);
        }
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
    static const char FCNAME[] = "MPI_Fetch_and_op";
    int mpi_errno = MPI_SUCCESS;
    MTCORE_Win *uh_win;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    mpi_errno = MTCORE_Fetch_and_op_impl(origin_addr, result_addr, datatype, target_rank,
                                         target_disp, op, win, uh_win);

  fn_exit:

    return mpi_errno;

  fn_fail:

    goto fn_exit;
}

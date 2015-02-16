#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"


static int MTCORE_Get_accumulate_impl(const void *origin_addr, int origin_count,
                                      MPI_Datatype origin_datatype, void *result_addr,
                                      int result_count, MPI_Datatype result_datatype,
                                      int target_rank, MPI_Aint target_disp, int target_count,
                                      MPI_Datatype target_datatype, MPI_Op op, MPI_Win win,
                                      MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint uh_target_disp = 0;
    int is_shared = 0;

    int rank;

    PMPI_Comm_rank(uh_win->user_comm, &rank);

#warning MPI_Get_accumulate is not implemented in segment-lock mode for now. \
Please do not set MTCORE_LOCK_METHOD=segment when using MPI_Get_accumulate.

    /* Should not do local RMA in accumulate because of atomicity issue */
    /* TODO: Implement get_acc for segmentation */
    {
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

        MTCORE_Get_epoch_win(target_rank, 0, uh_win, win_ptr);

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
        mpi_errno = PMPI_Get_accumulate(origin_addr, origin_count, origin_datatype,
                                        result_addr, result_count, result_datatype,
                                        target_h_rank_in_uh, uh_target_disp, target_count,
                                        target_datatype, op, *win_ptr);

        MTCORE_DBG_PRINT("MTCORE Get_accumulate to (helper %d, win 0x%x [%s]) instead of "
                         "target %d, 0x%lx(0x%lx + %d * %ld)\n",
                         target_h_rank_in_uh, *win_ptr,
                         MTCORE_Win_epoch_stat_name[uh_win->epoch_stat],
                         target_rank, uh_target_disp, target_h_offset,
                         uh_win->targets[target_rank].disp_unit, target_disp);
    }

  fn_exit:

    return mpi_errno;

  fn_fail:

    goto fn_exit;
}

int MPI_Get_accumulate(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype,
                       void *result_addr, int result_count, MPI_Datatype result_datatype,
                       int target_rank, MPI_Aint target_disp, int target_count,
                       MPI_Datatype target_datatype, MPI_Op op, MPI_Win win)
{
    static const char FCNAME[] = "MPI_Get_accumulate";
    int mpi_errno = MPI_SUCCESS;
    MTCORE_Win *uh_win;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    if (uh_win) {
        /* mtcore window */
        mpi_errno = MTCORE_Get_accumulate_impl(origin_addr, origin_count, origin_datatype,
                                               result_addr, result_count, result_datatype,
                                               target_rank, target_disp, target_count,
                                               target_datatype, op, win, uh_win);
    }
    else {
        /* normal window */
        mpi_errno = PMPI_Get_accumulate(origin_addr, origin_count, origin_datatype,
                                        result_addr, result_count, result_datatype,
                                        target_rank, target_disp, target_count,
                                        target_datatype, op, win);
    }
  fn_exit:

    return mpi_errno;

  fn_fail:

    goto fn_exit;
}

#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

static int MPIASP_Put_shared_impl(const void *origin_addr, int origin_count,
                                  MPI_Datatype origin_datatype,
                                  int target_rank, MPI_Aint target_disp,
                                  int target_count,
                                  MPI_Datatype target_datatype, MPI_Win win, MPIASP_Win * ua_win)
{
    int mpi_errno = MPI_SUCCESS;
    int target_rank_in_local_ua = 0;

    PMPI_Group_translate_ranks(ua_win->user_group, 1,
                               &target_rank, ua_win->local_ua_group, &target_rank_in_local_ua);

    /* Issue operation to the target through local shared window, because shared
     * communication is fully handled by local process.
     */
    mpi_errno = PMPI_Put(origin_addr, origin_count, origin_datatype,
                         target_rank_in_local_ua, target_disp,
                         target_count, target_datatype, ua_win->local_ua_win);

    MPIASP_DBG_PRINT("MPIASP PUT to target(in local_ua) %d in shared_comm\n",
                     target_rank_in_local_ua);

    goto fn_exit;

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int MPIASP_Put_impl(const void *origin_addr, int origin_count,
                           MPI_Datatype origin_datatype,
                           int target_rank, MPI_Aint target_disp,
                           int target_count,
                           MPI_Datatype target_datatype, MPI_Win win, MPIASP_Win * ua_win)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint ua_target_disp = 0;
    int target_node_id = -1;
    int rank;

    PMPI_Comm_rank(ua_win->user_comm, &rank);
    if (target_rank == rank && ua_win->is_self_locked) {
        /* If target is itself, we do not need translate it to any Helpers because
         * win_lock(self) will force lock(helper) to be granted so that it is safe
         * to send operations to the real target.
         */
        mpi_errno = MPIASP_Put_shared_impl(origin_addr, origin_count,
                                           origin_datatype, target_rank, target_disp, target_count,
                                           target_datatype, win, ua_win);
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
        int target_local_rank = ua_win->local_user_ranks[target_rank];

        mpi_errno = MPIASP_Get_node_ids(ua_win->user_group, 1, &target_rank, &target_node_id);
        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;

        ua_target_disp = ua_win->base_asp_offset[target_rank]
            + ua_win->disp_units[target_rank] * target_disp;

        /* Issue operation to the helper process in corresponding ua-window of target process. */
        mpi_errno = PMPI_Put(origin_addr, origin_count, origin_datatype,
                             ua_win->asp_ranks_in_ua[target_node_id], ua_target_disp,
                             target_count, target_datatype, ua_win->ua_wins[target_local_rank]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        MPIASP_DBG_PRINT("MPIASP Put to (helper %d, win[%d]) instead of "
                         "target %d(node_id %d), 0x%lx(0x%lx + %d * %ld)\n",
                         ua_win->asp_ranks_in_ua[target_node_id], target_local_rank,
                         target_rank, target_node_id, ua_target_disp,
                         ua_win->base_asp_offset[target_rank], ua_win->disp_units[target_rank],
                         target_disp);
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
    static const char FCNAME[] = "MPIASP_Put";
    int mpi_errno = MPI_SUCCESS;
    MPIASP_Win *ua_win;

    MPIASP_DBG_PRINT_FCNAME();

    /* Replace displacement if it is an MPIASP-window */
    mpi_errno = get_ua_win(win, &ua_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (ua_win > 0) {
        mpi_errno = MPIASP_Put_impl(origin_addr, origin_count,
                                    origin_datatype, target_rank, target_disp, target_count,
                                    target_datatype, win, ua_win);
    }
    else {
        mpi_errno = PMPI_Put(origin_addr, origin_count, origin_datatype,
                             target_rank, target_disp, target_count, target_datatype, win);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

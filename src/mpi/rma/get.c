#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

static int MTCORE_Get_shared_impl(void *origin_addr, int origin_count,
                                  MPI_Datatype origin_datatype,
                                  int target_rank, MPI_Aint target_disp,
                                  int target_count,
                                  MPI_Datatype target_datatype, MPI_Win win, MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int target_rank_in_local_uh = 0;

    PMPI_Group_translate_ranks(uh_win->user_group, 1,
                               &target_rank, uh_win->local_uh_group, &target_rank_in_local_uh);

    /* Issue operation to the target through local shared window, because shared
     * communication is fully handled by local process.
     */
    mpi_errno = PMPI_Get(origin_addr, origin_count, origin_datatype,
                         target_rank_in_local_uh, target_disp,
                         target_count, target_datatype, uh_win->local_uh_win);

    MTCORE_DBG_PRINT("MTCORE GET from target(in local_uh) %d in shared_comm\n",
                     target_rank_in_local_uh);

    goto fn_exit;

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int MTCORE_Get_impl(void *origin_addr, int origin_count,
                           MPI_Datatype origin_datatype,
                           int target_rank, MPI_Aint target_disp,
                           int target_count,
                           MPI_Datatype target_datatype, MPI_Win win, MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint uh_target_disp = 0;
    int target_node_id = -1;
    int rank;

    PMPI_Comm_rank(uh_win->user_comm, &rank);
#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
    if (target_rank == rank && uh_win->is_self_locked) {
        /* If target is itself, we do not need translate it to any Helpers because
         * win_lock(self) will force lock(helper) to be granted so that it is safe
         * to send operations to the real target.
         */
        mpi_errno = MTCORE_Get_shared_impl(origin_addr, origin_count,
                                           origin_datatype, target_rank, target_disp, target_count,
                                           target_datatype, win, uh_win);
        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;
    }
    else
#endif
    {
        /* Translation for intra/inter-node operations.
         *
         * We do not use force flush + shared window for optimizing operations to local targets.
         * Because: 1) we lose lock optimization on force flush; 2) Although most implementation
         * does shared-communication for operations on shared windows, MPI standard doesn’t
         * require it. Some implementation may use network even for shared targets for
         * shorter CPU occupancy.
         */
        int target_local_rank = uh_win->local_user_ranks[target_rank];
        int target_h_rank_in_uh = -1;

        MTCORE_Get_helper_rank(target_rank, 0, uh_win, &target_h_rank_in_uh);

        uh_target_disp = uh_win->base_h_offsets[target_rank]
            + uh_win->disp_units[target_rank] * target_disp;

        /* Issue operation to the helper process in corresponding uh-window of target process. */
        mpi_errno = PMPI_Get(origin_addr, origin_count, origin_datatype,
                             target_h_rank_in_uh, uh_target_disp,
                             target_count, target_datatype, uh_win->uh_wins[target_local_rank]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        MTCORE_DBG_PRINT("MTCORE Get from (helper %d, win[%d]) instead of "
                         "target %d, 0x%lx(0x%lx + %d * %ld)\n",
                         target_h_rank_in_uh, target_local_rank, target_rank, uh_target_disp,
                         uh_win->base_h_offsets[target_rank], uh_win->disp_units[target_rank],
                         target_disp);
    }
  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Get(void *origin_addr, int origin_count,
            MPI_Datatype origin_datatype,
            int target_rank, MPI_Aint target_disp,
            int target_count, MPI_Datatype target_datatype, MPI_Win win)
{
    static const char FCNAME[] = "MPI_Get";
    int mpi_errno = MPI_SUCCESS;
    MTCORE_Win *uh_win;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    mpi_errno = MTCORE_Get_impl(origin_addr, origin_count, origin_datatype,
                                target_rank, target_disp, target_count, target_datatype, win,
                                uh_win);

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

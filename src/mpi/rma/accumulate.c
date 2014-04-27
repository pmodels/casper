#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

static int MPIASP_Accumulate_impl(const void *origin_addr, int origin_count,
        MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp,
        int target_count, MPI_Datatype target_datatype, MPI_Op op, MPI_Win win,
        MPIASP_Win *ua_win) {
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint ua_target_disp = 0;

    ua_target_disp = ua_win->base_asp_offset[target_rank]
            + ua_win->disp_units[target_rank] * target_disp;

    mpi_errno = PMPI_Accumulate(origin_addr, origin_count, origin_datatype,
            ua_win->asp_ranks_in_ua[target_rank], ua_target_disp, target_count,
            target_datatype, op, win);

    MPIASP_DBG_PRINT(
            "MPIASP Accumulate to asp %d instead of target %d, 0x%lx(0x%lx + %d * %ld)\n",
            ua_win->asp_ranks_in_ua[target_rank], target_rank, ua_target_disp,
            ua_win->base_asp_offset[target_rank],
            ua_win->disp_units[target_rank], target_disp);

    fn_exit:

    return mpi_errno;

    fn_fail:

    goto fn_exit;
}

int MPI_Accumulate(const void *origin_addr, int origin_count,
        MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp,
        int target_count, MPI_Datatype target_datatype, MPI_Op op, MPI_Win win) {
    static const char FCNAME[] = "MPI_Accumulate";
    int mpi_errno = MPI_SUCCESS;
    MPIASP_Win *ua_win;

    MPIASP_DBG_PRINT_FCNAME();

    ua_win = get_ua_win(win);

    /* Replace displacement if it is an MPIASP-window */
    if (ua_win > 0) {
        mpi_errno = MPIASP_Accumulate_impl(origin_addr, origin_count,
                origin_datatype, target_rank, target_disp, target_count,
                target_datatype, op, win, ua_win);
    } else {
        mpi_errno = PMPI_Accumulate(origin_addr, origin_count, origin_datatype,
                target_rank, target_disp, target_count, target_datatype, op,
                win);
    }

    fn_exit:

    return mpi_errno;

    fn_fail:

    goto fn_exit;
}

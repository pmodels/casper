#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

int MPI_Accumulate(const void *origin_addr, int origin_count,
        MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp,
        int target_count, MPI_Datatype target_datatype, MPI_Op op, MPI_Win win) {
    static const char FCNAME[] = "MPI_Accumulate";
    int mpi_errno = MPI_SUCCESS;
    MPIASP_Win *ua_win;
    MPI_Aint ua_target_disp = 0;

    MPIASP_DBG_PRINT_FCNAME();

    if (MPIASP_Comm_rank_isasp())
        goto fn_exit;

    ua_win = get_ua_win(win);

    /* Replace displacement if it is an MPIASP-window */
    if (ua_win > 0) {
        ua_target_disp = ua_win->base_addrs[target_rank]
                + ua_win->disp_units[target_rank] * target_disp;

        mpi_errno = PMPI_Accumulate(origin_addr, origin_count, origin_datatype,
                target_rank, ua_target_disp, target_count, target_datatype, op,
                win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        MPIASP_DBG_PRINT(
                "MPIASP Accumulate on target %d, disp 0x%lx, org_addr %p \n", target_rank, ua_target_disp, origin_addr);
    } else {
        mpi_errno = PMPI_Accumulate(origin_addr, origin_count, origin_datatype,
                target_rank, target_disp, target_count, target_datatype, op,
                win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    fn_exit:

    return mpi_errno;

    fn_fail:

    goto fn_exit;
}

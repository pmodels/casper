#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

/* -- Begin Profiling Symbol Block for routine MPI_Win_create */
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPI_Get = MPIASP_Get
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF MPIASP_Get  MPI_Get
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPI_Get as MPIASP_Get
#endif
/* -- End Profiling Symbol Block */

#undef FUNCNAME
#define FUNCNAME MPIASP_Get

int MPIASP_Get(void *origin_addr, int origin_count,
        MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp,
        int target_count, MPI_Datatype target_datatype, MPI_Win win) {
    static const char FCNAME[] = "MPIASP_Get";
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

        mpi_errno = PMPI_Get(origin_addr, origin_count, origin_datatype,
                target_rank, ua_target_disp, target_count, target_datatype, win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        MPIASP_DBG_PRINT(
                "MPIASP Get from target %d, 0x%lx to \n", target_rank, ua_target_disp, origin_addr);
    } else {
        mpi_errno = PMPI_Get(origin_addr, origin_count, origin_datatype,
                target_rank, target_disp, target_count, target_datatype, win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    fn_exit:

    return mpi_errno;

    fn_fail:

    goto fn_exit;
}

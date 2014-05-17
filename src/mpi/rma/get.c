#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

#ifdef ENABLE_SHRD_COMM_TRANS
static int MPIASP_Get_shared_impl(void *origin_addr, int origin_count,
        MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp,
        int target_count, MPI_Datatype target_datatype, MPI_Win win,
        MPIASP_Win *ua_win) {
    int mpi_errno = MPI_SUCCESS;
    int target_rank_in_local_ua = 0;

    PMPI_Group_translate_ranks(ua_win->user_group, 1,
            &target_rank, ua_win->local_ua_group, &target_rank_in_local_ua);

    mpi_errno = PMPI_Put(origin_addr, origin_count, origin_datatype,
            target_rank_in_local_ua, target_disp,
            target_count, target_datatype, ua_win->local_ua_win);

    MPIASP_DBG_PRINT(
            "MPIASP GET from target(in local_ua) %d in shared_comm\n",
            target_rank_in_local_ua);

    goto fn_exit;

    fn_exit:
    return mpi_errno;

    fn_fail:
    goto fn_exit;
}
#endif


static int MPIASP_Get_impl(void *origin_addr, int origin_count,
        MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp,
        int target_count, MPI_Datatype target_datatype, MPI_Win win,
        MPIASP_Win *ua_win) {
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint ua_target_disp = 0;

#ifdef ENABLE_SHRD_COMM_TRANS
    int is_shared = 0;
    mpi_errno = MPIASP_Is_in_shrd_mem(target_rank, ua_win->user_group,
            &is_shared);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (is_shared) {
        mpi_errno = MPIASP_Get_shared_impl(origin_addr, origin_count,
                origin_datatype, target_rank, target_disp, target_count,
                target_datatype, win, ua_win);
    }
    else
#endif
    {
        ua_target_disp = ua_win->base_asp_offset[target_rank]
                + ua_win->disp_units[target_rank] * target_disp;

        mpi_errno = PMPI_Get(origin_addr, origin_count, origin_datatype,
                ua_win->asp_ranks_in_ua[target_rank], ua_target_disp,
                target_count, target_datatype, win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        MPIASP_DBG_PRINT(
                "MPIASP Get from asp %d instead of target %d, 0x%lx(0x%lx + %d * %ld)\n",
                ua_win->asp_ranks_in_ua[target_rank], target_rank,
                ua_target_disp,
                ua_win->base_asp_offset[target_rank],
                ua_win->disp_units[target_rank], target_disp);
    }
    fn_exit:
    return mpi_errno;

    fn_fail:
    goto fn_exit;
}

int MPI_Get(void *origin_addr, int origin_count,
        MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp,
        int target_count, MPI_Datatype target_datatype, MPI_Win win) {
    static const char FCNAME[] = "MPI_Get";
    int mpi_errno = MPI_SUCCESS;
    MPIASP_Win *ua_win;

    MPIASP_DBG_PRINT_FCNAME();

    /* Replace displacement if it is an MPIASP-window */
    mpi_errno = get_ua_win(win, &ua_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (ua_win > 0) {
        mpi_errno = MPIASP_Get_impl(origin_addr, origin_count, origin_datatype,
                target_rank, target_disp, target_count, target_datatype, win,
                ua_win);
    } else {
        mpi_errno = PMPI_Get(origin_addr, origin_count, origin_datatype,
                target_rank, target_disp, target_count, target_datatype, win);
    }

    fn_exit:
    return mpi_errno;

    fn_fail:
    goto fn_exit;
}

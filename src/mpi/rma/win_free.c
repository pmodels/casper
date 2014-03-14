#include <stdio.h>
#include <stdlib.h>
#include "asp.h"

/* -- Begin Profiling Symbol Block for routine MPI_Win_create */
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPI_Win_free = MPIASP_Win_free
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF MPIASP_Win_free  MPI_Win_free
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPI_Win_free as MPIASP_Win_free
#endif
/* -- End Profiling Symbol Block */

#undef FUNCNAME
#define FUNCNAME MPIASP_Win_free

int MPIASP_Win_free(MPI_Win *win) {
    static const char FCNAME[] = "MPIASP_Win_free";
    int mpi_errno = MPI_SUCCESS;
    MPIASP_Win *ua_win;
    int user_rank, user_nprocs;

    MPIASP_DBG_PRINT_FCNAME();

    if (MPIASP_Comm_rank_isasp())
        goto fn_exit;

    ua_win = remove_ua_win(*win);

    /* Release additional resources if it is an MPIASP-window */
    if (ua_win > 0) {
        PMPI_Comm_rank(ua_win->user_comm, &user_rank);
        PMPI_Comm_size(ua_win->user_comm, &user_nprocs);

        if (user_rank == 0) {
            MPIASP_Func_start(MPIASP_FUNC_WIN_FREE, user_nprocs,
                    (int) ua_win->user_comm);
        }

        if (ua_win->shrd_win > 0) {
            mpi_errno = PMPI_Win_free(ua_win->shrd_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        mpi_errno = PMPI_Win_free(win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        if (ua_win->shrd_comm > 0) {
            mpi_errno = PMPI_Comm_free(ua_win->shrd_comm);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
        if (ua_win->ua_comm > 0) {
            mpi_errno = PMPI_Comm_free(ua_win->ua_comm);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        // ua_win->user_comm is created by user, will be freed by user.

        if (ua_win->all_base_asp_addrs)
            free(ua_win->all_base_asp_addrs);
        if (ua_win->all_base_addrs)
            free(ua_win->all_base_addrs);

        free(ua_win);

        MPIASP_DBG_PRINT( "[%d] free MPIASP window 0x%lx\n", user_rank, *win);
    } else {
        PMPI_Comm_rank(MPI_COMM_WORLD, &user_rank);

        mpi_errno = PMPI_Win_free(win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        MPIASP_DBG_PRINT( "[%d] Freed MPI window 0x%lx\n", user_rank, *win);
    }

    fn_exit:

    return mpi_errno;

    fn_fail:

    goto fn_exit;
}

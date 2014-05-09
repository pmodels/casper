#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

int MPI_Win_flush_all(MPI_Win win) {
    MPIASP_Win *ua_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, local_ua_nprocs;

    MPIASP_DBG_PRINT_FCNAME();

#ifdef ENABLE_SHRD_COMM_TRANS
    mpi_errno = get_ua_win(win, &ua_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (ua_win > 0) {
        PMPI_Comm_rank(ua_win->user_comm, &user_rank);
        PMPI_Comm_size(ua_win->local_ua_comm, &local_ua_nprocs);

        // Also flush shared window for local communication
        // if there are multiple local user processes
        if (local_ua_nprocs > MPIASP_NUM_ASP_IN_LOCAL) {
            MPIASP_DBG_PRINT("[%d]flush all local_ua_win\n", user_rank);

            mpi_errno = PMPI_Win_flush_all(ua_win->local_ua_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
    }
#endif

    mpi_errno = PMPI_Win_flush_all(win);

    fn_exit:
    return mpi_errno;

    fn_fail:
    goto fn_exit;
}

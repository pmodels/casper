#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

int MPI_Win_lock_all(int assert, MPI_Win win)
{
    MPIASP_Win *ua_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, local_ua_nprocs;

    MPIASP_DBG_PRINT_FCNAME();

    mpi_errno = get_ua_win(win, &ua_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (ua_win > 0) {
#ifdef ENABLE_SHRD_COMM_TRANS
        PMPI_Comm_rank(ua_win->user_comm, &user_rank);
        PMPI_Comm_size(ua_win->local_ua_comm, &local_ua_nprocs);

        // Also lock shared window for later local communication
        // if there are multiple local user processes
        if (local_ua_nprocs > MPIASP_NUM_ASP_IN_LOCAL) {
            MPIASP_DBG_PRINT("[%d]lock local_ua_win\n", user_rank);

            mpi_errno = PMPI_Win_lock_all(assert, ua_win->local_ua_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
#endif
        mpi_errno = PMPI_Win_lock_all(assert, ua_win->ua_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        // TODO: we have not implement translation for all operations yet.
        // So still some of them are pushed into user window
//      goto fn_exit;
    }

    mpi_errno = PMPI_Win_lock_all(assert, win);

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

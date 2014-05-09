#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

int MPI_Win_flush(int rank, MPI_Win win) {
    MPIASP_Win *ua_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, rank_in_local_ua = 0;
    int is_shared = 0;

    MPIASP_DBG_PRINT_FCNAME();

#ifdef ENABLE_SHRD_COMM_TRANS
    mpi_errno = get_ua_win(win, &ua_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (ua_win > 0) {
        PMPI_Comm_rank(ua_win->user_comm, &user_rank);

        mpi_errno = MPIASP_Is_in_shrd_mem(rank, ua_win->user_group,
                &is_shared);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        if (is_shared) {
            PMPI_Group_translate_ranks(ua_win->user_group, 1, &rank,
                    ua_win->local_ua_group, &rank_in_local_ua);

            MPIASP_DBG_PRINT(
                    "[%d]flush local_ua_win, target %d instead of %d\n",
                    user_rank, rank_in_local_ua, rank);

            mpi_errno = PMPI_Win_flush(rank_in_local_ua, ua_win->local_ua_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;

            // TODO: we have not implement shared translation for all operations yet.
//            goto fn_exit;
        }
    }
#endif

    mpi_errno = PMPI_Win_flush(rank, win);

    fn_exit:
    return mpi_errno;

    fn_fail:
    goto fn_exit;
}

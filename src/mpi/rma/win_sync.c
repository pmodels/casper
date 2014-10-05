/*
 * win_sync.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

int MPI_Win_sync(MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank = 0;
    int i;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    if (uh_win == NULL) {
        /* normal window */
        return PMPI_Win_sync(win);
    }

    /* mtcore window starts */

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);

    if (uh_win->is_self_locked) {
        mpi_errno = PMPI_Win_sync(uh_win->local_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    for (i = 0; i < uh_win->targets[user_rank].num_uh_wins; i++) {
        mpi_errno = PMPI_Win_sync(uh_win->targets[user_rank].uh_wins[i]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    return mpi_errno;
}

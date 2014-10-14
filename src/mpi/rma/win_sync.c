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

    MPI_Win *win_ptr = NULL;
    MTCORE_Get_epoch_local_win(uh_win, win_ptr);

    mpi_errno = PMPI_Win_sync(*win_ptr);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    MTCORE_DBG_PRINT("[%d] win sync on %s local win 0x%x\n", user_rank,
                     MTCORE_Win_epoch_stat_name[uh_win->epoch_stat], *win_ptr);

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "csp.h"

int MPI_Win_sync(MPI_Win win)
{
    CSP_Win *ug_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank = 0;

    CSP_DBG_PRINT_FCNAME();

    CSP_Fetch_ug_win_from_cache(win, ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_sync(win);
    }

    /* casper window starts */

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);

    MPI_Win *win_ptr = NULL;
    CSP_Get_epoch_local_win(ug_win, win_ptr);

    mpi_errno = PMPI_Win_sync(*win_ptr);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    CSP_DBG_PRINT("[%d] win sync on %s local win 0x%x\n", user_rank,
                  CSP_Win_epoch_stat_name[ug_win->epoch_stat], *win_ptr);

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

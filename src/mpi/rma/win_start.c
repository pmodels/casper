/*
 * win_start.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

static int fill_ranks_in_win_grp(MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int *ranks_in_start_grp = NULL;
    int i, start_grp_size;

    mpi_errno = PMPI_Group_size(uh_win->start_group, &start_grp_size);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    ranks_in_start_grp = calloc(start_grp_size, sizeof(int));
    for (i = 0; i < start_grp_size; i++) {
        ranks_in_start_grp[i] = i;
    }

    mpi_errno = PMPI_Group_translate_ranks(uh_win->start_group, start_grp_size,
                                           ranks_in_start_grp, uh_win->user_group,
                                           uh_win->start_ranks_in_win_group);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    if (ranks_in_start_grp)
        free(ranks_in_start_grp);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_start(MPI_Group group, int assert, MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int start_grp_size = 0;
    int i;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    if (group == MPI_GROUP_NULL) {
        /* standard says do nothing for empty group */
        MTCORE_DBG_PRINT("Start empty group\n");
        return mpi_errno;
    }

    mpi_errno = PMPI_Group_size(group, &start_grp_size);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (start_grp_size <= 0) {
        /* standard says do nothing for empty group */
        MTCORE_DBG_PRINT("Start empty group\n");
        return mpi_errno;
    }

    uh_win->start_group = group;
    uh_win->start_ranks_in_win_group = calloc(start_grp_size, sizeof(int));
    MTCORE_DBG_PRINT("start group %p, size %d\n", uh_win->start_group, start_grp_size);

    /* Both lock and start only allow no_check assert. */
    assert = (assert == MPI_MODE_NOCHECK) ? MPI_MODE_NOCHECK : 0;

    mpi_errno = fill_ranks_in_win_grp(uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    for (i = 0; i < start_grp_size; i++) {
        MTCORE_DBG_PRINT("\t\t start target %d\n", uh_win->start_ranks_in_win_group[i]);

        /* Use manticore lock */
        mpi_errno = MPI_Win_lock(MPI_LOCK_SHARED, uh_win->start_ranks_in_win_group[i], assert, win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    if (uh_win->start_ranks_in_win_group)
        free(uh_win->start_ranks_in_win_group);
    uh_win->start_group = MPI_GROUP_NULL;
    uh_win->start_ranks_in_win_group = NULL;

    goto fn_exit;
}

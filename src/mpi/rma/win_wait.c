/*
 * win_wait.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

static int wait_pscw_origin_completion(int post_grp_size, MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank;
    PMPI_Comm_rank(uh_win->user_comm, &user_rank);

    /* We are not in any epoch now, need lock for accessing local window. */
    mpi_errno = PMPI_Win_lock(MPI_LOCK_SHARED, uh_win->my_rank_in_uh_comm, 0, uh_win->my_pscw_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Weak sync, the next origin start can go first. */
    while ((*uh_win->wait_counter_ptr) < post_grp_size) {
        mpi_errno = PMPI_Win_sync(uh_win->my_pscw_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
    *uh_win->wait_counter_ptr -= post_grp_size;

    mpi_errno = PMPI_Win_unlock(uh_win->my_rank_in_uh_comm, uh_win->my_pscw_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_wait(MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int post_grp_size = 0;
    int i;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    if (uh_win == NULL) {
        /* normal window */
        return PMPI_Win_wait(win);
    }

    /* mtcore window starts */

    MTCORE_Assert((uh_win->info_args.epoch_type & MTCORE_EPOCH_PSCW));

    if (uh_win->post_group == MPI_GROUP_NULL) {
        /* standard says do nothing for empty group */
        MTCORE_DBG_PRINT("Wait empty group\n");
        return mpi_errno;
    }

    mpi_errno = PMPI_Group_size(uh_win->post_group, &post_grp_size);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    MTCORE_Assert(post_grp_size > 0);

    MTCORE_DBG_PRINT("Wait group 0x%x, size %d, wait_counter %d\n",
                     uh_win->post_group, post_grp_size, *(uh_win->wait_counter_ptr));

    /* Wait for the completion on all origin processes */
    mpi_errno = wait_pscw_origin_completion(post_grp_size, uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Issue self exclusive lock.
     * Origins cannot access this target after self lock is issued and force acquired. */
    mpi_errno = MTCORE_Win_lock_self_pscw_win(uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    MTCORE_DBG_PRINT("Wait done\n");

  fn_exit:
    if (uh_win->post_ranks_in_win_group)
        free(uh_win->post_ranks_in_win_group);
    uh_win->post_group = MPI_GROUP_NULL;
    uh_win->post_ranks_in_win_group = NULL;

    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

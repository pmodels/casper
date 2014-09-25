/*
 * win_wait.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

int MPI_Win_wait(MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int post_grp_size = 0;
    int i;
    MPI_Request *reqs = NULL;
    MPI_Status *stats = NULL;
    char *bufs;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    if (uh_win == NULL) {
        /* normal window */
        return PMPI_Win_wait(win);
    }

    /* mtcore window starts */

    if (uh_win->post_group == MPI_GROUP_NULL) {
        /* standard says do nothing for empty group */
        MTCORE_DBG_PRINT("Wait empty group\n");
        return mpi_errno;
    }

    mpi_errno = PMPI_Group_size(uh_win->post_group, &post_grp_size);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    MTCORE_Assert(post_grp_size > 0);

    MTCORE_DBG_PRINT("Wait group %p, size %d\n", uh_win->post_group, post_grp_size);

    reqs = calloc(post_grp_size, sizeof(MPI_Request));
    stats = calloc(post_grp_size, sizeof(MPI_Status));
    bufs = calloc(post_grp_size, sizeof(char));

    /* block until all completes are done */
    for (i = 0; i < post_grp_size; i++) {
        mpi_errno = PMPI_Irecv(&bufs[i], 1, MPI_CHAR, uh_win->post_ranks_in_win_group[i],
                               MTCORE_PSCW_COMPLETE_TAG, uh_win->user_comm, &reqs[i]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    mpi_errno = PMPI_Waitall(post_grp_size, reqs, stats);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    MTCORE_DBG_PRINT("Wait done\n");

  fn_exit:
    if (uh_win->post_ranks_in_win_group)
        free(uh_win->post_ranks_in_win_group);
    uh_win->post_group = MPI_GROUP_NULL;
    uh_win->post_ranks_in_win_group = NULL;

    if (reqs)
        free(reqs);
    if (stats)
        free(stats);
    if (bufs)
        free(bufs);

    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

/*
 * win_complete.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

int MPI_Win_complete(MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int start_grp_size = 0;
    int i;
    MPI_Request *reqs = NULL;
    MPI_Status *stats = NULL;
    char *bufs;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    if (uh_win == NULL) {
        /* normal window */
        return PMPI_Win_complete(win);
    }

    /* mtcore window starts */

    MTCORE_Assert((uh_win->info_args.epoch_type & MTCORE_EPOCH_PSCW));

    if (uh_win->start_group == MPI_GROUP_NULL) {
        /* standard says do nothing for empty group */
        MTCORE_DBG_PRINT("Complete empty group\n");
        return mpi_errno;
    }

    mpi_errno = PMPI_Group_size(uh_win->start_group, &start_grp_size);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    MTCORE_Assert(start_grp_size > 0);

    MTCORE_DBG_PRINT("Complete group %p, size %d\n", uh_win->start_group, start_grp_size);

    reqs = calloc(start_grp_size, sizeof(MPI_Request));
    stats = calloc(start_grp_size, sizeof(MPI_Status));
    bufs = calloc(start_grp_size, sizeof(char));

    for (i = 0; i < start_grp_size; i++) {
        MTCORE_DBG_PRINT("\t\t complete target %d\n", uh_win->start_ranks_in_win_group[i]);

        /* use manticore unlock */
        mpi_errno = MPI_Win_unlock(uh_win->start_ranks_in_win_group[i], win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* notify target it is done, target is blocking in complete */
        mpi_errno = PMPI_Isend(&bufs[i], 1, MPI_CHAR, uh_win->start_ranks_in_win_group[i],
                               MTCORE_PSCW_COMPLETE_TAG, uh_win->user_comm, &reqs[i]);
    }

    /* TODO: can we do testall instead ? complete does not wait for the completion on target.
     * However, can we free send buffers before all sends are done ? */
    mpi_errno = PMPI_Waitall(start_grp_size, reqs, stats);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    MTCORE_DBG_PRINT("Complete done\n");

  fn_exit:
    if (uh_win->start_ranks_in_win_group)
        free(uh_win->start_ranks_in_win_group);
    uh_win->start_group = MPI_GROUP_NULL;
    uh_win->start_ranks_in_win_group = NULL;

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

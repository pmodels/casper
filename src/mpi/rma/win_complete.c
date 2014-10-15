/*
 * win_complete.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

static inline int MTCORE_Win_unlock_self_impl(MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank;

#ifdef MTCORE_ENABLE_SYNC_ALL_OPT
    /* unlockall already unlocked window for local target */
#else
    MTCORE_DBG_PRINT("[%d]unlock self(%d, local pscw_win 0x%x)\n", user_rank,
                     uh_win->my_rank_in_uh_comm, uh_win->my_pscw_win);
    mpi_errno = PMPI_Win_unlock(uh_win->my_rank_in_uh_comm, uh_win->my_pscw_win);
    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;
#endif

    uh_win->is_self_locked = 0;
    return mpi_errno;
}

static inline int isend_pscw_complete_msg(int target_rank, MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int main_h_off = uh_win->targets[target_rank].segs[0].main_h_off;
    int target_h_rank_in_uh = uh_win->targets[target_rank].h_ranks_in_uh[main_h_off];

    int cnt = 1;

    /* Increase target wait counter on the main helper.
     * Do not send to another helper, because we may need wait for lock acquired
     * for that helper. */
    MTCORE_DBG_PRINT("decrease pscw counter(Helper %d, offset 0x%lx)\n",
                     target_h_rank_in_uh, uh_win->targets[target_rank].wait_counter_offset);
    mpi_errno = PMPI_Accumulate(&cnt, 1, MPI_INT, target_h_rank_in_uh,
                                uh_win->targets[target_rank].wait_counter_offset, 1,
                                MPI_INT, MPI_SUM, uh_win->targets[target_rank].pscw_win);
    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;
}

static int MTCORE_Win_Pscw_unlock(int target_rank, MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank;
    int k;

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);

#ifdef MTCORE_ENABLE_SYNC_ALL_OPT
    MTCORE_DBG_PRINT("[%d]flush_all(pscw_win 0x%x), instead of target rank %d\n",
                     user_rank, uh_win->targets[target_rank].pscw_win, target_rank);
    mpi_errno = PMPI_Win_flush_all(uh_win->targets[target_rank].pscw_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
#else
    /* Flush every helper on every window. */
    for (k = 0; k < MTCORE_ENV.num_h; k++) {
        int target_h_rank_in_uh = uh_win->targets[target_rank].h_ranks_in_uh[k];

        MTCORE_DBG_PRINT("[%d]flush(Helper(%d), pscw_wins 0x%x), instead of "
                         "target rank %d\n", user_rank, target_h_rank_in_uh,
                         uh_win->targets[target_rank].pscw_win, target_rank);

        mpi_errno = PMPI_Win_flush(target_h_rank_in_uh, uh_win->targets[target_rank].pscw_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
#endif

    mpi_errno = isend_pscw_complete_msg(target_rank, uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

#ifdef MTCORE_ENABLE_SYNC_ALL_OPT
    MTCORE_DBG_PRINT("[%d]unlock_all(pscw_win 0x%x), instead of target rank %d\n",
                     user_rank, uh_win->targets[target_rank].pscw_win, target_rank);
    mpi_errno = PMPI_Win_unlock_all(uh_win->targets[target_rank].pscw_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
#else
    /* Unlock every helper on every window. */
    for (k = 0; k < MTCORE_ENV.num_h; k++) {
        int target_h_rank_in_uh = uh_win->targets[target_rank].h_ranks_in_uh[k];

        MTCORE_DBG_PRINT("[%d]unlock(Helper(%d), pscw_wins 0x%x), instead of "
                         "target rank %d\n", user_rank, target_h_rank_in_uh,
                         uh_win->targets[target_rank].pscw_win, target_rank);

        mpi_errno = PMPI_Win_unlock(target_h_rank_in_uh, uh_win->targets[target_rank].pscw_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
#endif

    if (user_rank == target_rank) {
#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
        mpi_errno = MTCORE_Win_unlock_self_impl(uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
#endif
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_complete(MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int start_grp_size = 0;
    int i;

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

    MTCORE_DBG_PRINT("Complete group 0x%x, size %d\n", uh_win->start_group, start_grp_size);

    for (i = 0; i < start_grp_size; i++) {
        MTCORE_DBG_PRINT("\t\t complete target %d\n", uh_win->start_ranks_in_win_group[i]);

        mpi_errno = MTCORE_Win_Pscw_unlock(uh_win->start_ranks_in_win_group[i], uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* Decrease start counter, change epoch status only when counter
     * become 0. */
    uh_win->start_counter--;
    if (uh_win->start_counter == 0) {
        MTCORE_DBG_PRINT("all starts are completed ! no epoch now\n");
        uh_win->epoch_stat = MTCORE_WIN_NO_EPOCH;
    }

    MTCORE_DBG_PRINT("Complete done\n");

  fn_exit:
    if (uh_win->start_ranks_in_win_group)
        free(uh_win->start_ranks_in_win_group);
    uh_win->start_group = MPI_GROUP_NULL;
    uh_win->start_ranks_in_win_group = NULL;

    return mpi_errno;

  fn_fail:
    goto fn_exit;

}

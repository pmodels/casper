#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

int MPI_Win_unlock(int target_rank, MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank;
    int j;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);

    uh_win->targets[target_rank].remote_lock_assert = 0;

    /* Unlock helper process in corresponding uh-window of target process. */
#ifdef MTCORE_ENABLE_SYNC_ALL_OPT

    /* Optimization for MPI implementations that have optimized lock_all.
     * However, user should be noted that, if MPI implementation issues lock messages
     * for every target even if it does not have any operation, this optimization
     * could lose performance and even lose asynchronous! */

    MTCORE_DBG_PRINT("[%d]unlock_all(uh_wins[%d]), instead of target rank %d\n",
                     user_rank, uh_win->targets[target_rank].local_user_rank, target_rank);
    mpi_errno = PMPI_Win_unlock_all(uh_win->targets[target_rank].uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
#else
    for (j = 0; j < MTCORE_NUM_H; j++) {
        int target_h_rank_in_uh = uh_win->targets[target_rank].h_ranks_in_uh[j];

        MTCORE_DBG_PRINT("[%d]unlock(Helper(%d), uh_wins[%d]), instead of target rank %d\n",
                         user_rank, target_h_rank_in_uh,
                         uh_win->targets[target_rank].local_user_rank, target_rank);

        mpi_errno = PMPI_Win_unlock(target_h_rank_in_uh, uh_win->targets[target_rank].uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
#endif

#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
    /* If target is itself, we need also release the lock of local rank  */
    if (user_rank == target_rank && uh_win->is_self_locked) {
        int local_uh_rank;
        PMPI_Comm_rank(uh_win->local_uh_comm, &local_uh_rank);

        MTCORE_DBG_PRINT("[%d]unlock self(%d, local_uh_win)\n", user_rank, local_uh_rank);
        mpi_errno = PMPI_Win_unlock(local_uh_rank, uh_win->local_uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        uh_win->is_self_locked = 0;
    }
#endif

#if (MTCORE_LOAD_OPT != MTCORE_LOAD_OPT_NON)
    uh_win->targets[target_rank].main_lock_stat = MTCORE_MAIN_LOCK_RESET;
#endif

    /* TODO: All the operations which we have not wrapped up will be failed, because they
     * are issued to user window. We need wrap up all operations.
     */

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

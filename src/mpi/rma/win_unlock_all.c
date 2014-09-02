#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

int MPI_Win_unlock_all(MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs;
    int i, j, k;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);
    PMPI_Comm_size(uh_win->user_comm, &user_nprocs);

    for (i = 0; i < user_nprocs; i++) {
        uh_win->targets[i].remote_lock_assert = 0;
    }

    /* Unlock all Helpers in corresponding uh-window of each target process. */
#ifdef MTCORE_ENABLE_SYNC_ALL_OPT

    /* Optimization for MPI implementations that have optimized lock_all.
     * However, user should be noted that, if MPI implementation issues lock messages
     * for every target even if it does not have any operation, this optimization
     * could lose performance and even lose asynchronous! */
    for (i = 0; i < uh_win->num_uh_wins; i++) {
        MTCORE_DBG_PRINT("[%d]unlock_all(uh_win 0x%x)\n", user_rank, uh_win->uh_wins[i]);
        mpi_errno = PMPI_Win_unlock_all(uh_win->uh_wins[i]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
#else
    for (i = 0; i < user_nprocs; i++) {
        for (j = 0; j < uh_win->targets[i].num_uh_wins; j++) {
            for (k = 0; k < MTCORE_NUM_H; k++) {
                int target_h_rank_in_uh = uh_win->targets[i].h_ranks_in_uh[k];

                MTCORE_DBG_PRINT("[%d]unlock(Helper(%d), uh_wins 0x%x), instead of "
                                 "target rank %d\n", user_rank, target_h_rank_in_uh,
                                 uh_win->targets[i].uh_wins[j], i);
                mpi_errno = PMPI_Win_unlock(target_h_rank_in_uh, uh_win->targets[i].uh_wins[j]);
                if (mpi_errno != MPI_SUCCESS)
                    goto fn_fail;
            }
        }
    }
#endif

#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
    if (uh_win->is_self_locked) {
        /* We need also release the lock of local rank */
        int local_uh_rank;
        PMPI_Comm_rank(uh_win->local_uh_comm, &local_uh_rank);

        MTCORE_DBG_PRINT("[%d]unlock self(%d, local_uh_win)\n", user_rank, local_uh_rank);
        mpi_errno = PMPI_Win_unlock(local_uh_rank, uh_win->local_uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        uh_win->is_self_locked = 0;
    }
#endif

#if defined(MTCORE_ENABLE_RUNTIME_LOAD_OPT)
    for (i = 0; i < user_nprocs; i++) {
        for (j = 0; j < uh_win->targets[i].num_segs; j++) {
            uh_win->targets[i].segs[j].main_lock_stat = MTCORE_MAIN_LOCK_RESET;
        }
    }
#endif

    /* TODO: All the operations which we have not wrapped up will be failed, because they
     * are issued to user window. We need wrap up all operations.
     */

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

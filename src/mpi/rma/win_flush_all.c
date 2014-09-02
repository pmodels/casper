#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

int MPI_Win_flush_all(MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs, local_uh_rank;
    int i, j, k;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);
    PMPI_Comm_rank(uh_win->local_uh_comm, &local_uh_rank);
    PMPI_Comm_size(uh_win->user_comm, &user_nprocs);

#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
    if (uh_win->is_self_locked) {
        /* Flush shared window for local communication (self-target). */
        MTCORE_DBG_PRINT("[%d]flush self(%d, local_uh_win)\n", user_rank, local_uh_rank);
        mpi_errno = PMPI_Win_flush(local_uh_rank, uh_win->local_uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
#endif

    /* Flush all Helpers in corresponding uh-window of each target process.. */
#ifdef MTCORE_ENABLE_SYNC_ALL_OPT

    /* Optimization for MPI implementations that have optimized lock_all.
     * However, user should be noted that, if MPI implementation issues lock messages
     * for every target even if it does not have any operation, this optimization
     * could lose performance and even lose asynchronous! */
    for (i = 0; i < uh_win->num_uh_win; i++) {
        MTCORE_DBG_PRINT("[%d]flush_all(uh_wins[%d])\n", user_rank, i);
        mpi_errno = PMPI_Win_flush_all(uh_win->uh_wins[i]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
#else

    /* We do not flush other user processes, otherwise it cannot be guaranteed
     * to be fully asynchronous. */
    for (i = 0; i < user_nprocs; i++) {
        for (j = 0; j < uh_win->targets[i].num_segs; j++) {
            /* RMA operations are only issued to the main helper, so we only flush it. */
            int main_h_off = uh_win->targets[i].segs[j].main_h_off;
            int target_h_rank_in_uh = uh_win->targets[i].h_ranks_in_uh[main_h_off];
            MTCORE_DBG_PRINT("[%d]flush(Helper(%d), uh_wins 0x%x), instead of "
                             "target rank %d seg %d\n", user_rank, target_h_rank_in_uh,
                             uh_win->targets[i].segs[j].uh_win, i, j);
            mpi_errno = PMPI_Win_flush(target_h_rank_in_uh, uh_win->targets[i].segs[j].uh_win);

#if (MTCORE_LOAD_OPT != MTCORE_LOAD_OPT_NON)
            if ((uh_win->targets[i].remote_lock_assert & MPI_MODE_NOCHECK) ||
                uh_win->targets[i].segs[j].main_lock_stat == MTCORE_MAIN_LOCK_GRANTED) {
                /* flush other helpers */
                for (k = 0; k < MTCORE_NUM_H; k++) {
                    if (k == main_h_off)
                        continue;

                    target_h_rank_in_uh = uh_win->targets[i].h_ranks_in_uh[k];
                    MTCORE_DBG_PRINT("[%d]flush(Helper(%d), uh_wins 0x%x), instead of "
                                     "target rank %d seg %d\n", user_rank, target_h_rank_in_uh,
                                     uh_win->targets[i].segs[j].uh_win, i, j);
                    mpi_errno = PMPI_Win_flush(target_h_rank_in_uh,
                                               uh_win->targets[i].segs[j].uh_win);
                }
            }
#endif
        }
    }
#endif /*end of MTCORE_ENABLE_SYNC_ALL_OPT */

#if (MTCORE_LOAD_OPT != MTCORE_LOAD_OPT_NON)
    for (i = 0; i < user_nprocs; i++) {
        for (j = 0; j < uh_win->targets[i].num_segs; j++) {
            /* Lock of main helper is granted, we can start load balancing from the next flush/unlock.
             * Note that only target which was issued operations to is guaranteed to be granted. */
            if (uh_win->targets[i].segs[j].main_lock_stat == MTCORE_MAIN_LOCK_OP_ISSUED) {
                uh_win->targets[i].segs[j].main_lock_stat = MTCORE_MAIN_LOCK_GRANTED;
                MTCORE_DBG_PRINT("[%d] main lock (rank %d, seg %d) granted\n", user_rank, i, j);
            }

            MTCORE_Reset_win_target_ordering(i, j, uh_win);
            MTCORE_Reset_win_target_load_opt(i, uh_win);
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

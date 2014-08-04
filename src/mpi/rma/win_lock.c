#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

int MPI_Win_lock(int lock_type, int target_rank, int assert, MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, target_local_rank = -1;
    int j;

    MTCORE_DBG_PRINT_FCNAME();

    mpi_errno = get_uh_win(win, &uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (uh_win > 0) {
        target_local_rank = uh_win->local_user_ranks[target_rank];
        PMPI_Comm_rank(uh_win->user_comm, &user_rank);

        /* Lock Helper processes in corresponding uh-window of target process. */
        for (j = 0; j < MTCORE_NUM_H; j++) {
            int target_h_rank_in_uh = uh_win->h_ranks_in_uh[target_rank * MTCORE_NUM_H + j];

            MTCORE_DBG_PRINT("[%d]lock(Helper(%d), uh_wins[%d]), instead of target rank %d\n",
                             user_rank, target_h_rank_in_uh, target_local_rank, target_rank);

            mpi_errno = PMPI_Win_lock(lock_type, target_h_rank_in_uh, assert,
                                      uh_win->uh_wins[target_local_rank]);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
        /*TODO: MTCORE_ENABLE_SYNC_ALL_OPT */
#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
        uh_win->is_self_locked = 0;
#endif

        if (user_rank == target_rank) {
            /* If target is itself, we need grant this lock before return.
             * However, the actual locked processes are the Helpers whose locks may be delayed by
             * most MPI implementation, thus we need a flush to force the lock to be granted.
             *
             * For performance reason, this operation is ignored if user passed information that
             * this process will not do local load/store on this window.
             */
            if (uh_win->is_self_lock_grant_required) {
                mpi_errno =
                    MTCORE_Win_grant_local_lock(uh_win->h_ranks_in_uh[target_rank * MTCORE_NUM_H],
                                                lock_type, assert, uh_win);
                if (mpi_errno != MPI_SUCCESS)
                    goto fn_fail;

#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
                /* Lock local rank so that operations can be executed through local target.
                 * Need grant lock on helper in advance due to permission check */
                int local_uh_rank;
                PMPI_Comm_rank(uh_win->local_uh_comm, &local_uh_rank);

                MTCORE_DBG_PRINT("[%d]lock self(%d, local_uh_win)\n", user_rank, local_uh_rank);
                mpi_errno = PMPI_Win_lock(lock_type, local_uh_rank, assert, uh_win->local_uh_win);
                if (mpi_errno != MPI_SUCCESS)
                    goto fn_fail;
                uh_win->is_self_locked = 1;
#endif
            }
        }
    }
    /* TODO: All the operations which we have not wrapped up will be failed, because they
     * are issued to user window. We need wrap up all operations.
     */
    else {
        mpi_errno = PMPI_Win_lock(lock_type, target_rank, assert, win);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

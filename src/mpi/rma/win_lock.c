#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

int MPI_Win_lock(int lock_type, int target_rank, int assert, MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, rank_in_local_uh = 0;
    int is_shared = 0;
    int target_node_id = -1;
    int target_local_rank;

    MTCORE_DBG_PRINT_FCNAME();

    mpi_errno = get_uh_win(win, &uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (uh_win > 0) {
        target_local_rank = uh_win->local_user_ranks[target_rank];
        PMPI_Comm_rank(uh_win->user_comm, &user_rank);

        mpi_errno = MTCORE_Get_node_ids(uh_win->user_group, 1, &target_rank, &target_node_id);
        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;

        /* Lock Helper processes in corresponding uh-window of target process. */
        MTCORE_DBG_PRINT("[%d]lock(Helper(%d), uh_wins[%d]), instead of %d, node_id %d\n",
                         user_rank, uh_win->h_ranks_in_uh[target_node_id], target_local_rank,
                         target_rank, target_node_id);

        mpi_errno = PMPI_Win_lock(lock_type, uh_win->h_ranks_in_uh[target_node_id],
                                  assert, uh_win->uh_wins[target_local_rank]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

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
                mpi_errno = MTCORE_Win_grant_local_lock(lock_type, assert, uh_win);
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

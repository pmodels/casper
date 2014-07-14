#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

int MPI_Win_lock_all(int assert, MPI_Win win)
{
    MPIASP_Win *ua_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs;
    int i;
    int *target_node_ids = NULL;
    int *target_ranks = NULL;

    MPIASP_DBG_PRINT_FCNAME();

    mpi_errno = get_ua_win(win, &ua_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (ua_win > 0) {
        PMPI_Comm_rank(ua_win->user_comm, &user_rank);
        PMPI_Comm_size(ua_win->user_comm, &user_nprocs);

        /* Lock all Helpers in corresponding ua-window of each target process. */
#ifdef MTCORE_ENABLE_SYNC_ALL_OPT

        /* Optimization for MPI implementations that have optimized lock_all.
         * However, user should be noted that, if MPI implementation issues lock messages
         * for every target even if it does not have any operation, this optimization
         * could lose performance and even lose asynchronous! */
        for (i = 0; i < ua_win->max_local_user_nprocs; i++) {
            MPIASP_DBG_PRINT("[%d]lock_all(ua_wins[%d])\n", user_rank, i);
            mpi_errno = PMPI_Win_lock_all(assert, ua_win->ua_wins[i]);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
#else
        target_ranks = calloc(user_nprocs, sizeof(int));
        target_node_ids = calloc(user_nprocs, sizeof(int));
        for (i = 0; i < user_nprocs; i++) {
            target_ranks[i] = i;
        }

        mpi_errno = MPIASP_Get_node_ids(ua_win->user_group, user_nprocs, target_ranks,
                                        target_node_ids);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* We do not lock other user processes, otherwise it cannot be guaranteed
         * to be fully asynchronous. */
        for (i = 0; i < user_nprocs; i++) {
            int target_local_rank = ua_win->local_user_ranks[i];

            MPIASP_DBG_PRINT("[%d]lock(Helper(%d), ua_wins[%d]), instead of %d, node_id %d\n",
                             user_rank, ua_win->asp_ranks_in_ua[target_node_ids[i]],
                             target_local_rank, i, target_node_ids[i]);
            mpi_errno = PMPI_Win_lock(MPI_LOCK_SHARED, ua_win->asp_ranks_in_ua[target_node_ids[i]],
                                      assert, ua_win->ua_wins[target_local_rank]);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
#endif

        ua_win->is_self_locked = 0;

        if (ua_win->is_self_lock_grant_required) {
            /* We need grant the local lock (self-target) before return.
             * However, the actual locked processes are the Helpers whose locks may be delayed by
             * most MPI implementation, thus we need a flush to force the lock to be granted.
             *
             * For performance reason, this operation is ignored if user passed information that
             * this process will not do local load/store on this window.
             */
            mpi_errno = MPIASP_Win_grant_local_lock(MPI_LOCK_SHARED, 0, ua_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;

            /* Lock local rank so that operations can be executed through local target.
             * Need grant lock on helper in advance due to permission check */
            int local_ua_rank;
            PMPI_Comm_rank(ua_win->local_ua_comm, &local_ua_rank);
            MPIASP_DBG_PRINT("[%d]lock self(%d, local_ua_win)\n", user_rank, local_ua_rank);

            mpi_errno = PMPI_Win_lock(MPI_LOCK_SHARED, local_ua_rank, 0, ua_win->local_ua_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
            ua_win->is_self_locked = 1;
        }
    }
    /* TODO: All the operations which we have not wrapped up will be failed, because they
     * are issued to user window. We need wrap up all operations.
     */
    else {
        mpi_errno = PMPI_Win_lock_all(assert, win);
    }

  fn_exit:
#ifndef MTCORE_ENABLE_SYNC_ALL_OPT
    if (target_ranks)
        free(target_ranks);
    if (target_node_ids)
        free(target_node_ids);
#endif

    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

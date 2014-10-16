#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

static inline int MTCORE_Win_flush_self_impl(MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;

#ifdef MTCORE_ENABLE_SYNC_ALL_OPT
    /* flush_all already flushed local target */
#else
    int user_rank;
    PMPI_Comm_rank(uh_win->user_comm, &user_rank);

    if (uh_win->is_self_locked) {
        /* Flush local window for local communication (self-target). */
        MTCORE_DBG_PRINT("[%d]flush self(%d, local win 0x%x)\n", user_rank,
                         uh_win->my_rank_in_uh_comm, uh_win->my_uh_win);
        mpi_errno = PMPI_Win_flush(uh_win->my_rank_in_uh_comm, uh_win->my_uh_win);
        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;
    }
#endif
    return mpi_errno;
}

static int MTCORE_Win_mixed_flush_all_impl(MPI_Win win, MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs;
    int i, j, k;

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);
    PMPI_Comm_size(uh_win->user_comm, &user_nprocs);

    /* Flush all Helpers in corresponding uh-window of each target process.. */
#ifdef MTCORE_ENABLE_SYNC_ALL_OPT

    /* Optimization for MPI implementations that have optimized lock_all.
     * However, user should be noted that, if MPI implementation issues lock messages
     * for every target even if it does not have any operation, this optimization
     * could lose performance and even lose asynchronous! */
    for (i = 0; i < uh_win->num_uh_wins; i++) {
        MTCORE_DBG_PRINT("[%d]flush_all(uh_win 0x%x)\n", user_rank, uh_win->uh_wins[i]);
        mpi_errno = PMPI_Win_flush_all(uh_win->uh_wins[i]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
#else

    /* TODO: track op issuing, only flush the helpers which receive ops. */
    for (i = 0; i < user_nprocs; i++) {
#if !defined(MTCORE_ENABLE_RUNTIME_LOAD_OPT)
        /* RMA operations are only issued to the main helper, so we only flush it. */
        for (j = 0; j < uh_win->targets[i].num_segs; j++) {
            int main_h_off = uh_win->targets[i].segs[j].main_h_off;
            int target_h_rank_in_uh = uh_win->targets[i].h_ranks_in_uh[main_h_off];
            MTCORE_DBG_PRINT("[%d]flush(Helper(%d), uh_wins 0x%x), instead of "
                             "target rank %d seg %d\n", user_rank, target_h_rank_in_uh,
                             uh_win->targets[i].segs[j].uh_win, i, j);

            mpi_errno = PMPI_Win_flush(target_h_rank_in_uh, uh_win->targets[i].segs[j].uh_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
#else
        /* RMA operations may be distributed to all helpers, so we should
         * flush all helpers on all windows. See discussion in win_flush. */
        for (k = 0; k < MTCORE_ENV.num_h; k++) {
            int target_h_rank_in_uh = uh_win->targets[i].h_ranks_in_uh[k];
            MTCORE_DBG_PRINT("[%d]flush(Helper(%d), uh_win 0x%x), instead of "
                             "target rank %d\n", user_rank, target_h_rank_in_uh,
                             uh_win->targets[i].uh_win, i);

            mpi_errno = PMPI_Win_flush(target_h_rank_in_uh, uh_win->targets[i].uh_win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
#endif /*end of MTCORE_ENABLE_RUNTIME_LOAD_OPT */
    }
#endif /*end of MTCORE_ENABLE_SYNC_ALL_OPT */

#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
    mpi_errno = MTCORE_Win_flush_self_impl(uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
#endif

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Win_flush_all(MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs;
    int i, j, k;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    if (uh_win == NULL) {
        /* normal window */
        return PMPI_Win_flush_all(win);
    }

    /* mtcore window starts */

    MTCORE_Assert((uh_win->info_args.epoch_type & MTCORE_EPOCH_LOCK) ||
                  (uh_win->info_args.epoch_type & MTCORE_EPOCH_LOCK_ALL));

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);
    PMPI_Comm_size(uh_win->user_comm, &user_nprocs);

    if (!(uh_win->info_args.epoch_type & MTCORE_EPOCH_LOCK)) {
        /* In lock_all only epoch, single window is shared by multiple targets. */

#ifdef MTCORE_ENABLE_SYNC_ALL_OPT

        /* Optimization for MPI implementations that have optimized lock_all.
         * However, user should be noted that, if MPI implementation issues lock messages
         * for every target even if it does not have any operation, this optimization
         * could lose performance and even lose asynchronous! */
        MTCORE_DBG_PRINT("[%d]flush_all(uh_win 0x%x)\n", user_rank, uh_win->uh_wins[0]);
        mpi_errno = PMPI_Win_flush_all(uh_win->uh_wins[0]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
#else
        /* Flush every helper once in the single window.
         * TODO: track op issuing, only flush the helpers which receive ops. */
        for (i = 0; i < uh_win->num_h_ranks_in_uh; i++) {
            mpi_errno = PMPI_Win_flush(uh_win->h_ranks_in_uh[i], uh_win->uh_wins[0]);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
#endif

#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
        mpi_errno = MTCORE_Win_flush_self_impl(uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
#endif

    }
    else {

        /* In lock_all/lock mixed epoch, separate windows are bound with each target. */
        mpi_errno = MTCORE_Win_mixed_flush_all_impl(win, uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

#if defined(MTCORE_ENABLE_RUNTIME_LOAD_OPT)
    for (i = 0; i < user_nprocs; i++) {
        for (j = 0; j < uh_win->targets[i].num_segs; j++) {
            /* Lock of main helper is granted, we can start load balancing from the next flush/unlock.
             * Note that only target which was issued operations to is guaranteed to be granted. */
            if (uh_win->targets[i].segs[j].main_lock_stat == MTCORE_MAIN_LOCK_OP_ISSUED) {
                uh_win->targets[i].segs[j].main_lock_stat = MTCORE_MAIN_LOCK_GRANTED;
                MTCORE_DBG_PRINT("[%d] main lock (rank %d, seg %d) granted\n", user_rank, i, j);
            }

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

#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

static int MTCORE_Fence_flush_all(MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs;
    int i, j, k;

    MTCORE_DBG_PRINT_FCNAME();

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);
    PMPI_Comm_size(uh_win->user_comm, &user_nprocs);

    /* Flush all helpers to finish the sequence of locally issued RMA operations */
#ifdef MTCORE_ENABLE_SYNC_ALL_OPT

    /* Optimization for MPI implementations that have optimized lock_all.
     * However, user should be noted that, if MPI implementation issues lock messages
     * for every target even if it does not have any operation, this optimization
     * could lose performance and even lose asynchronous! */
    MTCORE_DBG_PRINT("[%d]flush_all(active_win 0x%x)\n", user_rank, uh_win->active_win);
    mpi_errno = PMPI_Win_flush_all(uh_win->active_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
#else
    /* TODO: track op issuing, only flush the helpers which receive ops. */
    for (i = 0; i < uh_win->num_h_ranks_in_uh; i++) {
        mpi_errno = PMPI_Win_flush(uh_win->h_ranks_in_uh[i], uh_win->active_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
    mpi_errno = PMPI_Win_flush(uh_win->my_rank_in_uh_comm, uh_win->active_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
#endif

#endif

#if defined(MTCORE_ENABLE_RUNTIME_LOAD_OPT)
    for (i = 0; i < user_nprocs; i++) {
        for (j = 0; j < uh_win->targets[i].num_segs; j++) {
            /* Runtime load balancing is allowed in fence epoch because
             * 1. fence is a global collective call, all targets already "exposed" their epoch.
             * 2. no conflicting lock/lockall on fence window. */
            uh_win->targets[i].segs[j].main_lock_stat = MTCORE_MAIN_LOCK_GRANTED;
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

int MPI_Win_fence(int assert, MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    if (uh_win == NULL) {
        /* normal window */
        return PMPI_Win_fence(assert, win);
    }

    MTCORE_Assert((uh_win->info_args.epoch_type & MTCORE_EPOCH_FENCE));

    /* We do not support conflicting lock/fence epoch, because operations
     * must choose different window. Because user may not specify assert for the
     * last fence, we do not check the epoch status in lock/lockall. */
    if (uh_win->epoch_stat != MTCORE_WIN_NO_EPOCH && uh_win->epoch_stat != MTCORE_WIN_EPOCH_FENCE) {
        fprintf(stderr, "Wrong synchronization call! %d lock epoch and %d "
                "lockall epoch is still open\n", uh_win->lock_counter, uh_win->lockall_counter);
        mpi_errno = -1;
        goto fn_fail;
    }

    /* Eliminate flush_all if user explicitly specifies no preceding RMA calls. */
    if ((assert & MPI_MODE_NOPRECEDE) == 0) {
        mpi_errno = MTCORE_Fence_flush_all(uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* Eliminate win_sync if user explicitly specifies no preceding store. */
    if ((assert & MPI_MODE_NOSTORE) == 0) {
        mpi_errno = PMPI_Win_sync(uh_win->active_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* Cannot eliminate barrier for either no_precede or no_succeed.
     * In no_precede fence, it is used for synchronization between local store
     * and remote RMA; In no_succeed fence, it is also required to wait for
     * remote RMA completion. */
    mpi_errno = PMPI_Barrier(uh_win->user_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    uh_win->is_self_locked = 0;
#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
    /* During fence epoch, it is allowed to access local target directly */
    uh_win->is_self_locked = 1;
#endif

    /* Indicate epoch status, later operations will be redirected to active_win */
    uh_win->epoch_stat = MTCORE_WIN_EPOCH_FENCE;

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

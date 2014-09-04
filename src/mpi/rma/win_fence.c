#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

int MTCORE_Fence_win_release_locks(MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank;

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);

    MTCORE_DBG_PRINT("[%d]unlock_all(fence_win 0x%x)\n", user_rank, uh_win->fence_win);
    mpi_errno = PMPI_Win_unlock_all(uh_win->fence_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
    /* We need also release the lock of local rank */
    int local_uh_rank;
    PMPI_Comm_rank(uh_win->local_uh_comm, &local_uh_rank);

    MTCORE_DBG_PRINT("[%d]unlock self(%d, local_uh_win)\n", user_rank, local_uh_rank);
    mpi_errno = PMPI_Win_unlock(local_uh_rank, uh_win->local_uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
#endif

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int Fence_Win_unlock_all(MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs;
    int i, j, k;

    MTCORE_DBG_PRINT_FCNAME();

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);
    PMPI_Comm_size(uh_win->user_comm, &user_nprocs);

    for (i = 0; i < user_nprocs; i++) {
        uh_win->targets[i].remote_lock_assert = 0;
    }

    /* Unlock all Helpers in corresponding uh-window of each target process.
     * Since all processes are required to call fence, we do need worry about asynchronous. */
    MTCORE_DBG_PRINT("[%d]unlock_all(fence_win 0x%x)\n", user_rank, uh_win->fence_win);
    mpi_errno = PMPI_Win_unlock_all(uh_win->fence_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
    /* We need also release the lock of local rank */
    int local_uh_rank;
    PMPI_Comm_rank(uh_win->local_uh_comm, &local_uh_rank);

    MTCORE_DBG_PRINT("[%d]unlock self(%d, local_uh_win)\n", user_rank, local_uh_rank);
    mpi_errno = PMPI_Win_unlock(local_uh_rank, uh_win->local_uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    uh_win->is_self_locked = 0;
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

static int Fence_Win_lock_all(int assert, MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank, user_nprocs;
    int i, j, k;

    MTCORE_DBG_PRINT_FCNAME();

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);
    PMPI_Comm_size(uh_win->user_comm, &user_nprocs);

    for (i = 0; i < user_nprocs; i++) {
        uh_win->targets[i].remote_lock_assert = assert;
    }

    MTCORE_DBG_PRINT("[%d]lock_all, MPI_MODE_NOCHECK %d(assert %d)\n", user_rank,
                     (assert & MPI_MODE_NOCHECK) != 0, assert);

    /* Since all processes are required to call fence, we do need worry about asynchronous. */
    mpi_errno = PMPI_Win_lock_all(assert, uh_win->fence_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    uh_win->is_self_locked = 0;
#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
    /* Lock local rank so that operations can be executed through local target.
     * We do not need force lock in fence.*/
    int local_uh_rank;
    PMPI_Comm_rank(uh_win->local_uh_comm, &local_uh_rank);
    MTCORE_DBG_PRINT("[%d]lock self(%d, local_uh_win)\n", user_rank, local_uh_rank);

    mpi_errno = PMPI_Win_lock(MPI_LOCK_SHARED, local_uh_rank, 0, uh_win->local_uh_win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    uh_win->is_self_locked = 1;
#endif

#if defined(MTCORE_ENABLE_RUNTIME_LOAD_OPT)
    for (i = 0; i < user_nprocs; i++) {
        for (j = 0; j < uh_win->targets[i].num_segs; j++) {
            /* Runtime load balancing is allowed in fence epoch because
             * 1. fence is a global collective call, all targets already "exposed" their epoch.
             * 2. no conflicting lock/lockall on fence window. */
            uh_win->targets[i].segs[j].main_lock_stat = MTCORE_MAIN_LOCK_GRANTED;

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

int MPI_Win_fence(int assert, MPI_Win win)
{
    MTCORE_Win *uh_win;
    int mpi_errno = MPI_SUCCESS;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(win, uh_win);

    /* We do not support conflicting lock/fence epoch, because operations
     * must choose different window. Because user may not specify assert for the
     * last fence, we do not check the epoch status in lock/lockall. */
    if (uh_win->epoch_stat != MTCORE_WIN_NO_EPOCH && uh_win->epoch_stat != MTCORE_WIN_EPOCH_FENCE) {
        fprintf(stderr, "Wrong synchronization call! %d lock epoch and %d "
                "lockall epoch is still open\n", uh_win->lock_counter, uh_win->lockall_counter);
        mpi_errno = -1;
        goto fn_fail;
    }

    /* Should not consider user assert, unlock must be called if there is a
     * previous lock even if user gives wrong assert. */
    if (uh_win->fence_stat == MTCORE_FENCE_LOCKED) {
        mpi_errno = Fence_Win_unlock_all(uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
        uh_win->fence_stat = MTCORE_FENCE_UNLOCKED;
    }

    /* We only eliminate barrier if user explicitly specifies it is the first fence. */
    if (assert != MPI_MODE_NOPRECEDE) {
        mpi_errno = PMPI_Barrier(uh_win->user_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* Do not lock if user specifies no_succeed, it is the last fence. */
    if (assert != MPI_MODE_NOSUCCEED) {

        /* Fence allows following asserts, but all of them are not supported by lock,
         * thus we just give a empty assert to lock.
         *   MPI_MODE_NOSTORE
         *   MPI_MODE_NOPUT
         *   MPI_MODE_NOPRECEDE
         *   MPI_MODE_NOSUCCEED */
        mpi_errno = Fence_Win_lock_all(0, uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
        uh_win->fence_stat = MTCORE_FENCE_LOCKED;

        /* later operations will be redirected to fence_win until a lock/lockall
         * is called .*/
        uh_win->epoch_stat = MTCORE_WIN_EPOCH_FENCE;
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

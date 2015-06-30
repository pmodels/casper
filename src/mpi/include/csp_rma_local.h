/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSP_RMA_LOCAL_H_
#define CSP_RMA_LOCAL_H_

/* This header file includes all generic routines used in local RMA communication. */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "csp.h"

/* Lock self process on lock window.
 * It is called by both WIN_LOCK and WIN_LOCK_ALL. */
static inline int CSP_win_lock_self_impl(CSP_win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;

#ifdef CSP_ENABLE_SYNC_ALL_OPT
    /* lockall already locked window for local target */
#else
    CSP_win_target *target;
    int user_rank;

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    target = &(ug_win->targets[user_rank]);

    /* Note that we do not need call CSP_target_get_epoch_win here,
     * since it must be in LOCK epoch thus use per-target window,
     * or in LOCK_ALL only mode but this per-target window is pointed
     * to the global window.*/
    CSP_DBG_PRINT("[%d]lock self(%d, local win 0x%x)\n", user_rank,
                  ug_win->my_rank_in_ug_comm, target->ug_win);
    mpi_errno = PMPI_Win_lock(MPI_LOCK_SHARED, ug_win->my_rank_in_ug_comm,
                              MPI_MODE_NOCHECK, target->ug_win);
    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;
#endif

    ug_win->is_self_locked = 1;
    return mpi_errno;
}

/* Unlock self process on lock window.
 * It is called by both WIN_UNLOCK and WIN_UNLOCK_ALL. */
static inline int CSP_win_unlock_self_impl(CSP_win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;

#ifdef CSP_ENABLE_SYNC_ALL_OPT
    /* unlockall already released window for local target */
#else
    CSP_win_target *target;
    int user_rank;

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    target = &(ug_win->targets[user_rank]);

    if (ug_win->is_self_locked) {
        /* We need also release the lock of local rank */

        /* Note that we do not need call CSP_target_get_epoch_win here,
         * since it must be in LOCK epoch thus use per-target window,
         * or in LOCK_ALL only mode but this per-target window is pointed
         * to the global window.*/
        CSP_DBG_PRINT("[%d]unlock self(%d, local win 0x%x)\n", user_rank,
                      ug_win->my_rank_in_ug_comm, target->ug_win);
        mpi_errno = PMPI_Win_unlock(ug_win->my_rank_in_ug_comm, target->ug_win);
        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;
    }
#endif

    ug_win->is_self_locked = 0;
    return mpi_errno;
}

#ifdef CSP_ENABLE_LOCAL_LOCK_OPT
static inline int CSP_win_flush_self_impl(CSP_win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;

#ifdef CSP_ENABLE_SYNC_ALL_OPT
    /* flush_all already flushed local target */
#else
    CSP_win_target *target;
    int user_rank;

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    target = &(ug_win->targets[user_rank]);

    if (ug_win->is_self_locked) {
        /* Note that we do not need call CSP_target_get_epoch_win here,
         * since it must be in LOCK epoch thus use per-target window,
         * or in LOCK_ALL only mode but this per-target window is pointed
         * to the global window.*/
        CSP_DBG_PRINT("[%d]flush self(%d, local win 0x%x)\n", user_rank,
                      ug_win->my_rank_in_ug_comm, target->ug_win);
        mpi_errno = PMPI_Win_flush(ug_win->my_rank_in_ug_comm, target->ug_win);
    }
#endif
    return mpi_errno;
}
#endif

#endif /* CSP_RMA_LOCAL_H_ */

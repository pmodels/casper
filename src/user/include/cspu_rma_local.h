/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSPU_RMA_LOCAL_H_
#define CSPU_RMA_LOCAL_H_

/* This header file includes all generic routines used in local RMA communication. */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "cspu.h"

/* Get the window for lock/lockall epochs (do not check other epochs).
 * If only lock-all is used, then return the global window, otherwise return
 * per-target window. */
static inline void CSP_win_get_epoch_lock_win(CSP_win * ug_win, CSP_win_target * target,
                                              MPI_Win * win_ptr)
{
    if (ug_win->info_args.epoch_type & CSP_EPOCH_LOCK) {
        (*win_ptr) = target->ug_win;
    }
    else {
        (*win_ptr) = ug_win->active_win;
    }
}

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
    MPI_Win lock_win = MPI_WIN_NULL;

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    target = &(ug_win->targets[user_rank]);

    CSP_win_get_epoch_lock_win(ug_win, target, &lock_win);

    CSP_DBG_PRINT("[%d]lock self(%d, local win 0x%x)\n", user_rank,
                  ug_win->my_rank_in_ug_comm, lock_win);
    mpi_errno = PMPI_Win_lock(MPI_LOCK_SHARED, ug_win->my_rank_in_ug_comm,
                              MPI_MODE_NOCHECK, lock_win);
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
    MPI_Win lock_win = MPI_WIN_NULL;

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    target = &(ug_win->targets[user_rank]);

    if (ug_win->is_self_locked) {

        CSP_win_get_epoch_lock_win(ug_win, target, &lock_win);

        CSP_DBG_PRINT("[%d]unlock self(%d, local win 0x%x)\n", user_rank,
                      ug_win->my_rank_in_ug_comm, lock_win);
        mpi_errno = PMPI_Win_unlock(ug_win->my_rank_in_ug_comm, lock_win);
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
    MPI_Win lock_win = MPI_WIN_NULL;

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    target = &(ug_win->targets[user_rank]);

    CSP_win_get_epoch_lock_win(ug_win, target, &lock_win);

    if (ug_win->is_self_locked) {
        CSP_DBG_PRINT("[%d]flush self(%d, local win 0x%x)\n", user_rank,
                      ug_win->my_rank_in_ug_comm, lock_win);
        mpi_errno = PMPI_Win_flush(ug_win->my_rank_in_ug_comm, lock_win);
    }
#endif
    return mpi_errno;
}
#endif

#endif /* CSPU_RMA_LOCAL_H_ */

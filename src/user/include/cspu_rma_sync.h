/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSPU_RMA_SYNC_H_
#define CSPU_RMA_SYNC_H_

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "cspu.h"

/* This header file includes all generic routines used in RMA synchronization. */

extern int CSP_win_target_lock(int lock_type, int assert, int target_rank, CSP_win_t * ug_win);
extern int CSP_win_target_flush(int target_rank, CSP_win_t * ug_win);
extern int CSP_win_target_unlock(int target_rank, CSP_win_t * ug_win);
extern int CSP_win_global_flush_all(CSP_win_t * ug_win);
extern int CSP_win_target_flush_local(int target_rank, CSP_win_t * ug_win);

/* Receive buffer for receiving complete-wait sync message.
 * We don't need its value, so just use a global char variable to ensure
 * receive buffer is always allocated.*/
extern char wait_flg;
extern int CSP_recv_pscw_complete_msg(int post_grp_size, CSP_win_t * ug_win, int blocking,
                                      int *flag);


#define CSP_get_lock_type_name(lock_type) (lock_type == MPI_LOCK_SHARED ? "SHARED" : "EXCLUSIVE")
#define CSP_get_assert_name(assert) (assert & MPI_MODE_NOCHECK ? "NOCHECK" : (assert == 0 ? "none" : "other"))
#define CSP_get_win_type(win, ug_win) ((win) == ug_win->global_win ? "global_win" : "ug_wins")

/* ======================================================================
 * local synchronization
 * ====================================================================== */

static inline int CSP_win_grant_local_lock(int target_rank, CSP_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank, j;

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);

    /* force lock all the main ghosts for each segment */
    for (j = 0; j < ug_win->targets[target_rank].num_segs; j++) {
        int main_g_off = ug_win->targets[target_rank].segs[j].main_g_off;
        int target_g_rank_in_ug = ug_win->targets[target_rank].g_ranks_in_ug[main_g_off];

#ifdef CSP_ENABLE_GRANT_LOCK_HIDDEN_BYTE
        CSP_GRANT_LOCK_DATATYPE buf[1];
        mpi_errno = PMPI_Get(buf, 1, CSP_GRANT_LOCK_MPI_DATATYPE, target_g_rank_in_ug,
                             ug_win->grant_lock_g_offset, 1, CSP_GRANT_LOCK_MPI_DATATYPE,
                             ug_win->targets[target_rank].segs[j].ug_win);
#else
        /* Simply get 1 byte from start, it does not affect the result of other updates */
        char buf[1];
        mpi_errno = PMPI_Get(buf, 1, MPI_CHAR, target_g_rank_in_ug, 0,
                             1, MPI_CHAR, ug_win->targets[user_rank].segs[j].ug_win);
#endif
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        mpi_errno = PMPI_Win_flush(target_g_rank_in_ug,
                                   ug_win->targets[target_rank].segs[j].ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
        ug_win->targets[target_rank].segs[j].main_lock_stat = CSP_MAIN_LOCK_GRANTED;
#endif
        CSP_DBG_PRINT(" grant local lock(ghost(%d), ug_wins 0x%x) seg %d\n",
                      target_g_rank_in_ug, ug_win->targets[target_rank].segs[j].ug_win, j);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}


/* Lock self process on lock window.
 * It is called by both WIN_LOCK and WIN_LOCK_ALL (lock-exist mode).
 * Note that self lock is required for memory consistency, thus it must be
 * issued even if local PUT/GET optimization is disabled.*/
static inline int CSP_win_lock_self(CSP_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;

#if !defined(CSP_ENABLE_SYNC_ALL_OPT)
    CSP_win_target_t *target;
    int user_rank;
    MPI_Win lock_win = MPI_WIN_NULL;

    CSP_assert(ug_win->is_self_locked == 0);

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    target = &(ug_win->targets[user_rank]);
    lock_win = target->ug_win;  /* only lock-exist mode calls this routine */

    CSP_DBG_PRINT(" lock self(%d, local win 0x%x)\n", ug_win->my_rank_in_ug_comm, lock_win);
    mpi_errno = PMPI_Win_lock(MPI_LOCK_SHARED, ug_win->my_rank_in_ug_comm,
                              MPI_MODE_NOCHECK, lock_win);
    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;
#endif

    ug_win->is_self_locked = 1;
    return mpi_errno;
}

/* Unlock self process on lock window.
 * It is called by both WIN_UNLOCK and WIN_UNLOCK_ALL (lock-exist mode). */
static inline int CSP_win_unlock_self(CSP_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;

#if !defined(CSP_ENABLE_SYNC_ALL_OPT)
    CSP_win_target_t *target;
    int user_rank;
    MPI_Win lock_win = MPI_WIN_NULL;

    CSP_assert(ug_win->is_self_locked == 1);

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    target = &(ug_win->targets[user_rank]);
    lock_win = target->ug_win;  /* only lock-exist mode calls this routine */

    CSP_DBG_PRINT(" unlock self(%d, local win 0x%x)\n", ug_win->my_rank_in_ug_comm, lock_win);
    mpi_errno = PMPI_Win_unlock(ug_win->my_rank_in_ug_comm, lock_win);
    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;
#endif

    ug_win->is_self_locked = 0;
    return mpi_errno;
}

/* Flush the local process.
 * It is called in all flush{all}-included operations.
 * Note that the actual flush is only issued when local PUT/GET optimization is enabled. */
static inline int CSP_win_flush_self(CSP_win_t * ug_win CSP_ATTRIBUTE((unused)),
                                     int global_win_flag CSP_ATTRIBUTE((unused)))
{
    int mpi_errno = MPI_SUCCESS;
#if defined(CSP_ENABLE_LOCAL_RMA_OP_OPT) && !defined(CSP_ENABLE_SYNC_ALL_OPT)
    CSP_win_target_t *target;
    int user_rank;
    MPI_Win flush_win = MPI_WIN_NULL;

    CSP_assert(ug_win->is_self_locked == 1);

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    target = &(ug_win->targets[user_rank]);

    flush_win = target->ug_win;
    if (global_win_flag)
        flush_win = ug_win->global_win;

    CSP_DBG_PRINT(" flush self(%d, local win 0x%x)\n", ug_win->my_rank_in_ug_comm, flush_win);
    mpi_errno = PMPI_Win_flush(ug_win->my_rank_in_ug_comm, flush_win);
#endif
    return mpi_errno;
}


/* Locally flush the local process.
 * It is called in all flush_local{all}-included operations.
 * Note that the actual flush_local is only issued when local PUT/GET optimization is enabled. */
static inline int CSP_win_flush_local_self(CSP_win_t * ug_win CSP_ATTRIBUTE((unused)),
                                           int global_win_flag CSP_ATTRIBUTE((unused)))
{
    int mpi_errno = MPI_SUCCESS;
#if defined(CSP_ENABLE_LOCAL_RMA_OP_OPT) && !defined(CSP_ENABLE_SYNC_ALL_OPT)
    CSP_win_target_t *target;
    int user_rank;
    MPI_Win flush_win = MPI_WIN_NULL;

    CSP_assert(ug_win->is_self_locked == 1);

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    target = &(ug_win->targets[user_rank]);

    flush_win = target->ug_win;
    if (global_win_flag)
        flush_win = ug_win->global_win;

    CSP_DBG_PRINT(" flush_local self(%d, local win 0x%x)\n", ug_win->my_rank_in_ug_comm, flush_win);
    mpi_errno = PMPI_Win_flush_local(ug_win->my_rank_in_ug_comm, flush_win);
#endif
    return mpi_errno;
}

#endif /* CSPU_RMA_SYNC_H_ */

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSPU_RMA_SYNC_H_INCLUDED
#define CSPU_RMA_SYNC_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "cspu.h"

/* This header file includes all generic routines used in RMA synchronization. */

extern int CSPU_win_target_lock(int lock_type, int assert, int target_rank, CSPU_win_t * ug_win);
extern int CSPU_win_target_flush(int target_rank, CSPU_win_t * ug_win);
extern int CSPU_win_target_unlock(int target_rank, CSPU_win_t * ug_win);
extern int CSPU_win_global_flush_all(CSPU_win_t * ug_win);
extern int CSPU_win_target_flush_local(int target_rank, CSPU_win_t * ug_win);

/* Receive buffer for receiving complete-wait sync message.
 * We don't need its value, so just use a global char variable to ensure
 * receive buffer is always allocated.*/
extern char CSPU_pscw_wait_flg;
extern int CSPU_recv_pscw_complete_msg(int post_grp_size, CSPU_win_t * ug_win, int blocking,
                                       int *flag);


#define CSPU_GET_LOCK_TYPE_NAME(lock_type) (lock_type == MPI_LOCK_SHARED ? "SHARED" : "EXCLUSIVE")
#define CSPU_GET_ASSERT_NAME(assert) (assert & MPI_MODE_NOCHECK ? "NOCHECK" : (assert == 0 ? "none" : "other"))
#define CSPU_GET_WIN_TYPE(win, ug_win) ((win) == ug_win->global_win ? "global_win" : "ug_wins")

/* ======================================================================
 * local synchronization
 * ====================================================================== */

static inline int CSPU_win_grant_local_lock(int target_rank, CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank;

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_win->user_comm, &user_rank));

    /* force lock the main ghost */
    {
        int main_g_off = ug_win->targets[target_rank].main_g_off;
        int target_g_rank_in_ug = ug_win->targets[target_rank].g_ranks_in_ug[main_g_off];

#ifdef CSP_ENABLE_GRANT_LOCK_HIDDEN_BYTE
        CSP_GRANT_LOCK_DATATYPE buf[1];
        CSP_CALLMPI(JUMP, PMPI_Get(buf, 1, CSP_GRANT_LOCK_MPI_DATATYPE, target_g_rank_in_ug,
                                   ug_win->grant_lock_g_offset, 1, CSP_GRANT_LOCK_MPI_DATATYPE,
                                   ug_win->targets[target_rank].ug_win);
#else
        /* Simply get 1 byte from start, it does not affect the result of other updates */
        char buf[1];
        CSP_CALLMPI(JUMP, PMPI_Get(buf, 1, MPI_CHAR, target_g_rank_in_ug, 0,
                                   1, MPI_CHAR, ug_win->targets[user_rank].ug_win));
#endif

        CSP_CALLMPI(JUMP, PMPI_Win_flush(target_g_rank_in_ug, ug_win->targets[target_rank].ug_win));

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
        ug_win->targets[target_rank].main_lock_stat = CSPU_MAIN_LOCK_GRANTED;
#endif
        CSP_DBG_PRINT(" grant local lock(ghost(%d), ug_wins 0x%x)\n",
                      target_g_rank_in_ug, ug_win->targets[target_rank].ug_win);
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
static inline int CSPU_win_lock_self(CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;

#if !defined(CSP_ENABLE_SYNC_ALL_OPT)
    CSPU_win_target_t *target;
    int user_rank;
    MPI_Win lock_win = MPI_WIN_NULL;

    CSP_ASSERT(ug_win->is_self_locked == 0);

    CSP_CALLMPI(RETURN, PMPI_Comm_rank(ug_win->user_comm, &user_rank));
    target = &(ug_win->targets[user_rank]);
    lock_win = target->ug_win;  /* only lock-exist mode calls this routine */

    CSP_DBG_PRINT(" lock self(%d, local win 0x%x)\n", ug_win->my_rank_in_ug_comm, lock_win);
    CSP_CALLMPI(RETURN, PMPI_Win_lock(MPI_LOCK_SHARED, ug_win->my_rank_in_ug_comm,
                                      MPI_MODE_NOCHECK, lock_win));
#endif

    ug_win->is_self_locked = 1;
    return mpi_errno;
}

/* Unlock self process on lock window.
 * It is called by both WIN_UNLOCK and WIN_UNLOCK_ALL (lock-exist mode). */
static inline int CSPU_win_unlock_self(CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;

#if !defined(CSP_ENABLE_SYNC_ALL_OPT)
    CSPU_win_target_t *target;
    int user_rank;
    MPI_Win lock_win = MPI_WIN_NULL;

    CSP_ASSERT(ug_win->is_self_locked == 1);

    CSP_CALLMPI(RETURN, PMPI_Comm_rank(ug_win->user_comm, &user_rank));
    target = &(ug_win->targets[user_rank]);
    lock_win = target->ug_win;  /* only lock-exist mode calls this routine */

    CSP_DBG_PRINT(" unlock self(%d, local win 0x%x)\n", ug_win->my_rank_in_ug_comm, lock_win);
    CSP_CALLMPI(RETURN, PMPI_Win_unlock(ug_win->my_rank_in_ug_comm, lock_win));
#endif

    ug_win->is_self_locked = 0;
    return mpi_errno;
}

/* Flush the local process.
 * It is called in all flush{all}-included operations.
 * Note that the actual flush is only issued when local PUT/GET optimization is enabled. */
static inline int CSPU_win_flush_self(CSPU_win_t * ug_win CSP_ATTRIBUTE((unused)))
{
    int mpi_errno = MPI_SUCCESS;
#if defined(CSP_ENABLE_LOCAL_RMA_OP_OPT) && !defined(CSP_ENABLE_SYNC_ALL_OPT)
    CSPU_win_target_t *target;
    int user_rank;
    MPI_Win *win_ptr = NULL;

    CSP_ASSERT(ug_win->is_self_locked == 1);

    CSP_CALLMPI(RETURN, PMPI_Comm_rank(ug_win->user_comm, &user_rank));
    target = &(ug_win->targets[user_rank]);

    CSPU_TARGET_GET_EPOCH_WIN(target, ug_win, win_ptr);
    CSP_ASSERT(win_ptr != NULL);

    CSP_DBG_PRINT(" flush self(%d, local win 0x%x)\n", ug_win->my_rank_in_ug_comm, *win_ptr);
    CSP_CALLMPI(RETURN, PMPI_Win_flush(ug_win->my_rank_in_ug_comm, *win_ptr));
#endif
    return mpi_errno;
}


/* Locally flush the local process.
 * It is called in all flush_local{all}-included operations.
 * Note that the actual flush_local is only issued when local PUT/GET optimization is enabled. */
static inline int CSPU_win_flush_local_self(CSPU_win_t * ug_win CSP_ATTRIBUTE((unused)))
{
    int mpi_errno = MPI_SUCCESS;
#if defined(CSP_ENABLE_LOCAL_RMA_OP_OPT) && !defined(CSP_ENABLE_SYNC_ALL_OPT)
    CSPU_win_target_t *target;
    int user_rank;
    MPI_Win *win_ptr = NULL;

    CSP_ASSERT(ug_win->is_self_locked == 1);

    CSP_CALLMPI(RETURN, PMPI_Comm_rank(ug_win->user_comm, &user_rank));
    target = &(ug_win->targets[user_rank]);

    CSPU_TARGET_GET_EPOCH_WIN(target, ug_win, win_ptr);
    CSP_ASSERT(win_ptr != NULL);

    CSP_DBG_PRINT(" flush_local self(%d, local win 0x%x)\n", ug_win->my_rank_in_ug_comm, *win_ptr);
    CSP_CALLMPI(RETURN, PMPI_Win_flush_local(ug_win->my_rank_in_ug_comm, *win_ptr));
#endif
    return mpi_errno;
}

#endif /* CSPU_RMA_SYNC_H_INCLUDED */

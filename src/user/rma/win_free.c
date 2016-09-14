/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

/**
 * Release all window internal resources maintained by CASPER.
 */
int CSPU_win_release(CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int i;
    int user_nprocs;

    if (ug_win == NULL)
        goto fn_exit;

    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);

    /* Free windows. */

    /* Free ug_win before local_ug_win, because all the incoming operations
     * should be done before free shared buffers.
     *
     * We do not need additional barrier in CASPER for waiting all
     * operations complete, because Win_free already internally add a barrier
     * for waiting operations on that window complete. */
    if (ug_win->num_ug_wins > 0 && ug_win->ug_wins) {
        CSP_DBG_PRINT("\t free ug windows\n");
        for (i = 0; i < ug_win->num_ug_wins; i++) {
            if (ug_win->ug_wins[i] && ug_win->ug_wins[i] != MPI_WIN_NULL) {
                mpi_errno = PMPI_Win_free(&ug_win->ug_wins[i]);
                if (mpi_errno != MPI_SUCCESS)
                    goto fn_fail;
            }
        }
    }

    if (ug_win->global_win && ug_win->global_win != MPI_WIN_NULL) {
        CSP_DBG_PRINT("\t free global window\n");
        mpi_errno = PMPI_Win_free(&ug_win->global_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    if (ug_win->win && ug_win->win != MPI_WIN_NULL) {
        CSP_DBG_PRINT("\t free user window\n");
        mpi_errno = PMPI_Win_free(&ug_win->win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    if (ug_win->local_ug_win && ug_win->local_ug_win != MPI_WIN_NULL) {
        CSP_DBG_PRINT("\t free shared window\n");
        mpi_errno = PMPI_Win_free(&ug_win->local_ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* Free communicators.
     * ug_win->user_comm is created by user, will be freed by user. */
    if (ug_win->local_ug_comm && ug_win->local_ug_comm != MPI_COMM_NULL
        && ug_win->local_ug_comm != CSP_PROC.local_comm) {
        CSP_DBG_PRINT("\t free shared communicator\n");
        mpi_errno = PMPI_Comm_free(&ug_win->local_ug_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    if (ug_win->ug_comm && ug_win->ug_comm != MPI_COMM_NULL && ug_win->ug_comm != MPI_COMM_WORLD) {
        CSP_DBG_PRINT("\t free ug communicator\n");
        mpi_errno = PMPI_Comm_free(&ug_win->ug_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    if (ug_win->local_user_comm && ug_win->local_user_comm != MPI_COMM_NULL
        && ug_win->local_user_comm != CSP_PROC.user.u_local_comm) {
        CSP_DBG_PRINT("\t free local USER communicator\n");
        mpi_errno = PMPI_Comm_free(&ug_win->local_user_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    if (ug_win->user_root_comm && ug_win->user_root_comm != MPI_COMM_NULL
        && ug_win->user_root_comm != CSP_PROC.user.ur_comm) {
        CSP_DBG_PRINT("\t free ur communicator\n");
        mpi_errno = PMPI_Comm_free(&ug_win->user_root_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* Free groups. */

    if (ug_win->local_ug_group && ug_win->local_ug_group != MPI_GROUP_NULL) {
        mpi_errno = PMPI_Group_free(&ug_win->local_ug_group);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    if (ug_win->ug_group && ug_win->ug_group != MPI_GROUP_NULL) {
        mpi_errno = PMPI_Group_free(&ug_win->ug_group);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    if (ug_win->user_group && ug_win->user_group != MPI_GROUP_NULL) {
        mpi_errno = PMPI_Group_free(&ug_win->user_group);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* Free allocations. */

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    if (ug_win->g_ops_counts)
        free(ug_win->g_ops_counts);
    if (ug_win->g_bytes_counts)
        free(ug_win->g_bytes_counts);
#endif

    if (ug_win->targets) {
        for (i = 0; i < user_nprocs; i++) {
            if (ug_win->targets[i].base_g_offsets)
                free(ug_win->targets[i].base_g_offsets);
            if (ug_win->targets[i].g_ranks_in_ug)
                free(ug_win->targets[i].g_ranks_in_ug);
        }
        free(ug_win->targets);
    }
    if (ug_win->g_ranks_in_ug)
        free(ug_win->g_ranks_in_ug);
    if (ug_win->g_win_handles)
        free(ug_win->g_win_handles);
    if (ug_win->ug_wins)
        free(ug_win->ug_wins);

    free(ug_win);

    CSP_DBG_PRINT("Freed CASPER window\n");

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int issue_ghost_cmd(CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_cwp_pkt_t pkt;
    CSP_cwp_fnc_winfree_pkt_t *winfree_pkt = &pkt.u.fnc_winfree;
    MPI_Request *reqs = NULL;
    MPI_Status *stats = NULL;
    int i, user_local_rank = 0;

    reqs = CSP_calloc(CSP_ENV.num_g, sizeof(MPI_Request));
    stats = CSP_calloc(CSP_ENV.num_g, sizeof(MPI_Status));
    PMPI_Comm_rank(CSP_PROC.local_comm, &user_local_rank);

    /* Ensure all user roots have arrived before start lock. */
    mpi_errno = PMPI_Barrier(ug_win->user_root_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Lock ghost processes on all nodes. */
    mpi_errno = CSPU_mlock_acquire(ug_win->user_root_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* send command to root ghost. */
    CSP_cwp_init_pkt(CSP_CWP_FNC_WIN_FREE, &pkt);
    winfree_pkt->user_local_root = user_local_rank;

    mpi_errno = CSPU_cwp_issue(&pkt);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* send the handle of target ghost win.
     * Note that ghosts cannot fetch the corresponding window without handlers
     * so that only global communicator can be used here.*/
    for (i = 0; i < CSP_ENV.num_g; i++) {
        mpi_errno = PMPI_Isend(&ug_win->g_win_handles[i], 1, MPI_UNSIGNED_LONG,
                               CSP_PROC.user.g_lranks[i], CSP_CWP_PARAM_TAG, CSP_PROC.local_comm,
                               &reqs[i]);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    mpi_errno = PMPI_Waitall(CSP_ENV.num_g, reqs, stats);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    if (reqs)
        free(reqs);
    if (stats)
        free(stats);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}


int MPI_Win_free(MPI_Win * win)
{
    int mpi_errno = MPI_SUCCESS;
    CSPU_win_t *ug_win;
    int user_rank, user_nprocs, user_local_rank, user_local_nprocs;

    CSPU_fetch_ug_win_from_cache(*win, &ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_free(win);
    }

    /* casper window starts */

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);
    PMPI_Comm_rank(ug_win->local_user_comm, &user_local_rank);
    PMPI_Comm_size(ug_win->local_user_comm, &user_local_nprocs);

    /* First unlock global window */
    if ((ug_win->info_args.epochs_used & CSP_EPOCH_FENCE) ||
        (ug_win->info_args.epochs_used & CSP_EPOCH_PSCW) ||
        (ug_win->info_args.epochs_used == CSP_EPOCH_LOCK_ALL)) {

        CSP_DBG_PRINT("[%d]unlock_all(global_win 0x%x)\n", user_rank, ug_win->global_win);

        /* Since all processes must be in win_free, we do not need worry
         * the possibility losing asynchronous progress. */
        mpi_errno = PMPI_Win_unlock_all(ug_win->global_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* Local user root issues command to ghosts. */
    if (user_local_rank == 0) {
        mpi_errno = issue_ghost_cmd(ug_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    CSP_DBG_PRINT("\t free window cache\n");
    CSPU_remove_ug_win_from_cache(*win);

    /* Free PSCW arrays in case use does not call complete/wait. */
    if (ug_win->start_ranks_in_win_group)
        free(ug_win->start_ranks_in_win_group);
    if (ug_win->post_ranks_in_win_group)
        free(ug_win->post_ranks_in_win_group);
    if (ug_win->wait_reqs)
        free(ug_win->wait_reqs);

    /* Free all window resources. */
    mpi_errno = CSPU_win_release(ug_win);

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

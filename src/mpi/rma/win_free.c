/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "csp.h"

/**
 * Release all window internal resources maintained by CASPER.
 */
int CSP_win_release(CSP_win * ug_win)
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

    if (ug_win->active_win && ug_win->active_win != MPI_WIN_NULL) {
        CSP_DBG_PRINT("\t free active window\n");
        mpi_errno = PMPI_Win_free(&ug_win->active_win);
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

    if (ug_win->ur_g_comm && ug_win->ur_g_comm != MPI_COMM_NULL) {
        CSP_DBG_PRINT("\t free user root + ghosts communicator\n");
        mpi_errno = PMPI_Comm_free(&ug_win->ur_g_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    if (ug_win->local_ug_comm && ug_win->local_ug_comm != MPI_COMM_NULL
        && ug_win->local_ug_comm != CSP_COMM_LOCAL) {
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
        && ug_win->local_user_comm != CSP_COMM_USER_LOCAL) {
        CSP_DBG_PRINT("\t free local USER communicator\n");
        mpi_errno = PMPI_Comm_free(&ug_win->local_user_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    if (ug_win->user_root_comm && ug_win->user_root_comm != MPI_COMM_NULL
        && ug_win->user_root_comm != CSP_COMM_UR_WORLD) {
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
            if (ug_win->targets[i].segs)
                free(ug_win->targets[i].segs);
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

int MPI_Win_free(MPI_Win * win)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_win *ug_win;
    int user_rank, user_nprocs, user_local_rank, user_local_nprocs;
    int j;
    MPI_Request *reqs = NULL;
    MPI_Status *stats = NULL;

    CSP_DBG_PRINT_FCNAME();

    CSP_fetch_ug_win_from_cache(*win, ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_free(win);
    }

    /* casper window starts */

    PMPI_Comm_rank(ug_win->user_comm, &user_rank);
    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);
    PMPI_Comm_rank(ug_win->local_user_comm, &user_local_rank);
    PMPI_Comm_size(ug_win->local_user_comm, &user_local_nprocs);

    /* First unlock global active window */
    if ((ug_win->info_args.epoch_type & CSP_EPOCH_FENCE) ||
        (ug_win->info_args.epoch_type & CSP_EPOCH_PSCW)) {

        CSP_DBG_PRINT("[%d]unlock_all(active_win 0x%x)\n", user_rank, ug_win->active_win);

        /* Since all processes must be in win_free, we do not need worry
         * the possibility losing asynchronous progress. */
        mpi_errno = PMPI_Win_unlock_all(ug_win->active_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    if (user_local_rank == 0) {
        CSP_func_start(CSP_FUNC_WIN_FREE, user_nprocs, user_local_nprocs);
    }

    /* Notify the handle of target Ghost win. It is noted that ghosts cannot
     * fetch the corresponding window without handlers so that only global communicator
     * can be used here.*/
    if (user_local_rank == 0) {
        reqs = CSP_calloc(CSP_ENV.num_g, sizeof(MPI_Request));
        stats = CSP_calloc(CSP_ENV.num_g, sizeof(MPI_Status));

        for (j = 0; j < CSP_ENV.num_g; j++) {
            mpi_errno = PMPI_Isend(&ug_win->g_win_handles[j], 1, MPI_UNSIGNED_LONG,
                                   CSP_G_RANKS_IN_LOCAL[j], 0, CSP_COMM_LOCAL, &reqs[j]);
        }
        mpi_errno = PMPI_Waitall(CSP_ENV.num_g, reqs, stats);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    CSP_DBG_PRINT("\t free window cache\n");
    CSP_remove_ug_win_from_cache(*win);

    /* Free PSCW arrays in case use does not call complete/wait. */
    if (ug_win->start_ranks_in_win_group)
        free(ug_win->start_ranks_in_win_group);
    if (ug_win->post_ranks_in_win_group)
        free(ug_win->post_ranks_in_win_group);
    if (ug_win->wait_reqs)
        free(ug_win->wait_reqs);

    /* Free all window resources. */
    mpi_errno = CSP_win_release(ug_win);

  fn_exit:
    if (reqs)
        free(reqs);
    if (stats)
        free(stats);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

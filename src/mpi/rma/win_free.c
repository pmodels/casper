#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

int MPI_Win_free(MPI_Win * win)
{
    static const char FCNAME[] = "MTCORE_Win_free";
    int mpi_errno = MPI_SUCCESS;
    MTCORE_Win *uh_win;
    int user_rank, user_nprocs, user_local_rank, user_local_nprocs;
    int i, j;
    MPI_Request *reqs = NULL;
    MPI_Status *stats = NULL;

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_Fetch_uh_win_from_cache(*win, uh_win);

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);
    PMPI_Comm_size(uh_win->user_comm, &user_nprocs);
    PMPI_Comm_rank(uh_win->local_user_comm, &user_local_rank);
    PMPI_Comm_size(uh_win->local_user_comm, &user_local_nprocs);

    if (user_local_rank == 0) {
        MTCORE_Func_start(MTCORE_FUNC_WIN_FREE, user_nprocs, user_local_nprocs);
    }

    /* Notify the handle of target Helper win. It is noted that helpers cannot
     * fetch the corresponding window without handlers so that only global communicator
     * can be used here.*/
    if (user_local_rank == 0) {
        reqs = calloc(MTCORE_NUM_H, sizeof(MPI_Request));
        stats = calloc(MTCORE_NUM_H, sizeof(MPI_Status));

        for (j = 0; j < MTCORE_NUM_H; j++) {
            mpi_errno = PMPI_Isend(&uh_win->h_win_handles[j], 1, MPI_UNSIGNED_LONG,
                                   MTCORE_H_RANKS_IN_LOCAL[j], 0, MTCORE_COMM_LOCAL, &reqs[j]);
        }
        mpi_errno = PMPI_Waitall(MTCORE_NUM_H, reqs, stats);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    /* Free uh_win before local_uh_win, because all the incoming operations
     * should be done before free shared buffers.
     *
     * We do not need additional barrier in Manticore for waiting all
     * operations complete, because Win_free already internally add a barrier
     * for waiting operations on that window complete.
     */
    if (uh_win->uh_wins) {
        MTCORE_DBG_PRINT("\t free uh windows\n");
        for (i = 0; i < uh_win->max_local_user_nprocs; i++) {
            if (uh_win->uh_wins[i]) {
                mpi_errno = PMPI_Win_free(&uh_win->uh_wins[i]);
                if (mpi_errno != MPI_SUCCESS)
                    goto fn_fail;
            }
        }
    }

    if (uh_win->local_uh_win) {
        MTCORE_DBG_PRINT("\t free shared window\n");
        mpi_errno = PMPI_Win_free(&uh_win->local_uh_win);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    if (uh_win->user_group != MPI_GROUP_NULL) {
        mpi_errno = PMPI_Group_free(&uh_win->user_group);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    if (uh_win->ur_h_comm && uh_win->ur_h_comm != MPI_COMM_NULL) {
        MTCORE_DBG_PRINT("\t free user root + helpers communicator\n");
        mpi_errno = PMPI_Comm_free(&uh_win->ur_h_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    if (uh_win->local_uh_comm && uh_win->local_uh_comm != MTCORE_COMM_LOCAL) {
        MTCORE_DBG_PRINT("\t free shared communicator\n");
        mpi_errno = PMPI_Comm_free(&uh_win->local_uh_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
    if (uh_win->local_uh_group != MPI_GROUP_NULL) {
        mpi_errno = PMPI_Group_free(&uh_win->local_uh_group);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    if (uh_win->uh_comm != MPI_COMM_NULL && uh_win->uh_comm != MPI_COMM_WORLD) {
        MTCORE_DBG_PRINT("\t free uh communicator\n");
        mpi_errno = PMPI_Comm_free(&uh_win->uh_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
    if (uh_win->uh_group != MPI_GROUP_NULL) {
        mpi_errno = PMPI_Group_free(&uh_win->uh_group);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    if (uh_win->local_user_comm && uh_win->local_user_comm != MTCORE_COMM_USER_LOCAL) {
        MTCORE_DBG_PRINT("\t free local USER communicator\n");
        mpi_errno = PMPI_Comm_free(&uh_win->local_user_comm);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    MTCORE_DBG_PRINT("\t free window cache\n");
    MTCORE_Remove_uh_win_from_cache(*win);

    MTCORE_DBG_PRINT("\t free user window\n");
    mpi_errno = PMPI_Win_free(win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* uh_win->user_comm is created by user, will be freed by user. */

#if (MTCORE_LOAD_OPT == MTCORE_LOAD_OPT_COUNTING)
    if (uh_win->h_ops_counts)
        free(uh_win->h_ops_counts);
#endif
#if (MTCORE_LOAD_OPT == MTCORE_LOAD_BYTE_COUNTING)
    if (uh_win->h_bytes_counts)
        free(uh_win->h_bytes_counts);
#endif

    if (uh_win->targets) {
        for (i = 0; i < user_nprocs; i++) {
            if (uh_win->targets[i].base_h_offsets)
                free(uh_win->targets[i].base_h_offsets);
            if (uh_win->targets[i].h_ranks_in_uh)
                free(uh_win->targets[i].h_ranks_in_uh);
        }

        free(uh_win->targets);
    }

    if (uh_win->h_win_handles)
        free(uh_win->h_win_handles);
    if (uh_win->uh_wins)
        free(uh_win->uh_wins);

    free(uh_win);

    MTCORE_DBG_PRINT("Freed MTCORE window 0x%x\n", *win);

  fn_exit:
    if (reqs)
        free(reqs);
    if (stats)
        free(stats);
    return mpi_errno;

  fn_fail:

    goto fn_exit;
}

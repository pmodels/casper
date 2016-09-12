/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

static inline int issue_ghost_cmd(void)
{
    CSP_cwp_pkt_t pkt;

    CSP_cwp_init_pkt(CSP_CWP_FNC_FINALIZE, &pkt);

    /* send command to ghosts. */
    return CSPU_cwp_fnc_issue(&pkt);
}

/* Destroy global user process object */
int CSP_destroy_proc(void)
{
    int mpi_errno = MPI_SUCCESS;

    /* common objects */
    if (CSP_PROC.local_comm && CSP_PROC.local_comm != MPI_COMM_NULL) {
        CSP_DBG_PRINT(" free CSP_PROC.local_comm\n");
        PMPI_Comm_free(&CSP_PROC.local_comm);
    }

    if (CSP_PROC.wgroup && CSP_PROC.wgroup != MPI_GROUP_NULL)
        PMPI_Group_free(&CSP_PROC.wgroup);

    CSP_PROC.local_comm = MPI_COMM_NULL;
    CSP_PROC.wgroup = MPI_GROUP_NULL;

    /* user-specific objects */
    if (CSP_COMM_USER_WORLD && CSP_COMM_USER_WORLD != MPI_COMM_NULL) {
        CSP_DBG_PRINT(" free CSP_COMM_USER_WORLD\n");
        PMPI_Comm_free(&CSP_COMM_USER_WORLD);
    }

    if (CSP_PROC.user.u_local_comm && CSP_PROC.user.u_local_comm != MPI_COMM_NULL) {
        CSP_DBG_PRINT(" free CSP_PROC.user.u_local_comm\n");
        PMPI_Comm_free(&CSP_PROC.user.u_local_comm);
    }

    if (CSP_PROC.user.ur_comm && CSP_PROC.user.ur_comm != MPI_COMM_NULL) {
        CSP_DBG_PRINT(" free CSP_PROC.user.ur_comm\n");
        PMPI_Comm_free(&CSP_PROC.user.ur_comm);
    }

    if (CSP_PROC.user.g_lranks)
        free(CSP_PROC.user.g_lranks);

    if (CSP_PROC.user.g_wranks_per_user)
        free(CSP_PROC.user.g_wranks_per_user);

    if (CSP_PROC.user.g_wranks_unique)
        free(CSP_PROC.user.g_wranks_unique);

    CSP_COMM_USER_WORLD = MPI_COMM_NULL;
    CSP_PROC.user.u_local_comm = MPI_COMM_NULL;
    CSP_PROC.user.ur_comm = MPI_COMM_NULL;

    CSP_PROC.user.g_lranks = NULL;
    CSP_PROC.user.g_wranks_per_user = NULL;
    CSP_PROC.user.g_wranks_unique = NULL;

    return mpi_errno;
}

int MPI_Finalize(void)
{
    int mpi_errno = MPI_SUCCESS;
    int user_local_rank;

    PMPI_Comm_rank(CSP_PROC.user.u_local_comm, &user_local_rank);

    /* notify ghost processes to finalize */
    mpi_errno = issue_ghost_cmd();
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    CSP_destroy_proc();

    CSP_destroy_win_cache();

    mpi_errno = PMPI_Finalize();
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

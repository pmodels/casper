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
    return CSPU_cwp_issue(&pkt);
}

/* Destroy global user process object */
static int destroy_proc(void)
{
    int mpi_errno = MPI_SUCCESS;

    /* common objects */
    if (CSP_PROC.local_comm && CSP_PROC.local_comm != MPI_COMM_NULL) {
        CSP_DBG_PRINT(" free CSP_PROC.local_comm\n");
        CSP_CALLMPI(JUMP, PMPI_Comm_free(&CSP_PROC.local_comm));
    }

    if (CSP_PROC.wgroup && CSP_PROC.wgroup != MPI_GROUP_NULL) {
        CSP_CALLMPI(JUMP, PMPI_Group_free(&CSP_PROC.wgroup));
    }

    CSP_PROC.local_comm = MPI_COMM_NULL;
    CSP_PROC.wgroup = MPI_GROUP_NULL;

    /* user-specific objects */
    if (CSP_COMM_USER_WORLD && CSP_COMM_USER_WORLD != MPI_COMM_NULL) {
        CSP_DBG_PRINT(" free CSP_COMM_USER_WORLD\n");
        CSP_CALLMPI(JUMP, PMPI_Comm_free(&CSP_COMM_USER_WORLD));
    }

    if (CSP_PROC.user.u_local_comm && CSP_PROC.user.u_local_comm != MPI_COMM_NULL) {
        CSP_DBG_PRINT(" free CSP_PROC.user.u_local_comm\n");
        CSP_CALLMPI(JUMP, PMPI_Comm_free(&CSP_PROC.user.u_local_comm));
    }

    if (CSP_PROC.user.ur_comm && CSP_PROC.user.ur_comm != MPI_COMM_NULL) {
        CSP_DBG_PRINT(" free CSP_PROC.user.ur_comm\n");
        CSP_CALLMPI(JUMP, PMPI_Comm_free(&CSP_PROC.user.ur_comm));
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

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

/* Clean up cache for COMM_WORLDs' error handler wrappers. */
static void comm_errhan_cleanup_predefined(void)
{
    CSPU_comm_errhan_reset(MPI_COMM_WORLD);
    CSPU_comm_errhan_reset(CSP_COMM_USER_WORLD);
    CSPU_comm_errhan_reset(MPI_COMM_SELF);
}

int CSPU_global_finalize(void)
{
    int mpi_errno = MPI_SUCCESS;

    comm_errhan_cleanup_predefined();

    mpi_errno = CSPU_mlock_destroy();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    mpi_errno = CSPU_destroy_win_cache();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    mpi_errno = CSPU_errhan_destroy();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    mpi_errno = destroy_proc();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MPI_Finalize(void)
{
    int mpi_errno = MPI_SUCCESS;
    int user_local_rank;

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_PROC.user.u_local_comm, &user_local_rank));

    /* notify ghost processes to finalize */
    mpi_errno = issue_ghost_cmd();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    mpi_errno = CSPU_global_finalize();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    CSP_CALLMPI(JUMP, PMPI_Finalize());

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

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
    CSP_cmd_pkt_t pkt;

    CSP_cmd_init_fnc_pkt(&pkt.fnc);
    pkt.fnc.fnc_cmd = CSP_CMD_FNC_FINALIZE;
    pkt.fnc.lock_flag = 0;

    /* send command to ghosts. */
    return CSP_cmd_fnc_issue(&pkt);
}

int MPI_Finalize(void)
{
    int mpi_errno = MPI_SUCCESS;
    int user_local_rank;

    PMPI_Comm_rank(CSP_PROC.user_local_comm, &user_local_rank);

    CSP_DBG_PRINT_FCNAME();

    /* notify ghost processes to finalize */
    mpi_errno = issue_ghost_cmd();
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (CSP_COMM_USER_WORLD != MPI_COMM_NULL) {
        CSP_DBG_PRINT(" free CSP_COMM_USER_WORLD\n");
        PMPI_Comm_free(&CSP_COMM_USER_WORLD);
    }

    if (CSP_PROC.local_comm != MPI_COMM_NULL) {
        CSP_DBG_PRINT(" free CSP_PROC.local_comm\n");
        PMPI_Comm_free(&CSP_PROC.local_comm);
    }

    if (CSP_PROC.user_local_comm != MPI_COMM_NULL) {
        CSP_DBG_PRINT(" free CSP_PROC.user_local_comm\n");
        PMPI_Comm_free(&CSP_PROC.user_local_comm);
    }

    if (CSP_PROC.user_root_comm != MPI_COMM_NULL) {
        CSP_DBG_PRINT(" free CSP_PROC.user_root_comm\n");
        PMPI_Comm_free(&CSP_PROC.user_root_comm);
    }

    if (CSP_PROC.g_local_comm != MPI_COMM_NULL) {
        CSP_DBG_PRINT("free CSP_PROC.g_local_comm\n");
        PMPI_Comm_free(&CSP_PROC.g_local_comm);
    }

    if (CSP_PROC.wgroup != MPI_GROUP_NULL)
        PMPI_Group_free(&CSP_PROC.wgroup);

    CSP_destroy_win_cache();

    if (CSP_PROC.g_lranks)
        free(CSP_PROC.g_lranks);

    if (CSP_PROC.g_wranks_per_user)
        free(CSP_PROC.g_wranks_per_user);

    if (CSP_PROC.g_wranks_unique)
        free(CSP_PROC.g_wranks_unique);

    mpi_errno = PMPI_Finalize();
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

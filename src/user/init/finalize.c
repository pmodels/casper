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

    PMPI_Comm_rank(CSP_COMM_USER_LOCAL, &user_local_rank);

    CSP_DBG_PRINT_FCNAME();

    /* notify ghost processes to finalize */
    mpi_errno = issue_ghost_cmd();
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (CSP_COMM_USER_WORLD != MPI_COMM_NULL) {
        CSP_DBG_PRINT(" free CSP_COMM_USER_WORLD\n");
        PMPI_Comm_free(&CSP_COMM_USER_WORLD);
    }

    if (CSP_COMM_LOCAL != MPI_COMM_NULL) {
        CSP_DBG_PRINT(" free CSP_COMM_LOCAL\n");
        PMPI_Comm_free(&CSP_COMM_LOCAL);
    }

    if (CSP_COMM_USER_LOCAL != MPI_COMM_NULL) {
        CSP_DBG_PRINT(" free CSP_COMM_USER_LOCAL\n");
        PMPI_Comm_free(&CSP_COMM_USER_LOCAL);
    }

    if (CSP_COMM_UR_WORLD != MPI_COMM_NULL) {
        CSP_DBG_PRINT(" free CSP_COMM_UR_WORLD\n");
        PMPI_Comm_free(&CSP_COMM_UR_WORLD);
    }

    if (CSP_COMM_GHOST_LOCAL != MPI_COMM_NULL) {
        CSP_DBG_PRINT("free CSP_COMM_GHOST_LOCAL\n");
        PMPI_Comm_free(&CSP_COMM_GHOST_LOCAL);
    }

    if (CSP_GROUP_WORLD != MPI_GROUP_NULL)
        PMPI_Group_free(&CSP_GROUP_WORLD);
    if (CSP_GROUP_LOCAL != MPI_GROUP_NULL)
        PMPI_Group_free(&CSP_GROUP_LOCAL);

    CSP_destroy_win_cache();

    if (CSP_G_RANKS_IN_LOCAL)
        free(CSP_G_RANKS_IN_LOCAL);

    if (CSP_ALL_G_RANKS_IN_WORLD)
        free(CSP_ALL_G_RANKS_IN_WORLD);

    if (CSP_ALL_UNIQUE_G_RANKS_IN_WORLD)
        free(CSP_ALL_UNIQUE_G_RANKS_IN_WORLD);

    mpi_errno = PMPI_Finalize();
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

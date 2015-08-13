/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspg.h"

static int finalize_cnt = 0;

int CSPG_finalize(CSP_cmd_pkt_t * pkt CSP_ATTRIBUTE((unused)), int *exit_flag)
{
    int mpi_errno = MPI_SUCCESS;
    int local_nprocs, local_user_nprocs;

    finalize_cnt++;
    PMPI_Comm_size(CSP_COMM_LOCAL, &local_nprocs);
    local_user_nprocs = local_nprocs - CSP_ENV.num_g;

    CSPG_DBG_PRINT(" %d/%d processes already arrived finalize...\n",
                   finalize_cnt, local_user_nprocs);

    /* wait till all local processes arrive finalize.
     * Because every ghost is shared by multiple local user processes.*/
    if (finalize_cnt < local_user_nprocs) {
        (*exit_flag) = 0;
        goto fn_exit;
    }

    CSPG_DBG_PRINT(" All processes arrived finalize.\n");
    (*exit_flag) = 1;

    if (CSP_COMM_LOCAL != MPI_COMM_NULL) {
        CSPG_DBG_PRINT(" free CSP_COMM_LOCAL\n");
        PMPI_Comm_free(&CSP_COMM_LOCAL);
    }
    if (CSP_COMM_USER_WORLD != MPI_COMM_NULL) {
        CSPG_DBG_PRINT(" free CSP_COMM_USER_WORLD\n");
        PMPI_Comm_free(&CSP_COMM_USER_WORLD);
    }
    if (CSP_COMM_USER_LOCAL != MPI_COMM_NULL) {
        CSPG_DBG_PRINT(" free CSP_COMM_USER_LOCAL\n");
        PMPI_Comm_free(&CSP_COMM_USER_LOCAL);
    }
    if (CSP_COMM_UR_WORLD != MPI_COMM_NULL) {
        CSPG_DBG_PRINT(" free CSP_COMM_UR_WORLD\n");
        PMPI_Comm_free(&CSP_COMM_UR_WORLD);
    }
    if (CSP_COMM_GHOST_LOCAL != MPI_COMM_NULL) {
        CSPG_DBG_PRINT("free CSP_COMM_GHOST_LOCAL\n");
        PMPI_Comm_free(&CSP_COMM_GHOST_LOCAL);
    }

    if (CSP_GROUP_WORLD != MPI_GROUP_NULL)
        PMPI_Group_free(&CSP_GROUP_WORLD);
    if (CSP_GROUP_LOCAL != MPI_GROUP_NULL)
        PMPI_Group_free(&CSP_GROUP_LOCAL);

    if (CSP_G_RANKS_IN_LOCAL)
        free(CSP_G_RANKS_IN_LOCAL);

    if (CSP_ALL_G_RANKS_IN_WORLD)
        free(CSP_ALL_G_RANKS_IN_WORLD);

    if (CSP_ALL_UNIQUE_G_RANKS_IN_WORLD)
        free(CSP_ALL_UNIQUE_G_RANKS_IN_WORLD);

    CSPG_DBG_PRINT(" PMPI_Finalize\n");
    mpi_errno = PMPI_Finalize();
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    return mpi_errno;

  fn_fail:
    CSPG_ERR_PRINT("error happened in %s, abort\n", __FUNCTION__);
    PMPI_Abort(MPI_COMM_WORLD, 0);
    goto fn_exit;
}

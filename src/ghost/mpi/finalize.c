/*
 * finalize.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspg.h"

int CSP_G_finalize(void)
{
    int mpi_errno = MPI_SUCCESS;

    if (CSP_COMM_LOCAL != MPI_COMM_NULL) {
        CSP_G_DBG_PRINT(" free CSP_COMM_LOCAL\n");
        PMPI_Comm_free(&CSP_COMM_LOCAL);
    }
    if (CSP_COMM_USER_WORLD != MPI_COMM_NULL) {
        CSP_G_DBG_PRINT(" free CSP_COMM_USER_WORLD\n");
        PMPI_Comm_free(&CSP_COMM_USER_WORLD);
    }
    if (CSP_COMM_USER_LOCAL != MPI_COMM_NULL) {
        CSP_G_DBG_PRINT(" free CSP_COMM_USER_LOCAL\n");
        PMPI_Comm_free(&CSP_COMM_USER_LOCAL);
    }
    if (CSP_COMM_UR_WORLD != MPI_COMM_NULL) {
        CSP_G_DBG_PRINT(" free CSP_COMM_UR_WORLD\n");
        PMPI_Comm_free(&CSP_COMM_UR_WORLD);
    }
    if (CSP_COMM_GHOST_LOCAL != MPI_COMM_NULL) {
        CSP_G_DBG_PRINT("free CSP_COMM_GHOST_LOCAL\n");
        PMPI_Comm_free(&CSP_COMM_GHOST_LOCAL);
    }

    if (CSP_GROUP_WORLD != MPI_GROUP_NULL)
        PMPI_Group_free(&CSP_GROUP_WORLD);
    if (CSP_GROUP_LOCAL != MPI_GROUP_NULL)
        PMPI_Group_free(&CSP_GROUP_LOCAL);
    if (CSP_GROUP_USER_WORLD != MPI_GROUP_NULL)
        PMPI_Group_free(&CSP_GROUP_USER_WORLD);

    if (CSP_G_RANKS_IN_WORLD)
        free(CSP_G_RANKS_IN_WORLD);

    if (CSP_G_RANKS_IN_LOCAL)
        free(CSP_G_RANKS_IN_LOCAL);

    if (CSP_ALL_G_RANKS_IN_WORLD)
        free(CSP_ALL_G_RANKS_IN_WORLD);

    if (CSP_ALL_UNIQUE_G_RANKS_IN_WORLD)
        free(CSP_ALL_UNIQUE_G_RANKS_IN_WORLD);

    if (CSP_ALL_NODE_IDS)
        free(CSP_ALL_NODE_IDS);

    CSP_G_DBG_PRINT(" PMPI_Finalize\n");
    mpi_errno = PMPI_Finalize();
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    return mpi_errno;

  fn_fail:
    fprintf(stderr, "error happened in %s, abort\n", __FUNCTION__);
    PMPI_Abort(MPI_COMM_WORLD, 0);
    goto fn_exit;
}

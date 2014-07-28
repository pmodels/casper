/*
 * finalize.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include "mtcore_helper.h"

int MTCORE_H_finalize(void)
{
    int mpi_errno = MPI_SUCCESS;
    int rank, nprocs, local_rank, local_nprocs;

    if (MTCORE_COMM_LOCAL) {
        MTCORE_H_DBG_PRINT(" free MTCORE_COMM_LOCAL\n");
        PMPI_Comm_free(&MTCORE_COMM_LOCAL);
    }
    if (MTCORE_COMM_USER_WORLD) {
        MTCORE_H_DBG_PRINT(" free MTCORE_COMM_USER_WORLD\n");
        PMPI_Comm_free(&MTCORE_COMM_USER_WORLD);
    }
    if (MTCORE_COMM_USER_WORLD) {
        MTCORE_H_DBG_PRINT(" free MTCORE_COMM_USER_LOCAL\n");
        PMPI_Comm_free(&MTCORE_COMM_USER_LOCAL);
    }
    if (MTCORE_COMM_USER_ROOTS) {
        MTCORE_H_DBG_PRINT(" free MTCORE_COMM_USER_ROOTS\n");
        PMPI_Comm_free(&MTCORE_COMM_USER_ROOTS);
    }

    if (MTCORE_GROUP_WORLD != MPI_GROUP_NULL)
        PMPI_Group_free(&MTCORE_GROUP_WORLD);
    if (MTCORE_GROUP_LOCAL != MPI_GROUP_NULL)
        PMPI_Group_free(&MTCORE_GROUP_LOCAL);

    if (MTCORE_ALL_NODE_IDS)
        free(MTCORE_ALL_NODE_IDS);

    if (MTCORE_ALL_H_IN_COMM_WORLD)
        free(MTCORE_ALL_H_IN_COMM_WORLD);

    MTCORE_H_DBG_PRINT(" PMPI_Finalize\n");
    mpi_errno = PMPI_Finalize();
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    destroy_mtcore_h_win_table();

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

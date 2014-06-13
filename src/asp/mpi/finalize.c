/*
 * finalize.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include "asp.h"

int ASP_Finalize(void)
{
    int mpi_errno = MPI_SUCCESS;
    int rank, nprocs, local_rank, local_nprocs;

    if (MPIASP_COMM_LOCAL) {
        ASP_DBG_PRINT(" free MPIASP_COMM_LOCAL\n");
        PMPI_Comm_free(&MPIASP_COMM_LOCAL);
    }
    if (MPIASP_COMM_USER_WORLD) {
        ASP_DBG_PRINT(" free MPIASP_COMM_USER_WORLD\n");
        PMPI_Comm_free(&MPIASP_COMM_USER_WORLD);
    }
    if (MPIASP_COMM_USER_WORLD) {
        ASP_DBG_PRINT(" free MPIASP_COMM_USER_LOCAL\n");
        PMPI_Comm_free(&MPIASP_COMM_USER_LOCAL);
    }
    if (MPIASP_COMM_USER_ROOTS) {
        ASP_DBG_PRINT(" free MPIASP_COMM_USER_ROOTS\n");
        PMPI_Comm_free(&MPIASP_COMM_USER_ROOTS);
    }

    if (MPIASP_GROUP_WORLD != MPI_GROUP_NULL)
        PMPI_Group_free(&MPIASP_GROUP_WORLD);
    if (MPIASP_GROUP_LOCAL != MPI_GROUP_NULL)
        PMPI_Group_free(&MPIASP_GROUP_LOCAL);

    if (MPIASP_ALL_NODE_IDS)
        free(MPIASP_ALL_NODE_IDS);

    if (MPIASP_ALL_ASP_IN_COMM_WORLD)
        free(MPIASP_ALL_ASP_IN_COMM_WORLD);

    ASP_DBG_PRINT(" PMPI_Finalize\n");
    mpi_errno = PMPI_Finalize();
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    destroy_asp_win_table();

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

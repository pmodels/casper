/*
 * win_create_dynamic.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */
#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

int MPI_Win_create_dynamic(MPI_Info info, MPI_Comm comm, MPI_Win * win)
{
    int mpi_errno = MPI_SUCCESS;

    MTCORE_DBG_PRINT_FCNAME();

    if (comm == MPI_COMM_WORLD)
        comm = MTCORE_COMM_USER_WORLD;
    mpi_errno = PMPI_Win_create_dynamic(info, comm, win);

    MTCORE_WARN_PRINT("called MPI_Win_create_dynamic, no asynchronous progress on win 0x%x\n",
                      *win);

    return mpi_errno;
}

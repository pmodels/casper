/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *  (C) 2014 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 *
 */

#include <stdio.h>
#include "mpi.h"
#include "asp.h"

MPI_Comm asp_user_world = MPI_COMM_NULL;
MPI_Comm asp_local_comm = MPI_COMM_NULL;

int MPI_Init(int *argc, char ***argv)
{
    int ret = MPI_SUCCESS;
    int local_rank;

    ret = PMPI_Init(argc, argv);
    if (ret)
        goto fn_fail;

    /* Get a communicator only containing processes with shared memory */
    ret = PMPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &asp_local_comm);
    if (ret)
        goto fn_fail;

    /* Get a user comm_world for the user to use */
    PMPI_Comm_rank(asp_local_comm, &local_rank);
    ret = PMPI_Comm_split(MPI_COMM_WORLD, local_rank == 0, 0, &asp_user_world);
    if (ret)
        goto fn_fail;

fn_exit:
    return ret;

fn_fail:
    goto fn_exit;
}


int MPI_Finalize()
{
    int ret = MPI_SUCCESS;

    ret = PMPI_Comm_free(&asp_local_comm);
    if (ret)
        goto fn_fail;

    ret = PMPI_Comm_free(&asp_user_world);
    if (ret)
        goto fn_fail;

    ret = PMPI_Finalize();
    if (ret)
        goto fn_fail;

fn_exit:
    return ret;

fn_fail:
    goto fn_exit;
}

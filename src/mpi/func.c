/*
 * func.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

/**
 * The root process in current local user communicator ask helpers to start a new
 * function. A user root + helpers communicator will be created for later information
 * exchanges.
 */
int MTCORE_Func_start(MTCORE_Func FUNC, int user_nprocs, int user_local_nprocs)
{
    int mpi_errno = MPI_SUCCESS;
    MTCORE_Func_info info;

    info.FUNC = FUNC;
    info.user_nprocs = user_nprocs;
    info.user_local_nprocs = user_local_nprocs;

    MTCORE_DBG_PRINT("[%d] send Func start request to helper local %d\n",
                     MTCORE_MY_RANK_IN_WORLD, MTCORE_H_RANKS_IN_LOCAL[0]);
    /* Only send start request to root helper. */
    mpi_errno = PMPI_Send((char *) &info, sizeof(MTCORE_Func_info), MPI_CHAR,
                          MTCORE_H_RANKS_IN_LOCAL[0], MTCORE_FUNC_TAG, MTCORE_COMM_LOCAL);
    return mpi_errno;
}


int MTCORE_Func_new_ur_h_comm(MPI_Comm * ur_h_comm)
{
    int mpi_errno = MPI_SUCCESS;
    int local_rank;
    int *ur_h_ranks_in_local = NULL;
    MPI_Group ur_h_group = MPI_GROUP_NULL;
    int i;

    PMPI_Comm_rank(MTCORE_COMM_LOCAL, &local_rank);

    /* Create a user root + all helpers communicator for later information exchanges. */
    ur_h_ranks_in_local = calloc(sizeof(int), MTCORE_ENV.num_h + 1);
    /* Helpers' rank are always start from 0 */
    for (i = 0; i < MTCORE_ENV.num_h; i++)
        ur_h_ranks_in_local[i] = i;
    ur_h_ranks_in_local[MTCORE_ENV.num_h] = local_rank;

    PMPI_Group_incl(MTCORE_GROUP_LOCAL, MTCORE_ENV.num_h + 1, ur_h_ranks_in_local, &ur_h_group);
    mpi_errno = PMPI_Comm_create_group(MTCORE_COMM_LOCAL, ur_h_group, 0, ur_h_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

#ifdef DEBUG
    int ur_h_rank, ur_h_nprocs;
    PMPI_Comm_rank(*ur_h_comm, &ur_h_rank);
    PMPI_Comm_size(*ur_h_comm, &ur_h_nprocs);
    MTCORE_DBG_PRINT("[%d] created ur_h comm, my rank %d/%d \n", MTCORE_MY_RANK_IN_WORLD,
                     ur_h_rank, ur_h_nprocs);
#endif

  fn_exit:
    if (ur_h_ranks_in_local)
        free(ur_h_ranks_in_local);
    if (ur_h_group != MPI_GROUP_NULL)
        PMPI_Group_free(&ur_h_group);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MTCORE_Func_set_param(char *func_params, int size, MPI_Comm ur_h_comm)
{
    /* Simply broadcast parameters to all the helpers, because all of them are
     * guaranteed to be in current function.
     * User + all helpers communicator is organized as h0, h1,...,user root */
    MTCORE_DBG_PRINT("set param to helpers: size %d\n", size);
    return PMPI_Bcast(func_params, size, MPI_CHAR, MTCORE_ENV.num_h, ur_h_comm);
}

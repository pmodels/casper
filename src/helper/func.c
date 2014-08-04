/*
 * func.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include "mtcore_helper.h"

/**
 * Helpers receive a new function from user root process
 */
int MTCORE_H_func_start(MTCORE_Func * FUNC, int *user_local_root, int *user_nprocs,
                        int *user_local_nprocs)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Status status;
    MTCORE_H_func_info h_info;
    int local_helper_rank = 0;

    PMPI_Comm_rank(MTCORE_COMM_HELPER_LOCAL, &local_helper_rank);
    memset(&h_info, 0, sizeof(h_info));

    /* Only root helper receives start request from user roots.
     * Otherwise deadlock may happen if multiple user roots send request to
     * helpers concurrently and some helpers are locked in different communicator creation. */
    if (local_helper_rank == 0) {
        mpi_errno = PMPI_Recv((char *) &h_info, sizeof(MTCORE_Func_info), MPI_CHAR,
                              MPI_ANY_SOURCE, MTCORE_FUNC_TAG, MTCORE_COMM_LOCAL, &status);
        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;

        MTCORE_H_DBG_PRINT(" received Func start request from local rank %d\n", status.MPI_SOURCE);

        h_info.user_root_in_local = status.MPI_SOURCE;
    }

    /* All other helpers start from here */
    mpi_errno = PMPI_Bcast((char *) &h_info, sizeof(MTCORE_H_func_info), MPI_CHAR, 0,
                           MTCORE_COMM_HELPER_LOCAL);
    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;

    *FUNC = h_info.info.FUNC;
    *user_nprocs = h_info.info.user_nprocs;
    *user_local_nprocs = h_info.info.user_local_nprocs;
    *user_local_root = h_info.user_root_in_local;

    MTCORE_H_DBG_PRINT(" all helpers started for Func %d, user nprocs %d, local_nprocs %d,"
                       "user_local_root %d\n", *FUNC, *user_nprocs, *user_local_nprocs,
                       *user_local_root);

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int MTCORE_H_func_new_ur_h_comm(int user_local_root, MPI_Comm * ur_h_comm)
{
    int mpi_errno = MPI_SUCCESS;
    int *ur_h_ranks_in_local = NULL;
    MPI_Group ur_h_group = MPI_GROUP_NULL;
    int i;

    /* Create a user root + all helpers communicator for later information exchanges. */
    ur_h_ranks_in_local = calloc(sizeof(int), MTCORE_NUM_H + 1);
    /* Helpers' rank are always start from 0 */
    for (i = 0; i < MTCORE_NUM_H; i++)
        ur_h_ranks_in_local[i] = i;
    ur_h_ranks_in_local[MTCORE_NUM_H] = user_local_root;

    PMPI_Group_incl(MTCORE_GROUP_LOCAL, MTCORE_NUM_H + 1, ur_h_ranks_in_local, &ur_h_group);
    mpi_errno = PMPI_Comm_create_group(MTCORE_COMM_LOCAL, ur_h_group, 0, ur_h_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

#ifdef DEBUG
    int ur_h_rank, ur_h_nprocs;
    PMPI_Comm_rank(*ur_h_comm, &ur_h_rank);
    PMPI_Comm_size(*ur_h_comm, &ur_h_nprocs);
    MTCORE_H_DBG_PRINT(" created ur_h comm, my rank %d/%d \n", ur_h_rank, ur_h_nprocs);
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

int MTCORE_H_func_get_param(char *func_params, int size, MPI_Comm ur_h_comm)
{
    int local_user_rank, i;
    MPI_Status status;
    int mpi_errno = MPI_SUCCESS;

    /* User root is always the last rank in user root + helpers communicator */
    MTCORE_H_DBG_PRINT("get param from user root: size %d\n", size);
    return PMPI_Bcast(func_params, size, MPI_CHAR, MTCORE_NUM_H, ur_h_comm);
}

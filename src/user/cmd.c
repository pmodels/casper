/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "csp.h"

/**
 * The root process in current local user communicator ask ghosts to start a new
 * function. A user root + ghosts communicator will be created for later information
 * exchanges.
 */
int CSP_cmd_start(CSP_cmd CMD, int user_nprocs, int user_local_nprocs)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_cmd_info info;

    info.CMD = CMD;
    info.user_nprocs = user_nprocs;
    info.user_local_nprocs = user_local_nprocs;

    CSP_DBG_PRINT("[%d] send CMD start request to ghost local %d\n",
                  CSP_MY_RANK_IN_WORLD, CSP_G_RANKS_IN_LOCAL[0]);
    /* Only send start request to root ghost. */
    mpi_errno = PMPI_Send((char *) &info, sizeof(CSP_cmd_info), MPI_CHAR,
                          CSP_G_RANKS_IN_LOCAL[0], CSP_CMD_TAG, CSP_COMM_LOCAL);
    return mpi_errno;
}


int CSP_cmd_new_ur_g_comm(MPI_Comm * ur_g_comm)
{
    int mpi_errno = MPI_SUCCESS;
    int local_rank;
    int *ur_g_ranks_in_local = NULL;
    MPI_Group ur_g_group = MPI_GROUP_NULL;
    int i;

    PMPI_Comm_rank(CSP_COMM_LOCAL, &local_rank);

    /* Create a user root + all ghosts communicator for later information exchanges. */
    ur_g_ranks_in_local = CSP_calloc(sizeof(int), CSP_ENV.num_g + 1);
    /* Ghosts' rank are always start from 0 */
    for (i = 0; i < CSP_ENV.num_g; i++)
        ur_g_ranks_in_local[i] = i;
    ur_g_ranks_in_local[CSP_ENV.num_g] = local_rank;

    PMPI_Group_incl(CSP_GROUP_LOCAL, CSP_ENV.num_g + 1, ur_g_ranks_in_local, &ur_g_group);
    mpi_errno = PMPI_Comm_create_group(CSP_COMM_LOCAL, ur_g_group, 0, ur_g_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

#ifdef CSP_DEBUG
    int ur_g_rank, ur_g_nprocs;
    PMPI_Comm_rank(*ur_g_comm, &ur_g_rank);
    PMPI_Comm_size(*ur_g_comm, &ur_g_nprocs);
    CSP_DBG_PRINT("[%d] created ur_g comm, my rank %d/%d \n", CSP_MY_RANK_IN_WORLD,
                  ur_g_rank, ur_g_nprocs);
#endif

  fn_exit:
    if (ur_g_ranks_in_local)
        free(ur_g_ranks_in_local);
    if (ur_g_group != MPI_GROUP_NULL)
        PMPI_Group_free(&ur_g_group);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

int CSP_cmd_set_param(char *cmd_params, int size, MPI_Comm ur_g_comm)
{
    /* Simply broadcast parameters to all the ghosts, because all of them are
     * guaranteed to be in current function.
     * User + all ghosts communicator is organized as h0, h1,...,user root */
    CSP_DBG_PRINT("set param to ghosts: size %d\n", size);
    return PMPI_Bcast(cmd_params, size, MPI_CHAR, CSP_ENV.num_g, ur_g_comm);
}

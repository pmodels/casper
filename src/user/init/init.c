/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

CSP_define_win_cache;

#ifdef CSP_DEBUG
void CSP_print_proc(void)
{
    int rank, nprocs, local_rank, local_nprocs, user_rank, user_nprocs;
    int local_user_rank, local_user_nprocs;
    int i, j;

    CSP_assert(CSP_IS_USER);

    PMPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
    PMPI_Comm_size(CSP_PROC.local_comm, &local_nprocs);
    PMPI_Comm_rank(CSP_PROC.local_comm, &local_rank);
    PMPI_Comm_size(CSP_COMM_USER_WORLD, &user_nprocs);
    PMPI_Comm_rank(CSP_COMM_USER_WORLD, &user_rank);
    PMPI_Comm_size(CSP_PROC.user.u_local_comm, &local_user_nprocs);
    PMPI_Comm_rank(CSP_PROC.user.u_local_comm, &local_user_rank);

    CSP_DBG_PRINT("I am user: %d/%d in world, %d/%d in local\n"
                  "           %d/%d in user world, %d/%d in user local\n"
                  "           node_id %d\n",
                  rank, nprocs, local_rank, local_nprocs,
                  user_rank, user_nprocs, local_user_rank, local_user_nprocs, CSP_PROC.node_id);

    CSP_DBG_PRINT("g_lranks: ");
    for (j = 0; j < CSP_ENV.num_g; j++)
        fprintf(stdout, "%d,", CSP_PROC.user.g_lranks[j]);
    fprintf(stdout, "\n");
    fflush(stdout);

    CSP_DBG_PRINT("g_wranks_per_user:\n");
    for (i = 0; i < user_nprocs; i++) {
        CSP_DBG_PRINT("    user=%d: ", i);
        for (j = 0; j < CSP_ENV.num_g; j++)
            fprintf(stdout, "%d,", CSP_PROC.user.g_wranks_per_user[i * CSP_ENV.num_g + j]);
        fprintf(stdout, "\n");
        fflush(stdout);
    }

    CSP_DBG_PRINT("g_wranks_unique:\n");
    for (i = 0; i < CSP_PROC.num_nodes; i++) {
        CSP_DBG_PRINT("    node=%d: ", i);
        for (j = 0; j < CSP_ENV.num_g; j++)
            fprintf(stdout, "%d,", CSP_PROC.user.g_wranks_unique[i * CSP_ENV.num_g + j]);
        fprintf(stdout, "\n");
        fflush(stdout);
    }
}
#endif

/* Setup global user-specific information. */
int CSP_setup_proc(void)
{
    int mpi_errno = MPI_SUCCESS;
    int *tmp_gather_buf = NULL;
    int *g_ranks_in_world = NULL;
    int user_rank, user_nprocs;
    MPI_Group user_world_group = MPI_GROUP_NULL, local_group = MPI_GROUP_NULL;
    int i, j;

    PMPI_Comm_rank(CSP_COMM_USER_WORLD, &user_rank);
    PMPI_Comm_size(CSP_COMM_USER_WORLD, &user_nprocs);

    CSP_PROC.user.g_lranks = CSP_calloc(CSP_ENV.num_g, sizeof(int));
    CSP_PROC.user.g_wranks_per_user = CSP_calloc(user_nprocs * CSP_ENV.num_g, sizeof(int));
    CSP_PROC.user.g_wranks_unique = CSP_calloc(CSP_PROC.num_nodes * CSP_ENV.num_g, sizeof(int));

    g_ranks_in_world = CSP_calloc(CSP_ENV.num_g, sizeof(int));
    tmp_gather_buf = CSP_calloc(user_nprocs * (1 + CSP_ENV.num_g), sizeof(int));

    mpi_errno = PMPI_Comm_group(CSP_COMM_USER_WORLD, &user_world_group);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    mpi_errno = PMPI_Comm_group(CSP_PROC.local_comm, &local_group);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Specify the first num_g local processes as ghosts on each node */
    for (i = 0; i < CSP_ENV.num_g; i++)
        CSP_PROC.user.g_lranks[i] = i;

    /* Translate ghost ranks in world */
    mpi_errno = PMPI_Group_translate_ranks(local_group, CSP_ENV.num_g,
                                           CSP_PROC.user.g_lranks, CSP_PROC.wgroup,
                                           g_ranks_in_world);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Gather ghost ranks among all users in world */
    tmp_gather_buf[user_rank * (1 + CSP_ENV.num_g)] = CSP_PROC.node_id; /* node_id */
    for (i = 0; i < CSP_ENV.num_g; i++) /* ghost ranks in world */
        tmp_gather_buf[user_rank * (1 + CSP_ENV.num_g) + i + 1] = g_ranks_in_world[i];

    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                               tmp_gather_buf, 1 + CSP_ENV.num_g, MPI_INT, CSP_COMM_USER_WORLD);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    for (i = 0; i < user_nprocs; i++) {
        int node_id = 0;
        node_id = tmp_gather_buf[i * (1 + CSP_ENV.num_g)];

        for (j = 0; j < CSP_ENV.num_g; j++) {
            CSP_PROC.user.g_wranks_per_user[i * CSP_ENV.num_g + j] =
                tmp_gather_buf[i * (1 + CSP_ENV.num_g) + j + 1];
            CSP_PROC.user.g_wranks_unique[node_id * CSP_ENV.num_g + j] =
                tmp_gather_buf[i * (1 + CSP_ENV.num_g) + j + 1];
        }
    }

  fn_exit:
    if (user_world_group != MPI_GROUP_NULL)
        PMPI_Group_free(&user_world_group);
    if (local_group != MPI_GROUP_NULL)
        PMPI_Group_free(&local_group);
    if (tmp_gather_buf)
        free(tmp_gather_buf);
    if (g_ranks_in_world)
        free(g_ranks_in_world);
    return mpi_errno;

  fn_fail:
    /* Free global objects in main function. */
    goto fn_exit;
}

int CSP_init(void)
{
    int mpi_errno = MPI_SUCCESS;

    CSP_init_win_cache();

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

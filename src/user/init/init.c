/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

CSP_DEFINE_WIN_CACHE;

#ifdef CSP_DEBUG
static int dbg_print_proc(void)
{
    int mpi_errno = MPI_SUCCESS;
    int rank, nprocs, local_rank, local_nprocs, user_rank, user_nprocs;
    int local_user_rank, local_user_nprocs;
    int i, j;

    CSP_ASSERT(CSP_IS_USER);

    CSP_CALLMPI(RETURN, PMPI_Comm_size(MPI_COMM_WORLD, &nprocs));
    CSP_CALLMPI(RETURN, PMPI_Comm_rank(MPI_COMM_WORLD, &rank));
    CSP_CALLMPI(RETURN, PMPI_Comm_size(CSP_PROC.local_comm, &local_nprocs));
    CSP_CALLMPI(RETURN, PMPI_Comm_rank(CSP_PROC.local_comm, &local_rank));
    CSP_CALLMPI(RETURN, PMPI_Comm_size(CSP_COMM_USER_WORLD, &user_nprocs));
    CSP_CALLMPI(RETURN, PMPI_Comm_rank(CSP_COMM_USER_WORLD, &user_rank));
    CSP_CALLMPI(RETURN, PMPI_Comm_size(CSP_PROC.user.u_local_comm, &local_user_nprocs));
    CSP_CALLMPI(RETURN, PMPI_Comm_rank(CSP_PROC.user.u_local_comm, &local_user_rank));

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

    return mpi_errno;
}
#endif

/* Setup global user-specific information. */
static int setup_proc(int is_threaded)
{
    int mpi_errno = MPI_SUCCESS;
    int *tmp_gather_buf = NULL;
    int *g_ranks_in_world = NULL;
    int user_rank, user_nprocs;
    MPI_Group user_world_group = MPI_GROUP_NULL, local_group = MPI_GROUP_NULL;
    int i, j;

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_COMM_USER_WORLD, &user_rank));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(CSP_COMM_USER_WORLD, &user_nprocs));

    /* Initialize threading protection on every user process */
    CSP_PROC.user.is_thread_multiple = is_threaded;

    CSP_PROC.user.g_lranks = CSP_calloc(CSP_ENV.num_g, sizeof(int));
    CSP_PROC.user.g_wranks_per_user = CSP_calloc(user_nprocs * CSP_ENV.num_g, sizeof(int));
    CSP_PROC.user.g_wranks_unique = CSP_calloc(CSP_PROC.num_nodes * CSP_ENV.num_g, sizeof(int));
    CSP_PROC.user.comm_errhan_wrapper = MPI_ERRHANDLER_NULL;    /* created in CSPU_errhan_init. */

    g_ranks_in_world = CSP_calloc(CSP_ENV.num_g, sizeof(int));
    tmp_gather_buf = CSP_calloc(user_nprocs * (1 + CSP_ENV.num_g), sizeof(int));

    CSP_CALLMPI(JUMP, PMPI_Comm_group(CSP_COMM_USER_WORLD, &user_world_group));
    CSP_CALLMPI(JUMP, PMPI_Comm_group(CSP_PROC.local_comm, &local_group));

    /* Specify the first num_g local processes as ghosts on each node */
    for (i = 0; i < CSP_ENV.num_g; i++)
        CSP_PROC.user.g_lranks[i] = i;

    /* Translate ghost ranks in world */
    CSP_CALLMPI(JUMP, PMPI_Group_translate_ranks(local_group, CSP_ENV.num_g,
                                                 CSP_PROC.user.g_lranks, CSP_PROC.wgroup,
                                                 g_ranks_in_world));

    /* Gather ghost ranks among all users in world */
    tmp_gather_buf[user_rank * (1 + CSP_ENV.num_g)] = CSP_PROC.node_id; /* node_id */
    for (i = 0; i < CSP_ENV.num_g; i++) /* ghost ranks in world */
        tmp_gather_buf[user_rank * (1 + CSP_ENV.num_g) + i + 1] = g_ranks_in_world[i];

    CSP_CALLMPI(JUMP, PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                                     tmp_gather_buf, 1 + CSP_ENV.num_g, MPI_INT,
                                     CSP_COMM_USER_WORLD));

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

#ifdef CSP_DEBUG
    dbg_print_proc();
#endif

  fn_exit:
    if (user_world_group != MPI_GROUP_NULL)
        CSP_CALLMPI_EXIT(PMPI_Group_free(&user_world_group));
    if (local_group != MPI_GROUP_NULL)
        CSP_CALLMPI_EXIT(PMPI_Group_free(&local_group));
    if (tmp_gather_buf)
        free(tmp_gather_buf);
    if (g_ranks_in_world)
        free(g_ranks_in_world);
    return mpi_errno;

  fn_fail:
    /* Free global objects in main function. */
    goto fn_exit;
}

/* Setup the error handler wrapper for COMM_WORLDs and set RETURN for all
 * internal communicators. */
static int comm_errhan_wrap_predefined(void)
{
    int mpi_errno = MPI_SUCCESS;

    /* Set error handling wrapper for predefined communicators (COMM_WORLDs, SELF).
     * Thus we can separately handle the error caused by user MPI calls,
     * and the errors happened in CASPER overwriting calls (RETURN). */
    mpi_errno = CSPU_comm_errhan_wrap(MPI_COMM_WORLD, MPI_ERRORS_ARE_FATAL, NULL);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    mpi_errno = CSPU_comm_errhan_wrap(CSP_COMM_USER_WORLD, MPI_ERRORS_ARE_FATAL, NULL);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    mpi_errno = CSPU_comm_errhan_wrap(MPI_COMM_SELF, MPI_ERRORS_ARE_FATAL, NULL);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* Set RETURN error handler for all internal communicators.
     * Thus any error happened on them will be returned and handled by the
     * first level in CASPER. */
    CSPU_COMM_ERRHAN_SET_INTERN(CSP_PROC.local_comm);
    CSPU_COMM_ERRHAN_SET_INTERN(CSP_PROC.user.u_local_comm);
    if (CSP_PROC.user.ur_comm && CSP_PROC.user.ur_comm != MPI_COMM_NULL) {
        /* only the first user process on every node holds the ur_comm. */
        CSPU_COMM_ERRHAN_SET_INTERN(CSP_PROC.user.ur_comm);
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    /* Free global objects in main function. */
    goto fn_exit;
}

int CSPU_global_init(int is_threaded)
{
    int mpi_errno = MPI_SUCCESS;

    mpi_errno = setup_proc(is_threaded);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    mpi_errno = CSPU_init_win_cache();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    mpi_errno = CSPU_errhan_init();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    mpi_errno = CSPU_mlock_init();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    mpi_errno = comm_errhan_wrap_predefined();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    return mpi_errno;
  fn_fail:
    /* Do not release global objects, they are released at MPI_Init_thread. */
    goto fn_exit;
}

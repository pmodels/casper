/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "csp.h"
#include "cspg.h"

MPI_Comm CSP_COMM_USER_WORLD = MPI_COMM_NULL;
MPI_Comm CSP_COMM_LOCAL = MPI_COMM_NULL;
MPI_Comm CSP_COMM_USER_LOCAL = MPI_COMM_NULL;
MPI_Comm CSP_COMM_UR_WORLD = MPI_COMM_NULL;
MPI_Comm CSP_COMM_GHOST_LOCAL = MPI_COMM_NULL;
MPI_Group CSP_GROUP_WORLD = MPI_GROUP_NULL;
MPI_Group CSP_GROUP_LOCAL = MPI_GROUP_NULL;
MPI_Group CSP_GROUP_USER_WORLD = MPI_GROUP_NULL;

int CSP_NUM_G = CSP_DEFAULT_NG;
int *CSP_G_RANKS_IN_WORLD = NULL;
int *CSP_G_RANKS_IN_LOCAL = NULL;
int *CSP_ALL_G_RANKS_IN_WORLD = NULL;   /* Ghosts of user process x are stored as
                                         * [x*num_g : (x+1)*num_g-1] */
int *CSP_ALL_UNIQUE_G_RANKS_IN_WORLD = NULL;
int *CSP_USER_RANKS_IN_WORLD = NULL;

int CSP_MY_NODE_ID = -1;
int CSP_NUM_NODES = 0;
int *CSP_ALL_NODE_IDS = NULL;
int CSP_MY_RANK_IN_WORLD = -1;

CSP_Define_win_cache;

/* TODO: Move load balancing option into env setting */
CSP_Env_param CSP_ENV;

static int CSP_Initialize_env()
{
    char *val;
    int mpi_errno = MPI_SUCCESS;

    memset(&CSP_ENV, 0, sizeof(CSP_ENV));

    CSP_ENV.seg_size = CSP_DEFAULT_SEG_SIZE;
    val = getenv("CSP_SEG_SIZE");
    if (val && strlen(val)) {
        CSP_ENV.seg_size = atoi(val);
    }
    if (CSP_ENV.seg_size <= 0) {
        fprintf(stderr, "Wrong CSP_SEG_SIZE %d\n", CSP_ENV.seg_size);
        return -1;
    }

    CSP_ENV.num_g = CSP_DEFAULT_NG;
    val = getenv("CSP_NG");
    if (val && strlen(val)) {
        CSP_ENV.num_g = atoi(val);
    }
    if (CSP_ENV.num_g <= 0) {
        fprintf(stderr, "Wrong CSP_NG %d\n", CSP_ENV.num_g);
        return -1;
    }
    CSP_NUM_G = CSP_ENV.num_g;  /* expose to outside programs */

    CSP_ENV.lock_binding = CSP_LOCK_BINDING_RANK;
    val = getenv("CSP_LOCK_METHOD");
    if (val && strlen(val)) {
        if (!strncmp(val, "rank", strlen("rank"))) {
            CSP_ENV.lock_binding = CSP_LOCK_BINDING_RANK;
        }
        else if (!strncmp(val, "segment", strlen("segment"))) {
            CSP_ENV.lock_binding = CSP_LOCK_BINDING_SEGMENT;
        }
        else {
            fprintf(stderr, "Unknown CSP_LOCK_METHOD %s\n", val);
            return -1;
        }
    }

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    CSP_ENV.load_opt = CSP_LOAD_OPT_RANDOM;

    val = getenv("CSP_RUMTIME_LOAD_OPT");
    if (val && strlen(val)) {
        if (!strncmp(val, "random", strlen("random"))) {
            CSP_ENV.load_opt = CSP_LOAD_OPT_RANDOM;
        }
        else if (!strncmp(val, "op", strlen("op"))) {
            CSP_ENV.load_opt = CSP_LOAD_OPT_COUNTING;
        }
        else if (!strncmp(val, "byte", strlen("byte"))) {
            CSP_ENV.load_opt = CSP_LOAD_BYTE_COUNTING;
        }
        else {
            fprintf(stderr, "Unknown CSP_RUMTIME_LOAD_OPT %s\n", val);
            return -1;
        }
    }

    CSP_ENV.load_lock = CSP_LOAD_LOCK_NATURE;
    val = getenv("CSP_RUNTIME_LOAD_LOCK");
    if (val && strlen(val)) {
        if (!strncmp(val, "nature", strlen("nature"))) {
            CSP_ENV.load_lock = CSP_LOAD_LOCK_NATURE;
        }
        else if (!strncmp(val, "force", strlen("force"))) {
            CSP_ENV.load_lock = CSP_LOAD_LOCK_FORCE;
        }
        else {
            fprintf(stderr, "Unknown CSP_RUNTIME_LOAD_LOCK %s\n", val);
            return -1;
        }
    }
#else
    CSP_ENV.load_opt = CSP_LOAD_OPT_STATIC;
    CSP_ENV.load_lock = CSP_LOAD_LOCK_NATURE;
#endif

    CSP_DBG_PRINT("ENV: seg_size=%d, lock_binding=%d, load_lock=%d, load_opt=%d, "
                  "num_g=%d\n", CSP_ENV.seg_size, CSP_ENV.lock_binding,
                  CSP_ENV.load_lock, CSP_ENV.load_opt, CSP_ENV.num_g);

    return mpi_errno;
}

int MPI_Init_thread(int *argc, char ***argv, int required, int *provided)
{
    int mpi_errno = MPI_SUCCESS;
    int i, j;
    int local_rank, local_nprocs, rank, nprocs, user_rank, user_nprocs;
    int local_user_rank = -1, local_user_nprocs = -1;
    int *tmp_gather_buf = NULL, node_id = 0;
    int tmp_bcast_buf[2];
    int *ranks_in_user_world = NULL, *ranks_in_world = NULL;

    CSP_DBG_PRINT_FCNAME();

    if (required == 0 && provided == NULL) {
        /* default init */
        mpi_errno = PMPI_Init(argc, argv);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
    else {
        /* user init thread */
        mpi_errno = PMPI_Init_thread(argc, argv, required, provided);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

    PMPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
    CSP_MY_RANK_IN_WORLD = rank;

    mpi_errno = CSP_Initialize_env();
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Get a communicator only containing processes with shared memory */
    mpi_errno = PMPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0,
                                     MPI_INFO_NULL, &CSP_COMM_LOCAL);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Check number of ghosts and number of processes */
    PMPI_Comm_rank(CSP_COMM_LOCAL, &local_rank);
    PMPI_Comm_size(CSP_COMM_LOCAL, &local_nprocs);

    if (local_nprocs < 2) {
        fprintf(stderr, "No user process found, please run with more than 2 process per node\n");
        mpi_errno = -1;
        goto fn_fail;
    }
    if (CSP_ENV.num_g < 1 || CSP_ENV.num_g >= local_nprocs) {
        fprintf(stderr, "Wrong value of number of ghosts, %d. lt 1 or ge %d.\n",
                CSP_ENV.num_g, local_nprocs);
        mpi_errno = -1;
        goto fn_fail;
    }

    /* Specify the first N local processes to be Ghost processes */
    CSP_G_RANKS_IN_LOCAL = CSP_Calloc(CSP_ENV.num_g, sizeof(int));
    CSP_G_RANKS_IN_WORLD = CSP_Calloc(CSP_ENV.num_g, sizeof(int));
    for (i = 0; i < CSP_ENV.num_g; i++) {
        CSP_G_RANKS_IN_LOCAL[i] = i;
    }
    mpi_errno = PMPI_Comm_group(MPI_COMM_WORLD, &CSP_GROUP_WORLD);
    mpi_errno = PMPI_Comm_group(CSP_COMM_LOCAL, &CSP_GROUP_LOCAL);

    mpi_errno = PMPI_Group_translate_ranks(CSP_GROUP_LOCAL, CSP_ENV.num_g,
                                           CSP_G_RANKS_IN_LOCAL, CSP_GROUP_WORLD,
                                           CSP_G_RANKS_IN_WORLD);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Create a user comm_world including all the users,
     * user will access it instead of comm_world */
    mpi_errno = PMPI_Comm_split(MPI_COMM_WORLD,
                                local_rank < CSP_ENV.num_g, 0, &CSP_COMM_USER_WORLD);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    PMPI_Comm_size(CSP_COMM_USER_WORLD, &user_nprocs);
    PMPI_Comm_rank(CSP_COMM_USER_WORLD, &user_rank);
    PMPI_Comm_group(CSP_COMM_USER_WORLD, &CSP_GROUP_USER_WORLD);

    /* Create a user comm_local */
    mpi_errno = PMPI_Comm_split(CSP_COMM_LOCAL,
                                local_rank < CSP_ENV.num_g, 0, &CSP_COMM_USER_LOCAL);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Create a ghost comm_local */
    mpi_errno = PMPI_Comm_split(CSP_COMM_LOCAL,
                                local_rank < CSP_ENV.num_g, 1, &CSP_COMM_GHOST_LOCAL);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Exchange node id among local processes */
    /* -Only users create a user root communicator for exchanging local informations
     * between different nodes*/
    if (local_rank >= CSP_ENV.num_g) {
        PMPI_Comm_rank(CSP_COMM_USER_LOCAL, &local_user_rank);
        PMPI_Comm_size(CSP_COMM_USER_LOCAL, &local_user_nprocs);
        mpi_errno = PMPI_Comm_split(CSP_COMM_USER_WORLD,
                                    local_user_rank == 0, 1, &CSP_COMM_UR_WORLD);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* -Only user roots determine a node id for each USER processes */
        if (local_user_rank == 0) {
            PMPI_Comm_size(CSP_COMM_UR_WORLD, &CSP_NUM_NODES);
            PMPI_Comm_rank(CSP_COMM_UR_WORLD, &CSP_MY_NODE_ID);

            tmp_bcast_buf[0] = CSP_MY_NODE_ID;
            tmp_bcast_buf[1] = CSP_NUM_NODES;
        }
    }
    /* -User root broadcasts to other local processes */
    PMPI_Bcast(tmp_bcast_buf, 2, MPI_INT, CSP_ENV.num_g, CSP_COMM_LOCAL);
    CSP_MY_NODE_ID = tmp_bcast_buf[0];
    CSP_NUM_NODES = tmp_bcast_buf[1];

    /* Exchange node id and Ghost ranks among world processes */
    ranks_in_world = CSP_Calloc(nprocs, sizeof(int));
    ranks_in_user_world = CSP_Calloc(nprocs, sizeof(int));
    for (i = 0; i < nprocs; i++) {
        ranks_in_world[i] = i;
    }
    mpi_errno = PMPI_Group_translate_ranks(CSP_GROUP_WORLD, nprocs,
                                           ranks_in_world, CSP_GROUP_USER_WORLD,
                                           ranks_in_user_world);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    CSP_ALL_NODE_IDS = CSP_Calloc(nprocs, sizeof(int));
    CSP_ALL_G_RANKS_IN_WORLD = CSP_Calloc(user_nprocs * CSP_ENV.num_g, sizeof(int));
    CSP_ALL_UNIQUE_G_RANKS_IN_WORLD = CSP_Calloc(CSP_NUM_NODES * CSP_ENV.num_g, sizeof(int));
    tmp_gather_buf = CSP_Calloc(nprocs * (1 + CSP_ENV.num_g), sizeof(int));

    tmp_gather_buf[rank * (1 + CSP_ENV.num_g)] = CSP_MY_NODE_ID;
    for (i = 0; i < CSP_ENV.num_g; i++) {
        tmp_gather_buf[rank * (1 + CSP_ENV.num_g) + i + 1] = CSP_G_RANKS_IN_WORLD[i];
    }
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                               tmp_gather_buf, 1 + CSP_ENV.num_g, MPI_INT, MPI_COMM_WORLD);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    for (i = 0; i < nprocs; i++) {
        int i_user_rank = 0;
        node_id = tmp_gather_buf[i * (1 + CSP_ENV.num_g)];
        CSP_ALL_NODE_IDS[i] = node_id;

        /* Only copy ghost ranks for user processes */
        i_user_rank = ranks_in_user_world[i];
        if (i_user_rank != MPI_UNDEFINED) {
            for (j = 0; j < CSP_ENV.num_g; j++) {
                CSP_ALL_G_RANKS_IN_WORLD[i_user_rank * CSP_ENV.num_g + j] =
                    tmp_gather_buf[i * (1 + CSP_ENV.num_g) + j + 1];
                CSP_ALL_UNIQUE_G_RANKS_IN_WORLD[node_id * CSP_ENV.num_g + j] =
                    tmp_gather_buf[i * (1 + CSP_ENV.num_g) + j + 1];
            }
        }
    }

#ifdef DEBUG
    CSP_DBG_PRINT("Debug gathered info ***** \n");
    for (i = 0; i < nprocs; i++) {
        CSP_DBG_PRINT("node_id[%d]: %d\n", i, CSP_ALL_NODE_IDS[i]);
    }
#endif

    /* USER processes */
    if (local_rank >= CSP_ENV.num_g) {
        /* Get user ranks in world */
        for (i = 0; i < user_nprocs; i++)
            ranks_in_user_world[i] = i;
        CSP_USER_RANKS_IN_WORLD = CSP_Calloc(user_nprocs, sizeof(int));
        mpi_errno = PMPI_Group_translate_ranks(CSP_GROUP_USER_WORLD, user_nprocs,
                                               ranks_in_user_world, CSP_GROUP_WORLD,
                                               CSP_USER_RANKS_IN_WORLD);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

#ifdef DEBUG
        for (i = 0; i < user_nprocs; i++) {
            CSP_DBG_PRINT("gp_rank_in_world[%d]:\n", i);
            for (j = 0; j < CSP_ENV.num_g; j++) {
                CSP_DBG_PRINT("    %d\n", CSP_ALL_G_RANKS_IN_WORLD[i * CSP_ENV.num_g + j]);
            }
        }
#endif
        CSP_DBG_PRINT("I am user, %d/%d in world, %d/%d in local, %d/%d in user world, "
                      "%d/%d in user local, node_id %d\n", rank, nprocs, local_rank,
                      local_nprocs, user_rank, user_nprocs, local_user_rank,
                      local_user_nprocs, CSP_MY_NODE_ID);

        CSP_Init_win_cache();
    }
    /* Ghost processes */
    /* TODO: Ghost process should not run user program */
    else {
        /* free local buffers before enter ghost main function */
        if (tmp_gather_buf)
            free(tmp_gather_buf);
        if (ranks_in_user_world)
            free(ranks_in_user_world);
        if (ranks_in_world)
            free(ranks_in_world);

        CSP_DBG_PRINT("I am ghost, %d/%d in world, %d/%d in local, node_id %d\n", rank,
                      nprocs, local_rank, local_nprocs, CSP_MY_NODE_ID);
        run_g_main();
        exit(0);
    }

  fn_exit:
    if (tmp_gather_buf)
        free(tmp_gather_buf);
    if (ranks_in_user_world)
        free(ranks_in_user_world);
    if (ranks_in_world)
        free(ranks_in_world);

    return mpi_errno;

  fn_fail:
    /* --BEGIN ERROR HANDLING-- */
    if (CSP_COMM_USER_WORLD != MPI_COMM_NULL) {
        CSP_DBG_PRINT("free CSP_COMM_USER_WORLD\n");
        PMPI_Comm_free(&CSP_COMM_USER_WORLD);
    }
    if (CSP_COMM_LOCAL != MPI_COMM_NULL) {
        CSP_DBG_PRINT("free CSP_COMM_LOCAL\n");
        PMPI_Comm_free(&CSP_COMM_LOCAL);
    }
    if (CSP_COMM_USER_LOCAL != MPI_COMM_NULL) {
        CSP_DBG_PRINT("free CSP_COMM_USER_LOCAL\n");
        PMPI_Comm_free(&CSP_COMM_USER_LOCAL);
    }
    if (CSP_COMM_UR_WORLD != MPI_COMM_NULL) {
        CSP_DBG_PRINT("free CSP_COMM_UR_WORLD\n");
        PMPI_Comm_free(&CSP_COMM_UR_WORLD);
    }
    if (CSP_COMM_GHOST_LOCAL != MPI_COMM_NULL) {
        CSP_DBG_PRINT("free CSP_COMM_GHOST_LOCAL\n");
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
    if (CSP_USER_RANKS_IN_WORLD)
        free(CSP_USER_RANKS_IN_WORLD);

    CSP_Destroy_win_cache();

    /* Reset global variables */
    CSP_COMM_USER_WORLD = MPI_COMM_NULL;
    CSP_COMM_USER_LOCAL = MPI_COMM_NULL;
    CSP_COMM_LOCAL = MPI_COMM_NULL;

    CSP_ALL_G_RANKS_IN_WORLD = NULL;
    CSP_ALL_NODE_IDS = NULL;

    PMPI_Abort(MPI_COMM_WORLD, 0);

    goto fn_exit;
    /* --END ERROR HANDLING-- */
}

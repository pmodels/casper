#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"
#include "mtcore_helper.h"

MPI_Comm MTCORE_COMM_USER_WORLD = MPI_COMM_NULL;
MPI_Comm MTCORE_COMM_LOCAL = MPI_COMM_NULL;
MPI_Comm MTCORE_COMM_USER_LOCAL = MPI_COMM_NULL;
MPI_Comm MTCORE_COMM_UR_WORLD = MPI_COMM_NULL;
MPI_Comm MTCORE_COMM_HELPER_LOCAL = MPI_COMM_NULL;
MPI_Group MTCORE_GROUP_WORLD = MPI_GROUP_NULL;
MPI_Group MTCORE_GROUP_LOCAL = MPI_GROUP_NULL;
MPI_Group MTCORE_GROUP_USER_WORLD = MPI_GROUP_NULL;

/*TODO: Now user can set value directly, should be implemented as "-np N -nh X"*/
int MTCORE_NUM_H = 1;
int *MTCORE_H_RANKS_IN_WORLD = NULL;
int *MTCORE_H_RANKS_IN_LOCAL = NULL;
int *MTCORE_ALL_H_RANKS_IN_WORLD = NULL;        /* Helpers of user process x are stored as
                                                 * [x*num_h : (x+1)*num_h-1] */
int *MTCORE_USER_RANKS_IN_WORLD = NULL;

int MTCORE_MY_NODE_ID = -1;
int MTCORE_NUM_NODES = 0;
int *MTCORE_ALL_NODE_IDS = NULL;
int MTCORE_MY_RANK_IN_WORLD = -1;

MTCORE_Define_win_cache;

/* TODO: Move load balancing option into env setting */
MTCORE_Env_param MTCORE_ENV;

static int MTCORE_Initialize_env()
{
    char *val;
    int mpi_errno = MPI_SUCCESS;

    memset(&MTCORE_ENV, 0, sizeof(MTCORE_ENV));
    MTCORE_ENV.seg_size = MTCORE_DEFAULT_SEG_SIZE;

    val = getenv("MTCORE_SEG_SIZE");
    if (val && strlen(val)) {
        MTCORE_ENV.seg_size = atoi(val);
    }
    if (MTCORE_ENV.seg_size <= 0) {
        fprintf(stderr, "Wrong MTCORE_SEG_SIZE %d\n", MTCORE_ENV.seg_size);
        return -1;
    }

    MTCORE_ENV.lock_binding = MTCORE_LOCK_BINDING_RANK;
    val = getenv("MTCORE_LOCK_METHOD");
    if (val && strlen(val)) {
        if (!strncmp(val, "rank", strlen("rank"))) {
            MTCORE_ENV.lock_binding = MTCORE_LOCK_BINDING_RANK;
        }
        else if (!strncmp(val, "segment", strlen("segment"))) {
            MTCORE_ENV.lock_binding = MTCORE_LOCK_BINDING_SEGMENT;
        }
        else {
            fprintf(stderr, "Unknown MTCORE_LOCK_METHOD %s\n", val);
            return -1;
        }
    }

#if defined(MTCORE_ENABLE_RUNTIME_LOAD_OPT)
    MTCORE_ENV.load_opt = MTCORE_LOAD_OPT_RANDOM;

    val = getenv("MTCORE_RUMTIME_LOAD_OPT");
    if (val && strlen(val)) {
        if (!strncmp(val, "random", strlen("random"))) {
            MTCORE_ENV.load_opt = MTCORE_LOAD_OPT_RANDOM;
        }
        else if (!strncmp(val, "op", strlen("op"))) {
            MTCORE_ENV.load_opt = MTCORE_LOAD_OPT_COUNTING;
        }
        else if (!strncmp(val, "byte", strlen("byte"))) {
            MTCORE_ENV.load_opt = MTCORE_LOAD_BYTE_COUNTING;
        }
        else {
            fprintf(stderr, "Unknown MTCORE_RUMTIME_LOAD_OPT %s\n", val);
            return -1;
        }
    }
#else
    MTCORE_ENV.load_opt = MTCORE_LOAD_OPT_STATIC;
#endif

    MTCORE_DBG_PRINT("ENV: seg_size=%d, load_opt=%d\n", MTCORE_ENV.seg_size, MTCORE_ENV.load_opt);

    return mpi_errno;
}

int MPI_Init(int *argc, char ***argv)
{
    static const char FCNAME[] = "MPI_Init";
    int mpi_errno = MPI_SUCCESS;
    int i, j;
    int local_rank, local_nprocs, rank, nprocs, user_rank, user_nprocs;
    int local_user_rank = -1, local_user_nprocs = -1;
    int *tmp_gather_buf = NULL, node_id = 0;
    int tmp_bcast_buf[2];
    int *ranks_in_user_world = NULL, *ranks_in_world = NULL;

    MTCORE_DBG_PRINT_FCNAME();

    mpi_errno = PMPI_Init(argc, argv);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    PMPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MTCORE_MY_RANK_IN_WORLD = rank;

    if ((*argc) > 1) {
        MTCORE_NUM_H = atoi((*argv)[1]);
    }
    MTCORE_DBG_PRINT("MTCORE_NUM_H=%d\n", MTCORE_NUM_H);

    mpi_errno = MTCORE_Initialize_env();
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Get a communicator only containing processes with shared memory */
    mpi_errno = PMPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0,
                                     MPI_INFO_NULL, &MTCORE_COMM_LOCAL);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Check number of helpers and number of processes */
    PMPI_Comm_rank(MTCORE_COMM_LOCAL, &local_rank);
    PMPI_Comm_size(MTCORE_COMM_LOCAL, &local_nprocs);

    if (local_nprocs < 2) {
        fprintf(stderr, "No user process found, please run with more than 2 process per node\n");
        mpi_errno = -1;
        goto fn_fail;
    }
    if (MTCORE_NUM_H < 1 || MTCORE_NUM_H >= local_nprocs) {
        fprintf(stderr, "Wrong value of number of helpers, %d. lt 1 or ge %d.\n",
                MTCORE_NUM_H, local_nprocs);
        mpi_errno = -1;
        goto fn_fail;
    }

    /* Specify the first N local processes to be Helper processes */
    MTCORE_H_RANKS_IN_LOCAL = calloc(MTCORE_NUM_H, sizeof(int));
    MTCORE_H_RANKS_IN_WORLD = calloc(MTCORE_NUM_H, sizeof(int));
    for (i = 0; i < MTCORE_NUM_H; i++) {
        MTCORE_H_RANKS_IN_LOCAL[i] = i;
    }
    mpi_errno = PMPI_Comm_group(MPI_COMM_WORLD, &MTCORE_GROUP_WORLD);
    mpi_errno = PMPI_Comm_group(MTCORE_COMM_LOCAL, &MTCORE_GROUP_LOCAL);

    mpi_errno = PMPI_Group_translate_ranks(MTCORE_GROUP_LOCAL, MTCORE_NUM_H,
                                           MTCORE_H_RANKS_IN_LOCAL, MTCORE_GROUP_WORLD,
                                           MTCORE_H_RANKS_IN_WORLD);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Create a user comm_world including all the users,
     * user will access it instead of comm_world */
    mpi_errno = PMPI_Comm_split(MPI_COMM_WORLD,
                                local_rank < MTCORE_NUM_H, 0, &MTCORE_COMM_USER_WORLD);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    PMPI_Comm_size(MTCORE_COMM_USER_WORLD, &user_nprocs);
    PMPI_Comm_rank(MTCORE_COMM_USER_WORLD, &user_rank);
    PMPI_Comm_group(MTCORE_COMM_USER_WORLD, &MTCORE_GROUP_USER_WORLD);

    /* Create a user comm_local */
    mpi_errno = PMPI_Comm_split(MTCORE_COMM_LOCAL,
                                local_rank < MTCORE_NUM_H, 0, &MTCORE_COMM_USER_LOCAL);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Create a helper comm_local */
    mpi_errno = PMPI_Comm_split(MTCORE_COMM_LOCAL,
                                local_rank < MTCORE_NUM_H, 1, &MTCORE_COMM_HELPER_LOCAL);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Exchange node id among local processes */
    /* -Only users create a user root communicator for exchanging local informations
     * between different nodes*/
    if (local_rank >= MTCORE_NUM_H) {
        PMPI_Comm_rank(MTCORE_COMM_USER_LOCAL, &local_user_rank);
        PMPI_Comm_size(MTCORE_COMM_USER_LOCAL, &local_user_nprocs);
        mpi_errno = PMPI_Comm_split(MTCORE_COMM_USER_WORLD,
                                    local_user_rank == 0, 1, &MTCORE_COMM_UR_WORLD);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* -Only user roots determine a node id for each USER processes */
        if (local_user_rank == 0) {
            PMPI_Comm_size(MTCORE_COMM_UR_WORLD, &MTCORE_NUM_NODES);
            PMPI_Comm_rank(MTCORE_COMM_UR_WORLD, &MTCORE_MY_NODE_ID);

            tmp_bcast_buf[0] = MTCORE_MY_NODE_ID;
            tmp_bcast_buf[1] = MTCORE_NUM_NODES;
        }
    }
    /* -User root broadcasts to other local processes */
    PMPI_Bcast(tmp_bcast_buf, 2, MPI_INT, MTCORE_NUM_H, MTCORE_COMM_LOCAL);
    MTCORE_MY_NODE_ID = tmp_bcast_buf[0];
    MTCORE_NUM_NODES = tmp_bcast_buf[1];

    /* Exchange node id and Helper ranks among world processes */
    ranks_in_world = calloc(nprocs, sizeof(int));
    ranks_in_user_world = calloc(nprocs, sizeof(int));
    for (i = 0; i < nprocs; i++) {
        ranks_in_world[i] = i;
    }
    mpi_errno = PMPI_Group_translate_ranks(MTCORE_GROUP_WORLD, nprocs,
                                           ranks_in_world, MTCORE_GROUP_USER_WORLD,
                                           ranks_in_user_world);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    MTCORE_ALL_NODE_IDS = calloc(nprocs, sizeof(int));
    MTCORE_ALL_H_RANKS_IN_WORLD = calloc(user_nprocs * MTCORE_NUM_H, sizeof(int));
    tmp_gather_buf = calloc(nprocs * (1 + MTCORE_NUM_H), sizeof(int));

    tmp_gather_buf[rank * (1 + MTCORE_NUM_H)] = MTCORE_MY_NODE_ID;
    for (i = 0; i < MTCORE_NUM_H; i++) {
        tmp_gather_buf[rank * (1 + MTCORE_NUM_H) + i + 1] = MTCORE_H_RANKS_IN_WORLD[i];
    }
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                               tmp_gather_buf, 1 + MTCORE_NUM_H, MPI_INT, MPI_COMM_WORLD);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    for (i = 0; i < nprocs; i++) {
        int i_user_rank = 0;
        node_id = tmp_gather_buf[i * (1 + MTCORE_NUM_H)];
        MTCORE_ALL_NODE_IDS[i] = node_id;

        /* Only copy helper ranks for user processes */
        i_user_rank = ranks_in_user_world[i];
        if (i_user_rank != MPI_UNDEFINED) {
            for (j = 0; j < MTCORE_NUM_H; j++) {
                MTCORE_ALL_H_RANKS_IN_WORLD[i_user_rank * MTCORE_NUM_H + j] =
                    tmp_gather_buf[i * (1 + MTCORE_NUM_H) + j + 1];
            }
        }
    }

#ifdef DEBUG
    MTCORE_DBG_PRINT("Debug gathered info ***** \n");
    for (i = 0; i < nprocs; i++) {
        MTCORE_DBG_PRINT("node_id[%d]: %d\n", i, MTCORE_ALL_NODE_IDS[i]);
    }
#endif

    /* USER processes */
    if (local_rank >= MTCORE_NUM_H) {
        /* Get user ranks in world */
        for (i = 0; i < user_nprocs; i++)
            ranks_in_user_world[i] = i;
        MTCORE_USER_RANKS_IN_WORLD = calloc(user_nprocs, sizeof(int));
        mpi_errno = PMPI_Group_translate_ranks(MTCORE_GROUP_USER_WORLD, user_nprocs,
                                               ranks_in_user_world, MTCORE_GROUP_WORLD,
                                               MTCORE_USER_RANKS_IN_WORLD);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

#ifdef DEBUG
        for (i = 0; i < user_nprocs; i++) {
            MTCORE_DBG_PRINT("helper_rank_in_world[%d]:\n", i);
            for (j = 0; j < MTCORE_NUM_H; j++) {
                MTCORE_DBG_PRINT("    %d\n", MTCORE_ALL_H_RANKS_IN_WORLD[i * MTCORE_NUM_H + j]);
            }
        }
#endif
        MTCORE_DBG_PRINT("I am user, %d/%d in world, %d/%d in local, %d/%d in user world, "
                         "%d/%d in user local, node_id %d\n", rank, nprocs, local_rank,
                         local_nprocs, user_rank, user_nprocs, local_user_rank,
                         local_user_nprocs, MTCORE_MY_NODE_ID);

        MTCORE_Init_win_cache();
    }
    /* Helper processes */
    /* TODO: Helper process should not run user program */
    else {
        /* free local buffers before enter helper main function */
        if (tmp_gather_buf)
            free(tmp_gather_buf);
        if (ranks_in_user_world)
            free(ranks_in_user_world);
        if (ranks_in_world)
            free(ranks_in_world);

        MTCORE_DBG_PRINT("I am helper, %d/%d in world, %d/%d in local, node_id %d\n", rank,
                         nprocs, local_rank, local_nprocs, MTCORE_MY_NODE_ID);
        run_h_main();
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
    if (MTCORE_COMM_USER_WORLD != MPI_COMM_NULL) {
        MTCORE_DBG_PRINT("free MTCORE_COMM_USER_WORLD\n");
        PMPI_Comm_free(&MTCORE_COMM_USER_WORLD);
    }
    if (MTCORE_COMM_LOCAL != MPI_COMM_NULL) {
        MTCORE_DBG_PRINT("free MTCORE_COMM_LOCAL\n");
        PMPI_Comm_free(&MTCORE_COMM_LOCAL);
    }
    if (MTCORE_COMM_USER_LOCAL != MPI_COMM_NULL) {
        MTCORE_DBG_PRINT("free MTCORE_COMM_USER_LOCAL\n");
        PMPI_Comm_free(&MTCORE_COMM_USER_LOCAL);
    }
    if (MTCORE_COMM_UR_WORLD != MPI_COMM_NULL) {
        MTCORE_DBG_PRINT("free MTCORE_COMM_UR_WORLD\n");
        PMPI_Comm_free(&MTCORE_COMM_UR_WORLD);
    }
    if (MTCORE_COMM_HELPER_LOCAL != MPI_COMM_NULL) {
        MTCORE_DBG_PRINT("free MTCORE_COMM_HELPER_LOCAL\n");
        PMPI_Comm_free(&MTCORE_COMM_HELPER_LOCAL);
    }

    if (MTCORE_GROUP_WORLD != MPI_GROUP_NULL)
        PMPI_Group_free(&MTCORE_GROUP_WORLD);
    if (MTCORE_GROUP_LOCAL != MPI_GROUP_NULL)
        PMPI_Group_free(&MTCORE_GROUP_LOCAL);
    if (MTCORE_GROUP_USER_WORLD != MPI_GROUP_NULL)
        PMPI_Group_free(&MTCORE_GROUP_USER_WORLD);

    if (MTCORE_H_RANKS_IN_WORLD)
        free(MTCORE_H_RANKS_IN_WORLD);
    if (MTCORE_H_RANKS_IN_LOCAL)
        free(MTCORE_H_RANKS_IN_LOCAL);
    if (MTCORE_ALL_H_RANKS_IN_WORLD)
        free(MTCORE_ALL_H_RANKS_IN_WORLD);
    if (MTCORE_ALL_NODE_IDS)
        free(MTCORE_ALL_NODE_IDS);
    if (MTCORE_USER_RANKS_IN_WORLD)
        free(MTCORE_USER_RANKS_IN_WORLD);

    MTCORE_Destroy_win_cache();

    /* Reset global variables */
    MTCORE_COMM_USER_WORLD = MPI_COMM_NULL;
    MTCORE_COMM_USER_LOCAL = MPI_COMM_NULL;
    MTCORE_COMM_LOCAL = MPI_COMM_NULL;

    MTCORE_ALL_H_RANKS_IN_WORLD = NULL;
    MTCORE_ALL_NODE_IDS = NULL;

    PMPI_Abort(MPI_COMM_WORLD, 0);

    goto fn_exit;
    /* --END ERROR HANDLING-- */
}

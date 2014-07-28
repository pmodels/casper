#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"
#include "mtcore_helper.h"

MPI_Comm MTCORE_COMM_USER_WORLD = MPI_COMM_NULL;
MPI_Comm MTCORE_COMM_LOCAL = MPI_COMM_NULL;
MPI_Comm MTCORE_COMM_USER_LOCAL = MPI_COMM_NULL;
MPI_Comm MTCORE_COMM_USER_ROOTS = MPI_COMM_NULL;
MPI_Group MTCORE_GROUP_WORLD = MPI_GROUP_NULL;
MPI_Group MTCORE_GROUP_LOCAL = MPI_GROUP_NULL;

int MTCORE_NUM_H_IN_LOCAL = 0;
int MTCORE_RANK_IN_COMM_WORLD = -1;
int MTCORE_RANK_IN_COMM_LOCAL = -1;
int *MTCORE_ALL_H_IN_COMM_WORLD = NULL;
int MTCORE_MY_NODE_ID = -1;
int MTCORE_NUM_NODES = 0;
int *MTCORE_ALL_NODE_IDS = NULL;
int MTCORE_MY_RANK_IN_WORLD = -1;

hashtable_t *uh_win_ht;

int MPI_Init(int *argc, char ***argv)
{
    static const char FCNAME[] = "MPI_Init";
    int mpi_errno = MPI_SUCCESS;
    MTCORE_GROUP_LOCAL = 0;
    int i;
    int local_rank, local_nprocs, rank, nprocs, user_rank, user_nprocs;
    int local_user_rank, local_user_nprocs;
    int *tmp_node_helper_gather_buf = NULL, node_id = 0;
    int tmp_local_node_bcast_buf[2];

    MTCORE_DBG_PRINT_FCNAME();

    MTCORE_NUM_H_IN_LOCAL = 1;

    mpi_errno = PMPI_Init(argc, argv);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Get a communicator only containing processes with shared memory */
    mpi_errno = PMPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0,
                                     MPI_INFO_NULL, &MTCORE_COMM_LOCAL);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /*
     * Specify the first N local processes to be Helper processes
     */
    mpi_errno = PMPI_Comm_group(MPI_COMM_WORLD, &MTCORE_GROUP_WORLD);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    mpi_errno = PMPI_Comm_group(MTCORE_COMM_LOCAL, &MTCORE_GROUP_LOCAL);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    MTCORE_RANK_IN_COMM_LOCAL = 0;
    PMPI_Group_translate_ranks(MTCORE_GROUP_LOCAL, MTCORE_NUM_H_IN_LOCAL,
                               &MTCORE_RANK_IN_COMM_LOCAL, MTCORE_GROUP_WORLD,
                               &MTCORE_RANK_IN_COMM_WORLD);

    /* Get a user comm_world for the user to use */
    PMPI_Comm_rank(MTCORE_COMM_LOCAL, &local_rank);
    PMPI_Comm_size(MTCORE_COMM_LOCAL, &local_nprocs);

    if (local_nprocs == 1) {
        fprintf(stderr, "No user process found, please run with more than 2 process per node\n");
        mpi_errno = -1;
        goto fn_fail;
    }

    mpi_errno = PMPI_Comm_split(MPI_COMM_WORLD,
                                local_rank < MTCORE_NUM_H_IN_LOCAL, 0, &MTCORE_COMM_USER_WORLD);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Get a user comm_local for the user to use */
    mpi_errno = PMPI_Comm_split(MTCORE_COMM_LOCAL,
                                local_rank < MTCORE_NUM_H_IN_LOCAL, 0, &MTCORE_COMM_USER_LOCAL);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Get a user root communicator for exchange local informations between different nodes */
    PMPI_Comm_rank(MTCORE_COMM_USER_LOCAL, &local_user_rank);
    PMPI_Comm_size(MTCORE_COMM_USER_LOCAL, &local_user_nprocs);
    mpi_errno = PMPI_Comm_split(MTCORE_COMM_USER_WORLD,
                                local_user_rank == 0, 1, &MTCORE_COMM_USER_ROOTS);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Determine a node id for each USER processes */
    PMPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
    PMPI_Comm_size(MTCORE_COMM_USER_ROOTS, &MTCORE_NUM_NODES);
    PMPI_Comm_rank(MTCORE_COMM_USER_ROOTS, &MTCORE_MY_NODE_ID);
    PMPI_Comm_size(MTCORE_COMM_USER_WORLD, &user_nprocs);
    PMPI_Comm_rank(MTCORE_COMM_USER_WORLD, &user_rank);
    MTCORE_MY_RANK_IN_WORLD = rank;

    /* Exchange node id among local processes */
    tmp_local_node_bcast_buf[0] = MTCORE_MY_NODE_ID;
    tmp_local_node_bcast_buf[1] = MTCORE_NUM_NODES;
    PMPI_Bcast(tmp_local_node_bcast_buf, 2, MPI_INT, 0, MTCORE_COMM_LOCAL);
    MTCORE_MY_NODE_ID = tmp_local_node_bcast_buf[0];
    MTCORE_NUM_NODES = tmp_local_node_bcast_buf[1];

    MTCORE_ALL_NODE_IDS = calloc(nprocs, sizeof(int));
    MTCORE_ALL_H_IN_COMM_WORLD = calloc(MTCORE_NUM_NODES * MTCORE_NUM_H_IN_LOCAL, sizeof(int));
    tmp_node_helper_gather_buf = calloc(nprocs, sizeof(int) * 2);

    /* Exchange node id and Helper ranks among world processes */
    tmp_node_helper_gather_buf[rank * 2] = MTCORE_MY_NODE_ID;
    /* TODO: support multiple helpers */
    tmp_node_helper_gather_buf[rank * 2 + 1] = MTCORE_RANK_IN_COMM_WORLD;
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                               tmp_node_helper_gather_buf, 2, MPI_INT, MPI_COMM_WORLD);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    for (i = 0; i < nprocs; i++) {
        node_id = tmp_node_helper_gather_buf[i * 2];
        MTCORE_ALL_NODE_IDS[i] = node_id;
        MTCORE_ALL_H_IN_COMM_WORLD[node_id] = tmp_node_helper_gather_buf[i * 2 + 1];
    }

#ifdef DEBUG
    MTCORE_DBG_PRINT("Debug gathered info ***** \n");
    for (i = 0; i < nprocs; i++) {
        node_id = MTCORE_ALL_NODE_IDS[i];
        MTCORE_DBG_PRINT("[%d] node_id[%d]: %d, helper_rank_in_world[%d]: %d\n", rank, i,
                         MTCORE_ALL_NODE_IDS[i], node_id, MTCORE_ALL_H_IN_COMM_WORLD[node_id]);
    }
    PMPI_Barrier(MPI_COMM_WORLD);
#endif

    /* USER processes */
    if (local_rank >= MTCORE_NUM_H_IN_LOCAL) {
        MTCORE_DBG_PRINT("I am user, %d/%d in world, %d/%d in local, %d/%d in user world, "
                         "%d/%d in user local, h_rank_in_world %d, node_id %d\n",
                         rank, nprocs, local_rank, local_nprocs, user_rank, user_nprocs,
                         local_user_rank, local_user_nprocs, MTCORE_RANK_IN_COMM_WORLD,
                         MTCORE_ALL_NODE_IDS[rank]);

        mpi_errno = init_uh_win_table();
        if (mpi_errno != 0)
            goto fn_fail;
    }
    /* Helper processes */
    /* TODO: Helper process should not run user program */
    else {
        MTCORE_DBG_PRINT("I am helper, %d/%d in world, %d/%d in local, node_id %d\n",
                         rank, nprocs, local_rank, local_nprocs, MTCORE_RANK_IN_COMM_WORLD,
                         MTCORE_ALL_NODE_IDS[rank]);

        run_h_main();
        exit(0);
    }

  fn_exit:
    if (tmp_node_helper_gather_buf)
        free(tmp_node_helper_gather_buf);

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

    if (MTCORE_GROUP_WORLD != MPI_GROUP_NULL)
        PMPI_Group_free(&MTCORE_GROUP_WORLD);
    if (MTCORE_GROUP_LOCAL != MPI_GROUP_NULL)
        PMPI_Group_free(&MTCORE_GROUP_LOCAL);

    if (MTCORE_ALL_NODE_IDS)
        free(MTCORE_ALL_NODE_IDS);
    if (MTCORE_ALL_H_IN_COMM_WORLD)
        free(MTCORE_ALL_H_IN_COMM_WORLD);

    /* Reset global variables */
    MTCORE_COMM_USER_WORLD = MPI_COMM_NULL;
    MTCORE_COMM_USER_LOCAL = MPI_COMM_NULL;
    MTCORE_COMM_LOCAL = MPI_COMM_NULL;

    MTCORE_ALL_H_IN_COMM_WORLD = NULL;
    MTCORE_ALL_NODE_IDS = NULL;

    PMPI_Abort(MPI_COMM_WORLD, 0);

    goto fn_exit;
    /* --END ERROR HANDLING-- */
}

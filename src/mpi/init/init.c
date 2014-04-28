#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"
#include "asp.h"

MPI_Comm MPIASP_COMM_USER_WORLD = MPI_COMM_NULL;
MPI_Comm MPIASP_COMM_USER_LOCAL = MPI_COMM_NULL;
MPI_Comm MPIASP_COMM_LOCAL = MPI_COMM_NULL;
MPI_Comm MPIASP_COMM_USER_ROOTS = MPI_COMM_NULL;

//MPI_Comm MPIASP_COMM_ASP_LOCAL = MPI_COMM_NULL;
//MPI_Comm MPIASP_COMM_ASP_WORLD = MPI_COMM_NULL;

int MPIASP_NUM_ASP_IN_LOCAL = 0;
int MPIASP_RANK_IN_COMM_WORLD = -1;
int MPIASP_RANK_IN_COMM_LOCAL = -1;
int *MPIASP_ALL_ASP_IN_COMM_WORLD = NULL;
int MPIASP_NUM_UNIQUE_ASP = 0;
int MPIASP_MY_NODE_ID = -1;
int *MPIASP_ALL_NODE_IDS = NULL;

int MPI_Init(int *argc, char ***argv) {
    static const char FCNAME[] = "MPI_Init";
    int mpi_errno = MPI_SUCCESS;
    MPI_Group world_group = 0, local_group = 0;
    int i;
    int local_rank, local_nprocs, rank, nprocs, user_rank, user_nprocs;
    int local_user_rank, local_user_nprocs;

    MPIASP_DBG_PRINT_FCNAME();

    MPIASP_NUM_ASP_IN_LOCAL = 1;

    mpi_errno = PMPI_Init(argc, argv);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    mpi_errno = table_init();
    if (mpi_errno != 0)
        goto fn_fail;

    /* Get a communicator only containing processes with shared memory */
    mpi_errno = PMPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0,
            MPI_INFO_NULL, &MPIASP_COMM_LOCAL);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /*
     * Specify the first N local processes to be ASP process
     */
    mpi_errno = PMPI_Comm_group(MPI_COMM_WORLD, &world_group);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    mpi_errno = PMPI_Comm_group(MPIASP_COMM_LOCAL, &local_group);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    MPIASP_RANK_IN_COMM_LOCAL = 0;
    PMPI_Group_translate_ranks(local_group, MPIASP_NUM_ASP_IN_LOCAL,
            &MPIASP_RANK_IN_COMM_LOCAL, world_group, &MPIASP_RANK_IN_COMM_WORLD);

    /* Get a user comm_world for the user to use */
    PMPI_Comm_rank(MPIASP_COMM_LOCAL, &local_rank);
    PMPI_Comm_size(MPIASP_COMM_LOCAL, &local_nprocs);
    mpi_errno = PMPI_Comm_split(MPI_COMM_WORLD,
            local_rank < MPIASP_NUM_ASP_IN_LOCAL, 0, &MPIASP_COMM_USER_WORLD);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Get a user comm_local for the user to use */
    mpi_errno = PMPI_Comm_split(MPIASP_COMM_LOCAL,
            local_rank < MPIASP_NUM_ASP_IN_LOCAL, 0, &MPIASP_COMM_USER_LOCAL);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Get a user root communicator for exchange local informations between different nodes*/
    PMPI_Comm_rank(MPIASP_COMM_USER_LOCAL, &local_user_rank);
    mpi_errno = PMPI_Comm_split(MPIASP_COMM_USER_WORLD,
            local_user_rank == 0, 1, &MPIASP_COMM_USER_ROOTS);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Determine a node id for each USER processes */
    PMPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
    PMPI_Comm_rank(MPIASP_COMM_USER_ROOTS, &MPIASP_MY_NODE_ID);
    PMPI_Comm_size(MPIASP_COMM_USER_WORLD, &user_nprocs);
    PMPI_Comm_rank(MPIASP_COMM_USER_WORLD, &user_rank);

    MPIASP_ALL_NODE_IDS = calloc(nprocs, sizeof(int));

    PMPI_Bcast(&MPIASP_MY_NODE_ID, 1, MPI_INT, 0, MPIASP_COMM_LOCAL);
    MPIASP_ALL_NODE_IDS[rank] = MPIASP_MY_NODE_ID;
    mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
            MPIASP_ALL_NODE_IDS, 1, MPI_INT, MPI_COMM_WORLD);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    // USER processes
    MPIASP_DBG_PRINT("%d/%d in world, %d/%d in local, asp_rank_in_ua %d, "
            "node_id %d\n", rank, nprocs, local_rank, local_nprocs,
            MPIASP_RANK_IN_COMM_WORLD, MPIASP_ALL_NODE_IDS[rank]);

    if (local_rank >= MPIASP_NUM_ASP_IN_LOCAL) {

        /* -Gather the rank_in_world of ASP processes from all user process */
        MPIASP_ALL_ASP_IN_COMM_WORLD = calloc(user_nprocs, sizeof(int));
        MPIASP_ALL_ASP_IN_COMM_WORLD[user_rank] = MPIASP_RANK_IN_COMM_WORLD;
        mpi_errno = PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                MPIASP_ALL_ASP_IN_COMM_WORLD, 1, MPI_INT,
                MPIASP_COMM_USER_WORLD);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

#ifdef DEBUG
        PMPI_Comm_size(MPIASP_COMM_USER_LOCAL, &local_user_nprocs);
        MPIASP_DBG_PRINT("create MPIASP_COMM_USER_WORLD,"
                "I am %d/%d in world, %d/%d in local, %d/%d in user world, "
                "%d/%d in user local\n", rank, nprocs, local_rank, local_nprocs,
                user_rank, user_nprocs, local_user_rank, local_user_nprocs);

        if (user_rank == 0) {
            MPIASP_DBG_PRINT("Debug gathered info ***** \n");
            for (i = 0; i < user_nprocs; i++) {
                MPIASP_DBG_PRINT("[%d] asp_rank_in_ua[%d]: %d\n",
                        rank, i, MPIASP_ALL_ASP_IN_COMM_WORLD[i]);
            }

            for (i = 0; i < nprocs; i++) {
                MPIASP_DBG_PRINT("[%d] node_id[%d]: %d\n", rank, i,
                        MPIASP_ALL_NODE_IDS[i]);
            }
        }
        PMPI_Barrier(MPI_COMM_WORLD);
#endif
    }
    //ASP processes
    /* TODO: ASP process should not run user program */
    else {
#ifdef DEBUG
        ASP_DBG_PRINT("I am ASP on node %d, %d/%d in world, %d/%d in local\n",
                MPIASP_ALL_NODE_IDS[rank], rank, nprocs, local_rank,
                local_nprocs);
        ASP_DBG_PRINT("Debug gathered info ***** \n");
        for (i = 0; i < nprocs; i++) {
            ASP_DBG_PRINT(" node_id[%d]: %d\n", i, MPIASP_ALL_NODE_IDS[i]);
        }
        PMPI_Barrier(MPI_COMM_WORLD);
#endif

        // Finish cleaning work before start ASP processing.
        if (world_group)
            PMPI_Group_free(&world_group);
        if (local_group)
            PMPI_Group_free(&local_group);

        run_asp_main();
        exit(0);
    }

    fn_exit:
    if (world_group)
        PMPI_Group_free(&world_group);
    if (local_group)
        PMPI_Group_free(&local_group);

    return mpi_errno;

    fn_fail:
    /* --BEGIN ERROR HANDLING-- */
    if (MPIASP_COMM_USER_WORLD)
        PMPI_Comm_free(&MPIASP_COMM_USER_WORLD);
    if (MPIASP_COMM_LOCAL)
        PMPI_Comm_free(&MPIASP_COMM_LOCAL);
    if (MPIASP_COMM_USER_LOCAL)
        PMPI_Comm_free(&MPIASP_COMM_USER_LOCAL);

    if (MPIASP_ALL_NODE_IDS)
        free(MPIASP_ALL_NODE_IDS);
    if (MPIASP_ALL_ASP_IN_COMM_WORLD)
        free(MPIASP_ALL_ASP_IN_COMM_WORLD);

    // Reset global variables
    MPIASP_COMM_USER_WORLD = MPI_COMM_NULL;
    MPIASP_COMM_USER_LOCAL = MPI_COMM_NULL;
    MPIASP_COMM_LOCAL = MPI_COMM_NULL;

    MPIASP_ALL_ASP_IN_COMM_WORLD = NULL;
    MPIASP_ALL_NODE_IDS = NULL;

    goto fn_exit;
    /* --END ERROR HANDLING-- */
}

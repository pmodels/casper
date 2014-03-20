#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

MPI_Comm MPIASP_COMM_USER_WORLD = MPI_COMM_NULL;
MPI_Comm MPIASP_COMM_LOCAL = MPI_COMM_NULL;
int MPIASP_RANK_IN_COMM_WORLD = -1;

int MPI_Init(int *argc, char ***argv) {
    static const char FCNAME[] = "MPI_Init";
    int mpi_errno = MPI_SUCCESS;
    MPI_Group world_group = 0, local_group = 0;
    int asp_ranks[1];
    int local_rank, rank, nprocs;

    MPIASP_DBG_PRINT_FCNAME();

    mpi_errno = PMPI_Init(argc, argv);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
    PMPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    /* Get a communicator only containing processes with shared memory */
    mpi_errno = PMPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0,
            MPI_INFO_NULL, &MPIASP_COMM_LOCAL);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    /* Specify the first local process to be an ASP process */
    mpi_errno = PMPI_Comm_group(MPI_COMM_WORLD, &world_group);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    mpi_errno = PMPI_Comm_group(MPIASP_COMM_LOCAL, &local_group);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    asp_ranks[0] = 0;
    PMPI_Group_translate_ranks(world_group, 1, asp_ranks,
            local_group, &MPIASP_RANK_IN_COMM_WORLD);

    /* Get a user comm_world for the user to use */
    PMPI_Comm_rank(MPIASP_COMM_LOCAL, &local_rank);
    mpi_errno = PMPI_Comm_split(MPI_COMM_WORLD, local_rank == 0, 0,
            &MPIASP_COMM_USER_WORLD);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    MPIASP_DBG_PRINT(
            "[MPIASP] create MPIASP_COMM_USER_WORLD, asp rank %d, I am %d/%d\n",
            MPIASP_RANK_IN_COMM_WORLD, rank, nprocs);

//    PMPI_Barrier(MPI_COMM_WORLD);

    /* TODO: ASP process should not run user program */
    if (MPIASP_IsASP(rank)) {
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
    if (MPIASP_COMM_USER_WORLD) {
        PMPI_Comm_free(&MPIASP_COMM_USER_WORLD);
    }
    if (MPIASP_COMM_LOCAL) {
        PMPI_Comm_free(&MPIASP_COMM_LOCAL);
    }

    // Reset global variables
    MPIASP_COMM_USER_WORLD = 0;
    MPIASP_RANK_IN_COMM_WORLD = -1;

    goto fn_exit;
    /* --END ERROR HANDLING-- */
}

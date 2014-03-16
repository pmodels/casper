#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

/* -- Begin Profiling Symbol Block for routine MPI_Init */
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPI_Init = MPIASP_Init
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF MPIASP_Init  MPI_Init
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPI_Init as MPIASP_Init
#endif
/* -- End Profiling Symbol Block */

MPI_Comm MPIASP_COMM_USER = 0;
int MPIASP_RANK_IN_COMM_WORLD = -1;

int MPIASP_Init(int *argc, char ***argv) {
    static const char FCNAME[] = "MPIASP_Init";
    int mpi_errno = MPI_SUCCESS;
    MPI_Group world_group = 0, user_group = 0;
    int asp_ranks[1];
    int rank, nprocs;

    MPIASP_DBG_PRINT_FCNAME();

    mpi_errno = PMPI_Init(argc, argv);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    mpi_errno = PMPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    mpi_errno = PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    // Specify the last process to be an as-process
    MPIASP_RANK_IN_COMM_WORLD = nprocs - 1;

    mpi_errno = PMPI_Comm_group(MPI_COMM_WORLD, &world_group);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    asp_ranks[0] = MPIASP_RANK_IN_COMM_WORLD;
    mpi_errno = PMPI_Group_excl(world_group, 1, asp_ranks, &user_group);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    mpi_errno = PMPI_Comm_create(MPI_COMM_WORLD, user_group, &MPIASP_COMM_USER);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    MPIASP_DBG_PRINT(
            "[MPIASP] create MPIASP_COMM_USER, asp rank %d, I am %d/%d\n", MPIASP_RANK_IN_COMM_WORLD, rank, nprocs);

    PMPI_Barrier(MPI_COMM_WORLD);

    /* TODO: ASP process should not run user program */
    if (MPIASP_IsASP(rank)) {
        // Finish cleaning work before start ASP processing.
        if (world_group)
            PMPI_Group_free(&world_group);
        if (user_group)
            PMPI_Group_free(&user_group);

        run_asp_main();
        exit(0);
    }

    fn_exit:

    if (world_group)
        PMPI_Group_free(&world_group);
    if (user_group)
        PMPI_Group_free(&user_group);

    return mpi_errno;

    fn_fail:
    /* --BEGIN ERROR HANDLING-- */
    if (MPIASP_COMM_USER) {
        PMPI_Comm_free(&MPIASP_COMM_USER);
    }

    // Reset global variables
    MPIASP_COMM_USER = 0;
    MPIASP_RANK_IN_COMM_WORLD = -1;

    goto fn_exit;
    /* --END ERROR HANDLING-- */
}

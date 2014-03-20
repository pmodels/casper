#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

int MPI_Finalize(void) {
    static const char FCNAME[] = "MPI_Finalize";
    int mpi_errno = MPI_SUCCESS;
    int rank, nprocs , local_rank, local_nprocs;

    MPIASP_DBG_PRINT_FCNAME();

    PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
    PMPI_Comm_rank(MPI_COMM_WORLD, &nprocs);

    PMPI_Comm_rank(MPIASP_COMM_LOCAL, &local_rank);
    PMPI_Comm_size(MPIASP_COMM_LOCAL, &local_nprocs);

    if (local_rank == local_nprocs - 1) {
        // ASP does not need user process information because it is a global call
        MPIASP_Func_start(MPIASP_FUNC_FINALIZE, 0, 0);
    }

    if (MPIASP_COMM_USER_WORLD) {
        PMPI_Comm_free(&MPIASP_COMM_USER_WORLD);
    }
    if (MPIASP_COMM_LOCAL) {
        PMPI_Comm_free(&MPIASP_COMM_LOCAL);
    }
    MPIASP_DBG_PRINT("free MPIASP_COMM_USER_WORLD, I am %d/%d, local %d/%d \n",
            rank, nprocs, local_rank, local_nprocs);

    mpi_errno = PMPI_Finalize();
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    fn_exit:
    return mpi_errno;

    fn_fail:
    goto fn_exit;
}

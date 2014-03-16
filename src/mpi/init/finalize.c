#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

/* -- Begin Profiling Symbol Block for routine MPI_Finalize */
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPI_Finalize = MPIASP_Finalize
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF MPIASP_Finalize  MPI_Finalize
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPI_Finalize as MPIASP_Finalize
#endif
/* -- End Profiling Symbol Block */

#undef FUNCNAME
#define FUNCNAME MPIASP_Finalize

int MPIASP_Finalize(void) {
    static const char FCNAME[] = "MPIASP_Finalize";
    int mpi_errno = MPI_SUCCESS;
    int rank, nprocs;

    MPIASP_DBG_PRINT_FCNAME();

    PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
    PMPI_Comm_rank(MPI_COMM_WORLD, &nprocs);

    if (rank == 0) {
        // ASP does not need user process information because it is a global call
        MPIASP_Func_start(MPIASP_FUNC_FINALIZE, 0, 0);
    }

    // Only the processes in user communicator free MPIASP_COMM_USER
    if (MPIASP_Asp_initialized() && !MPIASP_IsASP(rank)) {
        mpi_errno = PMPI_Comm_free(&MPIASP_COMM_USER);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        MPIASP_DBG_PRINT("[MPIASP] free MPIASP_COMM_USER, I am %d \n", rank);
    }

    mpi_errno = PMPI_Finalize();
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    fn_exit:

    return mpi_errno;

    fn_fail:

    goto fn_exit;
}

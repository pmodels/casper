#include <stdio.h>
#include <stdlib.h>
#include "asp.h"

/* -- Begin Profiling Symbol Block for routine MPI_Win_create */
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPI_Comm_dup = MPIASP_Comm_dup
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF MPIASP_Comm_dup  MPI_Comm_dup
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPI_Comm_dup as MPIASP_Comm_dup
#endif
/* -- End Profiling Symbol Block */

#undef FUNCNAME
#define FUNCNAME MPIASP_Comm_dup

int MPIASP_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm) {
    static const char FCNAME[] = "MPIASP_Comm_dup";
    int mpi_errno = MPI_SUCCESS;
    int compare_result = MPI_UNEQUAL;
    int rank;

    MPIASP_DBG_PRINT_FCNAME();

    PMPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /*
     *  Replace MPI_COMM_WORLD with MPIASP_COMM_USER if current process is a user process.
     *  TODO:
     *  1. Do we need to also replace other communicators which include the asp process ?
     *  2. What should asp process do ?
     */
    PMPI_Comm_compare(comm, MPI_COMM_WORLD, &compare_result);
    if (MPIASP_Asp_initialized() && compare_result == MPI_IDENT) {
        if (!MPIASP_IsASP(rank)) {
            MPIASP_DBG_PRINT("[MPIASP] duplicate MPIASP_COMM_USER, I am %d\n", rank);
            mpi_errno = PMPI_Comm_dup(MPIASP_COMM_USER, newcomm);
        }
    } else {
        mpi_errno = PMPI_Comm_dup(comm, newcomm);
    }

    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    fn_exit:

    return mpi_errno;

    fn_fail:

    goto fn_exit;
}

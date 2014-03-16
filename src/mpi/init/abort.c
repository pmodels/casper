#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

/* -- Begin Profiling Symbol Block for routine MPI_Finalize */
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPI_Abort = MPIASP_Abort
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF MPIASP_Abort  MPI_Abort
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPI_Abort as MPIASP_Abort
#endif
/* -- End Profiling Symbol Block */

#undef FUNCNAME
#define FUNCNAME MPIASP_Abort

int MPIASP_Abort(MPI_Comm comm, int errorcode) {
    static const char FCNAME[] = "MPIASP_Abort";
    int mpi_errno = MPI_SUCCESS;
    int compare_result = MPI_UNEQUAL;

    MPIASP_DBG_PRINT_FCNAME();

    // asp process cannot abort in user app
    if(MPIASP_Comm_rank_isasp()){
        goto fn_exit;
    }

    MPI_Comm_compare(comm, MPIASP_COMM_USER, &compare_result);
    if (MPIASP_Asp_initialized() && compare_result == MPI_IDENT) {
        MPIASP_DBG_PRINT("[MPIASP] abort MPI_COMM_WORLD\n");
        mpi_errno = PMPI_Abort(MPI_COMM_WORLD, errorcode);
    } else {
        mpi_errno = PMPI_Abort(comm, errorcode);
    }

    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    fn_exit:

    return mpi_errno;

    fn_fail:

    goto fn_exit;
}

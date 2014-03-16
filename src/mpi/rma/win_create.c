#include <stdio.h>
#include <stdlib.h>
#include "mpiasp.h"

/* -- Begin Profiling Symbol Block for routine MPI_Win_create */
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPI_Win_create = MPIASP_Win_create
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF MPIASP_Win_create  MPI_Win_create
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPI_Win_create as MPIASP_Win_create
#endif
/* -- End Profiling Symbol Block */

#undef FUNCNAME
#define FUNCNAME MPIASP_Win_create

int MPIASP_Win_create(void *base, MPI_Aint size, int disp_unit, MPI_Info info,
        MPI_Comm comm, MPI_Win *win) {
    static const char FCNAME[] = "MPIASP_Win_create";
    int mpi_errno = MPI_SUCCESS;

    MPIASP_DBG_PRINT_FCNAME();

    if (MPIASP_Comm_rank_isasp())
        goto fn_exit;

    mpi_errno = PMPI_Win_create(base, size, disp_unit, info, comm, win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    fn_exit:

    return mpi_errno;

    fn_fail:

    goto fn_exit;
}

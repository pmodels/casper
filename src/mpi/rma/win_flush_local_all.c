#include <stdio.h>
#include <stdlib.h>
#include "csp.h"

int MPI_Win_flush_local_all(MPI_Win win)
{
    int mpi_errno = MPI_SUCCESS;

    CSP_DBG_PRINT_FCNAME();

    /* Simply translate to manticore flush */
    mpi_errno = MPI_Win_flush_all(win);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

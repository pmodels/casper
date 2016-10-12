/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Win_call_errhandler(MPI_Win win, int errorcode)
{
    int mpi_errno = MPI_SUCCESS;
    int errcode_val = errorcode;        /* to pass valid address */

    CSPU_ERRHAN_RESET_EXTOBJ();
    mpi_errno = CSPU_win_call_errhandler(win, &errcode_val);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

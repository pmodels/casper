/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

CSP_define_win_cache;

int CSP_init(void)
{
    int mpi_errno = MPI_SUCCESS;

    CSP_init_win_cache();

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

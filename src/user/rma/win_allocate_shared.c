/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Win_allocate_shared(MPI_Aint size, int disp_unit, MPI_Info info, MPI_Comm comm,
                            void *baseptr, MPI_Win * win)
{
    int mpi_errno = MPI_SUCCESS;
    int shmbuf_regist_flag = 0;

    /* Skip internal processing when disabled */
    if (CSP_IS_DISABLED)
        return PMPI_Win_allocate_shared(size, disp_unit, info, comm, baseptr, win);

    if (comm == MPI_COMM_WORLD)
        comm = CSP_COMM_USER_WORLD;

    if (CSP_IS_MODE_ENABLED(PT2PT)) {
        mpi_errno = CSPU_check_shmbuf_regist_info(info, &shmbuf_regist_flag);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);

        /* Create shmbuf window if user sets info. */
        if (shmbuf_regist_flag)
            return CSPU_shmbuf_regist(size, disp_unit, info, comm, baseptr, win);
    }

    CSP_CALLMPI(NOSTMT, PMPI_Win_allocate_shared(size, disp_unit, info, comm, baseptr, win));
    CSP_msg_print(CSP_MSG_WARN,
                  "called MPI_Win_allocate_shared, no asynchronous progress on win 0x%x\n", *win);

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspg.h"

int run_g_main(void)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_func FUNC;
    int user_local_root, user_nprocs, user_local_nprocs;

    CSPG_DBG_PRINT(" main start\n");

    /*TODO: init in user app or here ? */
    /*    MPI_Init(&argc, &argv); */
    while (1) {
        mpi_errno = CSPG_func_start(&FUNC, &user_local_root, &user_nprocs, &user_local_nprocs);
        if (mpi_errno != MPI_SUCCESS)
            break;

        switch (FUNC) {
        case CSP_FUNC_WIN_ALLOCATE:
            mpi_errno = CSPG_win_allocate(user_local_root, user_nprocs);
            break;

        case CSP_FUNC_WIN_FREE:
            mpi_errno = CSPG_win_free(user_local_root);
            break;

            /* other commands */
        case CSP_FUNC_ABORT:
            PMPI_Abort(MPI_COMM_WORLD, 1);
            goto exit;

            break;

        case CSP_FUNC_FINALIZE:
            CSPG_finalize();
            goto exit;

            break;

        default:
            CSPG_DBG_PRINT(" FUNC %d not supported\n", (int) FUNC);
            break;
        }
    }

  exit:
    CSPG_DBG_PRINT(" main done\n");

    return mpi_errno;
}

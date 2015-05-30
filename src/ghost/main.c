#include <stdio.h>
#include <stdlib.h>
#include "cspg.h"

hashtable_t *csp_g_win_ht;

int run_g_main(void)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_Func FUNC;
    int user_local_root, user_nprocs, user_local_nprocs;

    CSP_G_DBG_PRINT(" main start\n");
    csp_init_g_win_table();

    /*TODO: init in user app or here ? */
    /*    MPI_Init(&argc, &argv); */
    while (1) {
        mpi_errno = CSP_G_func_start(&FUNC, &user_local_root, &user_nprocs, &user_local_nprocs);
        if (mpi_errno != MPI_SUCCESS)
            break;

        switch (FUNC) {
        case CSP_FUNC_WIN_ALLOCATE:
            mpi_errno = CSP_G_win_allocate(user_local_root, user_nprocs);
            break;

        case CSP_FUNC_WIN_FREE:
            mpi_errno = CSP_G_win_free(user_local_root);
            break;

            /* other commands */
        case CSP_FUNC_ABORT:
            PMPI_Abort(MPI_COMM_WORLD, 1);
            goto exit;

            break;

        case CSP_FUNC_FINALIZE:
            CSP_G_finalize();
            goto exit;

            break;

        default:
            CSP_G_DBG_PRINT(" FUNC %d not supported\n", FUNC);
            break;
        }
    }

  exit:
    CSP_G_DBG_PRINT(" main done\n");

    return mpi_errno;
}

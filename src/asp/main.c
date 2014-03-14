#include <stdio.h>
#include <stdlib.h>
#include "asp.h"

int run_asp_main(void) {
    int mpi_errno = MPI_SUCCESS;
    MPIASP_Func FUNC;
    int user_root, user_nprocs, user_tag;

    MPIASP_DBG_PRINT("[ASP] main start\n");

    /*TODO: init in user app or here ? */
    //    MPI_Init(&argc, &argv);
    while (1) {
        mpi_errno = ASP_Func_start(&FUNC, &user_root, &user_nprocs, &user_tag);
        if (mpi_errno != MPI_SUCCESS)
            break;

        MPIASP_DBG_PRINT(
                "[ASP] FUNC %d start, root %d, nprocs %d, tag %d\n", FUNC, user_root, user_nprocs, user_tag);

        switch (FUNC) {
        case MPIASP_FUNC_WIN_ALLOCATE:
            mpi_errno = ASP_Win_allocate(user_root, user_nprocs, user_tag);
            break;

        case MPIASP_FUNC_WIN_FREE:
            mpi_errno = ASP_Win_free(user_root, user_nprocs, user_tag);
            break;

            /* other commands */
        case MPIASP_FUNC_ABORT:
            PMPI_Abort(MPI_COMM_WORLD, 1);
            goto exit;

            break;

        case MPIASP_FUNC_FINALIZE:
            PMPI_Finalize();
            goto exit;

            break;

        default:
            MPIASP_DBG_PRINT("[ASP] FUNC %d not supported\n", FUNC);
            break;
        }
    }

    exit:

    MPIASP_DBG_PRINT("[ASP] main done\n");

    return mpi_errno;
}

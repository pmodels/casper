#include <stdio.h>
#include <stdlib.h>
#include "asp.h"

hashtable_t *asp_win_ht;

int run_asp_main(void)
{
    int mpi_errno = MPI_SUCCESS;
    MPIASP_Func FUNC;
    int user_local_root, user_nprocs, user_local_nprocs, user_tag;

    ASP_DBG_PRINT(" main start\n");
    init_asp_win_table();

    /*TODO: init in user app or here ? */
    /*    MPI_Init(&argc, &argv); */
    while (1) {
        mpi_errno = ASP_Func_start(&FUNC, &user_local_root, &user_nprocs, &user_local_nprocs,
                                   &user_tag);
        if (mpi_errno != MPI_SUCCESS)
            break;

        ASP_DBG_PRINT(" FUNC %d start, local root %d, nprocs %d, local nprocs %d, tag %d\n", FUNC,
                      user_local_root, user_nprocs, user_local_nprocs, user_tag);

        switch (FUNC) {
        case MPIASP_FUNC_WIN_ALLOCATE:
            mpi_errno = ASP_Win_allocate(user_local_root, user_nprocs, user_local_nprocs, user_tag);
            break;

        case MPIASP_FUNC_WIN_FREE:
            mpi_errno = ASP_Win_free(user_local_root, user_nprocs, user_local_nprocs, user_tag);
            break;

            /* other commands */
        case MPIASP_FUNC_ABORT:
            PMPI_Abort(MPI_COMM_WORLD, 1);
            goto exit;

            break;

        case MPIASP_FUNC_FINALIZE:
            ASP_Finalize();
            goto exit;

            break;

        default:
            ASP_DBG_PRINT(" FUNC %d not supported\n", FUNC);
            break;
        }
    }

  exit:

    ASP_DBG_PRINT(" main done\n");

    return mpi_errno;
}

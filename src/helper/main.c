#include <stdio.h>
#include <stdlib.h>
#include "mtcore_helper.h"

hashtable_t *mtcore_h_win_ht;

int run_h_main(void)
{
    int mpi_errno = MPI_SUCCESS;
    MTCORE_Func FUNC;
    int user_local_root, user_nprocs, user_local_nprocs, user_tag;

    MTCORE_H_DBG_PRINT(" main start\n");
    mtcore_init_h_win_table();

    /*TODO: init in user app or here ? */
    /*    MPI_Init(&argc, &argv); */
    while (1) {
        mpi_errno = MTCORE_H_func_start(&FUNC, &user_local_root, &user_nprocs, &user_local_nprocs,
                                   &user_tag);
        if (mpi_errno != MPI_SUCCESS)
            break;

        MTCORE_H_DBG_PRINT(" FUNC %d start, local root %d, nprocs %d, local nprocs %d, tag %d\n", FUNC,
                      user_local_root, user_nprocs, user_local_nprocs, user_tag);

        switch (FUNC) {
        case MTCORE_FUNC_WIN_ALLOCATE:
            mpi_errno = MTCORE_H_win_allocate(user_local_root, user_nprocs, user_local_nprocs, user_tag);
            break;

        case MTCORE_FUNC_WIN_FREE:
            mpi_errno = MTCORE_H_win_free(user_local_root, user_nprocs, user_local_nprocs, user_tag);
            break;

            /* other commands */
        case MTCORE_FUNC_ABORT:
            PMPI_Abort(MPI_COMM_WORLD, 1);
            goto exit;

            break;

        case MTCORE_FUNC_FINALIZE:
            MTCORE_H_finalize();
            goto exit;

            break;

        default:
            MTCORE_H_DBG_PRINT(" FUNC %d not supported\n", FUNC);
            break;
        }
    }

  exit:

    MTCORE_H_DBG_PRINT(" main done\n");

    return mpi_errno;
}

#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

int MPI_Finalize(void)
{
    static const char FCNAME[] = "MPI_Finalize";
    int mpi_errno = MPI_SUCCESS;
    int user_local_rank;

    PMPI_Comm_rank(MTCORE_COMM_USER_LOCAL, &user_local_rank);

    MTCORE_DBG_PRINT_FCNAME();

    /* Helpers do not need user process information because it is a global call. */
    if (user_local_rank == 0) {
        MTCORE_Func_start(MTCORE_FUNC_FINALIZE, 0, 0);
    }

    if (MTCORE_COMM_USER_WORLD != MPI_COMM_NULL) {
        MTCORE_DBG_PRINT(" free MTCORE_COMM_USER_WORLD\n");
        PMPI_Comm_free(&MTCORE_COMM_USER_WORLD);
    }

    if (MTCORE_COMM_LOCAL != MPI_COMM_NULL) {
        MTCORE_DBG_PRINT(" free MTCORE_COMM_LOCAL\n");
        PMPI_Comm_free(&MTCORE_COMM_LOCAL);
    }

    if (MTCORE_COMM_USER_LOCAL != MPI_COMM_NULL) {
        MTCORE_DBG_PRINT(" free MTCORE_COMM_USER_LOCAL\n");
        PMPI_Comm_free(&MTCORE_COMM_USER_LOCAL);
    }

    if (MTCORE_COMM_UR_WORLD != MPI_COMM_NULL) {
        MTCORE_DBG_PRINT(" free MTCORE_COMM_UR_WORLD\n");
        PMPI_Comm_free(&MTCORE_COMM_UR_WORLD);
    }

    if (MTCORE_COMM_HELPER_LOCAL != MPI_COMM_NULL) {
        MTCORE_DBG_PRINT("free MTCORE_COMM_HELPER_LOCAL\n");
        PMPI_Comm_free(&MTCORE_COMM_HELPER_LOCAL);
    }

    if (MTCORE_GROUP_WORLD != MPI_GROUP_NULL)
        PMPI_Group_free(&MTCORE_GROUP_WORLD);
    if (MTCORE_GROUP_LOCAL != MPI_GROUP_NULL)
        PMPI_Group_free(&MTCORE_GROUP_LOCAL);
    if (MTCORE_GROUP_USER_WORLD != MPI_GROUP_NULL)
        PMPI_Group_free(&MTCORE_GROUP_USER_WORLD);

    MTCORE_Destroy_win_cache();

    if (MTCORE_H_RANKS_IN_WORLD)
        free(MTCORE_H_RANKS_IN_WORLD);

    if (MTCORE_H_RANKS_IN_LOCAL)
        free(MTCORE_H_RANKS_IN_LOCAL);

    if (MTCORE_ALL_H_RANKS_IN_WORLD)
        free(MTCORE_ALL_H_RANKS_IN_WORLD);

    if (MTCORE_ALL_NODE_IDS)
        free(MTCORE_ALL_NODE_IDS);

    if (MTCORE_USER_RANKS_IN_WORLD)
        free(MTCORE_USER_RANKS_IN_WORLD);

    mpi_errno = PMPI_Finalize();
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

#include <stdio.h>
#include <stdlib.h>
#include "asp.h"

#undef FUNCNAME
#define FUNCNAME ASP_Win_free

int ASP_Win_free(int user_root, int user_nprocs, int user_tag) {
    int mpi_errno = MPI_SUCCESS;
    int dst;
    int ua_nprocs, ua_rank;
    ASP_Win *win;
    MPI_Win win_bkup;

    win = remove_asp_win(0);

    /* Release ASP resources if there is a corresponding ASP-window */
    if (win > 0) {
        win_bkup = win->win;

        for (dst = 0; dst < user_nprocs; dst++) {
            //-Free window shared with user processes
            if (win->all_shrd_wins[dst]) {
                MPIASP_DBG_PRINT("[ASP] \t free shared window[%d]\n", dst);
                mpi_errno = PMPI_Win_free(&win->all_shrd_wins[dst]);
                if (mpi_errno != MPI_SUCCESS)
                    goto fn_fail;
            }

            //-Free communicator including local process and ASP
            if (win->all_shrd_comms[dst]) {
                MPIASP_DBG_PRINT("[ASP] \t free shared communicator[%d]\n", dst);
                mpi_errno = PMPI_Comm_free(&win->all_shrd_comms[dst]);
                if (mpi_errno != MPI_SUCCESS)
                    goto fn_fail;
            }
        }

        //-Free window including user processes and ASP
        if (win->win) {
            MPIASP_DBG_PRINT("[ASP] \t free ua window\n");
            mpi_errno = PMPI_Win_free(&win->win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        //-Free communicator including user processes and ASP
        if (win->ua_comm) {
            MPIASP_DBG_PRINT("[ASP] \t free ua communicator\n");
            mpi_errno = PMPI_Comm_free(&win->ua_comm);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        if (win->all_shrd_base_addrs)
            free(win->all_shrd_base_addrs);
        if (win->all_shrd_comms)
            free(win->all_shrd_comms);
        if (win->all_shrd_wins)
            free(win->all_shrd_wins);

        free(win);

        MPIASP_DBG_PRINT( "[ASP] Freed ASP window 0x%x\n", win_bkup);
    } else {
        MPIASP_DBG_PRINT(
                "[ASP] no corresponding ASP window, tag 0x%x\n", user_tag);
    }

    fn_exit:

    return mpi_errno;

    fn_fail:

    goto fn_exit;
}

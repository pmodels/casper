#include <stdio.h>
#include <stdlib.h>
#include "asp.h"

#undef FUNCNAME
#define FUNCNAME ASP_Win_free

/**
 * TODO: should implement table[user_win_handle : asp_win object]
 */
ASP_Win *asp_win_table[2];

int ASP_Win_free(int user_root, int user_nprocs, int user_tag) {
    int mpi_errno = MPI_SUCCESS;
    int dst;
    int ua_nprocs, ua_rank;
    ASP_Win *win;
    MPI_Win win_bkup;

    win = remove_ua_win(0);

    /* Release ASP resources if there is a corresponding ASP-window */
    if (win > 0) {
        win_bkup = win->win;

        for (dst = 0; dst < user_nprocs; dst++) {
            if (win->all_shrd_wins[dst]) {
                mpi_errno = PMPI_Win_free(win->all_shrd_wins[dst]);
                if (mpi_errno != MPI_SUCCESS)
                    goto fn_fail;
            }
            if (win->all_shrd_comms[dst]) {
                mpi_errno = PMPI_Comm_free(win->all_shrd_comms[dst]);
                if (mpi_errno != MPI_SUCCESS)
                    goto fn_fail;
            }
        }
        if (win->win > 0) {
            mpi_errno = PMPI_Win_free(win->win);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
        if (win->ua_comm) {
            mpi_errno = PMPI_Comm_free(win->ua_comm);
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

        MPIASP_DBG_PRINT( "[ASP] Freed ASP window 0x%lx\n", win_bkup);
    } else {
        MPIASP_DBG_PRINT(
                "[ASP] no corresponding ASP window, tag 0x%lx\n", user_tag);
    }

    fn_exit:

    return mpi_errno;

    fn_fail:

    goto fn_exit;
}

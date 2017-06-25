/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

CSPU_shmbuf_record_t *CSPU_shmbuf_list = NULL;

int CSPU_shmbuf_free(MPI_Win * win, int *freed)
{
    int mpi_errno = MPI_SUCCESS;
    CSPU_shmbuf_win_t *shmbuf_win = NULL;
    int ulrank = 0, lrank = 0;
    MPI_Request *reqs = NULL;

    *freed = 0;

    mpi_errno = CSPU_fetch_shmbuf_win_from_cache(*win, &shmbuf_win);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    if (shmbuf_win) {
        CSP_CALLMPI(JUMP, PMPI_Comm_rank(shmbuf_win->ug_comm->local_user_comm, &ulrank));
        CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_PROC.local_comm, &lrank));

        if (ulrank == 0) {
            int i;
            CSP_cwp_pkt_t pkt;
            CSP_cwp_shmbuf_free_pkt_t *shmbuf_free_pkt = &pkt.u.fnc_shmbuf_free;

            /* Send command to root ghost.
             * Do not mlock ghost because only one node. */
            CSP_cwp_init_pkt(CSP_CWP_FNC_SHMBUF_FREE, &pkt);
            shmbuf_free_pkt->user_local_root = lrank;

            mpi_errno = CSPU_cwp_issue(&pkt);
            CSP_CHKMPIFAIL_JUMP(mpi_errno);

            /* Send the handle of window to each ghost. */
            reqs = CSP_calloc(CSP_ENV.num_g, sizeof(MPI_Request));
            for (i = 0; i < CSP_ENV.num_g; i++) {
                CSP_CALLMPI(JUMP, PMPI_Isend(&shmbuf_win->g_win_handles[i], 1, MPI_AINT,
                                             CSP_PROC.user.g_lranks[i], CSP_CWP_PARAM_TAG,
                                             CSP_PROC.local_comm, &reqs[i]));

                CSP_DBG_PRINT("SHMBUF free: sent g_win_handles[%d]=0x%lx\n", i,
                              shmbuf_win->g_win_handles[i]);
            }
            CSP_CALLMPI(JUMP, PMPI_Waitall(CSP_ENV.num_g, reqs, MPI_STATUS_IGNORE));

            free(shmbuf_win->g_win_handles);
        }

        CSPU_shmbuf_record_remove(shmbuf_win->base);

        mpi_errno = CSPU_remove_shmbuf_win_from_cache(*win);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);

        CSP_DBG_PRINT("SHMBUF free: shmbuf_win=%p, win=0x%x\n", shmbuf_win, *win);
        CSP_CALLMPI(JUMP, PMPI_Win_free(win));
        free(shmbuf_win);

        *freed = 1;
    }

  fn_exit:
    if (reqs)
        free(reqs);
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

int CSPU_shmbuf_regist(CSPU_comm_t * ug_comm, MPI_Aint size, int disp_unit, MPI_Info info,
                       MPI_Comm comm, void *baseptr, MPI_Win * win)
{
    int mpi_errno = MPI_SUCCESS;
    CSPU_shmbuf_win_t *shmbuf_win = NULL;
    CSP_cwp_pkt_t pkt;
    CSP_cwp_shmbuf_regist_pkt_t *shmbuf_regist_pkt = &pkt.u.fnc_shmbuf_regist;
    int ulrank = 0, lrank = 0;
    void **base_pp = (void **) baseptr;
    MPI_Request *reqs = NULL;
    int i;

    shmbuf_win = CSP_calloc(1, sizeof(CSPU_shmbuf_win_t));
    CSP_ASSERT(shmbuf_win != NULL);

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(ug_comm->local_user_comm, &ulrank));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_PROC.local_comm, &lrank));

    if (ulrank == 0) {
        /* Send command to root ghost.
         * Do not mlock ghost because only one node. */
        CSP_cwp_init_pkt(CSP_CWP_FNC_SHMBUF_REGIST, &pkt);
        shmbuf_regist_pkt->user_local_root = lrank;

        mpi_errno = CSPU_cwp_issue(&pkt);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);

        /* Send the handle of ug_comm to each ghost. */
        reqs = CSP_calloc(CSP_ENV.num_g, sizeof(MPI_Request));
        for (i = 0; i < CSP_ENV.num_g; i++) {
            CSP_CALLMPI(JUMP, PMPI_Isend(&ug_comm->g_ugcomm_handles[i], 1, MPI_AINT,
                                         CSP_PROC.user.g_lranks[i], CSP_CWP_PARAM_TAG,
                                         CSP_PROC.local_comm, &reqs[i]));
        }
        CSP_CALLMPI(JUMP, PMPI_Waitall(CSP_ENV.num_g, reqs, MPI_STATUS_IGNORE));
    }

    /* Create shared buffer window. */
    CSP_CALLMPI(NOSTMT, PMPI_Win_allocate_shared(size, disp_unit, info,
                                                 ug_comm->ug_comm, &shmbuf_win->base,
                                                 &shmbuf_win->win));

    /* Receive the address of my shared buffer on bound ghost process.
     * This is used to translate my user buffer to ghost address at offloading call.*/
    mpi_errno = CSPU_cwp_recv_params(&shmbuf_win->g_base_bound, sizeof(MPI_Aint),
                                     CSPU_offload_ch.bound_g_lrank);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* Gather window handles on user root. Used at win_free. */
    if (ulrank == 0) {
        shmbuf_win->g_win_handles = CSP_calloc(CSP_ENV.num_g, sizeof(MPI_Aint));
        mpi_errno = CSPU_cwp_gather_params(shmbuf_win->g_win_handles, sizeof(MPI_Aint));
        CSP_CHKMPIFAIL_JUMP(mpi_errno);

        for (i = 0; i < CSP_ENV.num_g; i++) {
            CSP_DBG_PRINT("SHMBUF: received g_win_handles[%d]=0x%lx\n", i,
                          shmbuf_win->g_win_handles[i]);
        }
    }

    /* Cache it thus can free at win_free. */
    mpi_errno = CSPU_cache_shmbuf_win(shmbuf_win->win, shmbuf_win);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* Cache the address and scope, used to translate user buffer address. */
    CSPU_shmbuf_record_append(shmbuf_win->base, size, shmbuf_win->g_base_bound);

    (*base_pp) = shmbuf_win->base;
    (*win) = shmbuf_win->win;
    shmbuf_win->size = size;
    shmbuf_win->ug_comm = ug_comm;

    CSP_DBG_PRINT
        ("SHMBUF: created shm_win=%p, win=0x%x, base=%p, size=0x%lx, disp_unit=%d, g_base_bound=0x%lx\n",
         shmbuf_win, shmbuf_win->win, shmbuf_win->base, size, disp_unit, shmbuf_win->g_base_bound);

  fn_exit:
    if (reqs)
        free(reqs);
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

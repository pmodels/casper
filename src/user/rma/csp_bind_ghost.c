/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include "cspu.h"

static int bind_by_ranks(int n_targets, int *local_targets, CSP_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int i, g_off, t_rank, user_nprocs;
    int np_per_ghost;

    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);

    np_per_ghost = n_targets / CSP_ENV.num_g;

    int np = np_per_ghost;
    i = 0;
    g_off = 0;
    t_rank = local_targets[i];

    while (i < n_targets) {
        if (np == 0) {
            /* next ghost */
            g_off++;
            np = np_per_ghost + ((g_off == CSP_ENV.num_g - 1) ? (n_targets % CSP_ENV.num_g) : 0);
        }
        CSP_assert(g_off <= CSP_ENV.num_g);

        t_rank = local_targets[i];
        ug_win->targets[t_rank].num_segs = 1;
        ug_win->targets[t_rank].segs = CSP_calloc(1, sizeof(CSP_win_target_seg_t));
        ug_win->targets[t_rank].segs[0].base_offset = 0;
        ug_win->targets[t_rank].segs[0].size = ug_win->targets[i].size;
        ug_win->targets[t_rank].segs[0].main_g_off = g_off;

        /* next target */
        i++;
        np--;
    }

    return mpi_errno;
}

/**
 * Bind every target in the window with single ghost process to
 * guarantee lock permission, accumulate ordering and atomicity.
 */
int CSP_win_bind_ghosts(CSP_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int i, user_nprocs;
    int *local_targets = NULL;

    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);
    local_targets = CSP_calloc(ug_win->num_nodes * ug_win->max_local_user_nprocs, sizeof(int));

    /* Sort targets by node_ids */
    for (i = 0; i < user_nprocs; i++) {
        int off = ug_win->targets[i].node_id * ug_win->max_local_user_nprocs +
            ug_win->targets[i].local_user_rank;
        local_targets[off] = i;
    }

    /* Specify main ghosts on each node */
    for (i = 0; i < ug_win->num_nodes; i++) {
        int s_off = i * ug_win->max_local_user_nprocs;
        int s_rank = local_targets[s_off];      /* my local targets */
        int n_targets = ug_win->targets[s_rank].local_user_nprocs;

        mpi_errno = bind_by_ranks(n_targets, &local_targets[s_off], ug_win);
    }

#ifdef CSP_DEBUG
    int j;
    for (i = 0; i < user_nprocs; i++) {
        CSP_DBG_PRINT("\t target[%d] .num_segs %d\n", i, ug_win->targets[i].num_segs);
        for (j = 0; j < CSP_ENV.num_g; j++) {
            CSP_DBG_PRINT("\t\t .g_rank[%d] %d, offset[%d] 0x%lx \n",
                          j, ug_win->targets[i].g_ranks_in_ug[j],
                          j, ug_win->targets[i].base_g_offsets[j]);
        }
        for (j = 0; j < ug_win->targets[i].num_segs; j++) {
            CSP_DBG_PRINT("\t\t .seg[%d].main_g_off=%d, base_offset=0x%lx, size=0x%x\n",
                          j, ug_win->targets[i].segs[j].main_g_off,
                          ug_win->targets[i].segs[j].base_offset, ug_win->targets[i].segs[j].size);
        }
    }
#endif

    if (local_targets)
        free(local_targets);

    return mpi_errno;
}

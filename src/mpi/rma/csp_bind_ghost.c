/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include "csp.h"

static int bind_by_segments(int n_targets, int *local_targets, CSP_Win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint seg_size, t_size, sum_size, size_per_ghost, assigned_size, max_t_size;
    int t_num_segs, t_last_g_off, max_t_num_seg;
    int i, j, g_off, t_rank, user_nprocs;
    MPI_Aint *t_seg_sizes = NULL;

    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);

    /* Calculate size binded to each ghost */
    max_t_size = 0;
    sum_size = 0;
    for (i = 0; i < n_targets; i++) {
        t_rank = local_targets[i];
        CSP_Assert(t_rank < user_nprocs);

        sum_size += ug_win->targets[t_rank].size;
        max_t_size = max(max_t_size, ug_win->targets[t_rank].size);
    }
    /* Never divide less than segment unit */
    size_per_ghost = align(sum_size / CSP_ENV.num_g, CSP_SEGMENT_UNIT);
    max_t_num_seg = sum_size / size_per_ghost + 3;
    t_seg_sizes = calloc(max_t_num_seg, sizeof(MPI_Aint));

    /* Divide segments based on sizes */
    i = 0;
    t_rank = local_targets[0];
    g_off = 0;
    t_last_g_off = 0;
    t_size = ug_win->targets[0].size;
    t_num_segs = 0;
    assigned_size = 0;
    seg_size = size_per_ghost;
    while (assigned_size < sum_size && i < n_targets) {
        if (t_size == 0) {
            ug_win->targets[t_rank].num_segs = t_num_segs;
            ug_win->targets[t_rank].segs = calloc(t_num_segs, sizeof(CSP_Win_target_seg));

            MPI_Aint prev_seg_base = 0, prev_seg_size = 0;
            for (j = 0; j < t_num_segs; j++) {
                ug_win->targets[t_rank].segs[j].base_offset = prev_seg_base + prev_seg_size;
                ug_win->targets[t_rank].segs[j].size = t_seg_sizes[j];
                ug_win->targets[t_rank].segs[j].main_g_off = t_last_g_off + 1 - t_num_segs + j;

                CSP_Assert(ug_win->targets[t_rank].segs[j].main_g_off < CSP_ENV.num_g);

                prev_seg_base = ug_win->targets[t_rank].segs[j].base_offset;
                prev_seg_size = ug_win->targets[t_rank].segs[j].size;
            }

            assigned_size += ug_win->targets[t_rank].size;

            /* next target */
            if (i >= n_targets) {
                break;
            }
            t_rank = local_targets[++i];
            t_size = ug_win->targets[t_rank].size;
            t_num_segs = 0;
        }
        /* finish this target if remaining size is small than seg_size or
         * it is already at the last ghost. */
        else if (t_size < seg_size || g_off == CSP_ENV.num_g - 1) {
            CSP_Assert(t_num_segs < max_t_num_seg);

            t_seg_sizes[t_num_segs++] = t_size;
            seg_size -= t_size;
            /* make sure remaining segment size is aligned */
            seg_size = align(seg_size, CSP_SEGMENT_UNIT);

            t_size = 0;

            t_last_g_off = g_off;
        }
        else if (t_size >= seg_size) {
            CSP_Assert(t_num_segs < max_t_num_seg);

            /* divide large target */
            t_seg_sizes[t_num_segs++] = seg_size;
            t_size -= seg_size;
            seg_size = 0;

            t_last_g_off = g_off;
        }

        /* next ghost */
        if (seg_size == 0) {
            g_off++;
            CSP_Assert(g_off <= CSP_ENV.num_g);
            seg_size = size_per_ghost
                + (g_off == CSP_ENV.num_g - 1 ? (sum_size % CSP_ENV.num_g) : 0);
            /* make sure new segment size is aligned */
            seg_size = align(seg_size, CSP_SEGMENT_UNIT);
        }
    }

    CSP_Assert(assigned_size == sum_size);

    if (t_seg_sizes)
        free(t_seg_sizes);

    return mpi_errno;
}

static int bind_by_ranks(int n_targets, int *local_targets, CSP_Win * ug_win)
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
        CSP_Assert(g_off <= CSP_ENV.num_g);

        t_rank = local_targets[i];
        ug_win->targets[t_rank].num_segs = 1;
        ug_win->targets[t_rank].segs = calloc(1, sizeof(CSP_Win_target_seg));
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
int CSP_Win_bind_ghosts(CSP_Win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int i, user_nprocs;
    int *local_targets = NULL;

    PMPI_Comm_size(ug_win->user_comm, &user_nprocs);
    local_targets = calloc(ug_win->num_nodes * ug_win->max_local_user_nprocs, sizeof(int));

    /* Sort targets by node_ids */
    for (i = 0; i < user_nprocs; i++) {
        int off = ug_win->targets[i].node_id * ug_win->max_local_user_nprocs +
            ug_win->targets[i].local_user_rank;
        local_targets[off] = i;
    }

    /* Specify main ghosts on each node */
    if (CSP_ENV.lock_binding == CSP_LOCK_BINDING_SEGMENT) {
        for (i = 0; i < ug_win->num_nodes; i++) {
            int s_off = i * ug_win->max_local_user_nprocs;
            int s_rank = local_targets[s_off];  /* my local targets */
            int n_targets = ug_win->targets[s_rank].local_user_nprocs;

            mpi_errno = bind_by_segments(n_targets, &local_targets[s_off], ug_win);
        }
    }
    else {
        for (i = 0; i < ug_win->num_nodes; i++) {
            int s_off = i * ug_win->max_local_user_nprocs;
            int s_rank = local_targets[s_off];  /* my local targets */
            int n_targets = ug_win->targets[s_rank].local_user_nprocs;

            mpi_errno = bind_by_ranks(n_targets, &local_targets[s_off], ug_win);
        }
    }

#ifdef DEBUG
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

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

/* dummy variable to avoid warnings on Mac OS/X with respect to no
 * symbols found in the file */
static int dummy CSP_ATTRIBUTE((unused, used)) = 0;

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
void CSPU_target_get_ghost_opload_by_op(int target_rank, int is_order_required,
                                        CSPU_win_t * ug_win, int *target_g_rank_in_ug,
                                        int *target_g_rank_idx, MPI_Aint * target_g_offset)
{
    int idx, min_count, g_rank, min_idx;

    /* Choose the ghost who has the lowest value of operation counting. */
    g_rank = ug_win->targets[target_rank].g_ranks_in_ug[0];
    min_count = ug_win->g_ops_counts[g_rank];
    min_idx = 0;

    for (idx = 1; idx < CSP_ENV.num_g; idx++) {
        g_rank = ug_win->targets[target_rank].g_ranks_in_ug[idx];
        if (ug_win->g_ops_counts[g_rank] < min_count) {
            min_count = ug_win->g_ops_counts[g_rank];
            min_idx = idx;
        }
    }

    *target_g_rank_in_ug = ug_win->targets[target_rank].g_ranks_in_ug[min_idx];
    *target_g_offset = ug_win->targets[target_rank].base_g_offsets[min_idx];
    *target_g_rank_idx = min_idx;

    CSP_DBG_PRINT("[load_opt_op] choose lowest counting ghost %d, off 0x%lx for target %d\n",
                  *target_g_rank_in_ug, *target_g_offset, target_rank);

    /* Count the number of operations issued to every ghost */
    CSPU_inc_target_opload_op_counting(*target_g_rank_in_ug, ug_win);

    return;
}

void CSPU_target_get_ghost_opload_by_byte(int target_rank, int is_order_required, int size,
                                          CSPU_win_t * ug_win, int *target_g_rank_in_ug,
                                          int *target_g_rank_idx, MPI_Aint * target_g_offset)
{
    int idx, min_count, g_rank, min_idx;

    /* Choose the ghost who has the lowest value of operation counting. */
    g_rank = ug_win->targets[target_rank].g_ranks_in_ug[0];
    min_count = ug_win->g_bytes_counts[g_rank];
    min_idx = 0;

    for (idx = 1; idx < CSP_ENV.num_g; idx++) {
        g_rank = ug_win->targets[target_rank].g_ranks_in_ug[idx];
        if (ug_win->g_bytes_counts[g_rank] < min_count) {
            min_count = ug_win->g_bytes_counts[g_rank];
            min_idx = idx;
        }
    }

    *target_g_rank_in_ug = ug_win->targets[target_rank].g_ranks_in_ug[min_idx];
    *target_g_offset = ug_win->targets[target_rank].base_g_offsets[min_idx];
    *target_g_rank_idx = min_idx;

    CSP_DBG_PRINT("[load_opt_byte] choose lowest counting ghost %d, off 0x%lx for target %d\n",
                  *target_g_rank_in_ug, *target_g_offset, target_rank);

    /* Count the number of operations issued to every ghost */
    CSPU_inc_target_opload_bytes_counting(*target_g_rank_in_ug, size, ug_win);

    return;
}
#endif

/*
 * get_helper.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

#if defined(MTCORE_ENABLE_RUNTIME_LOAD_OPT)
void MTCORE_Get_helper_rank_load_opt_counting(int target_rank, int is_order_required,
                                              MTCORE_Win * uh_win, int *target_h_rank_in_uh,
                                              int *target_h_rank_idx, MPI_Aint * target_h_offset)
{
    int idx, min_count, h_rank, min_idx;

    /* Choose the helper who has the lowest value of operation counting. */
    h_rank = uh_win->targets[target_rank].h_ranks_in_uh[0];
    min_count = uh_win->h_ops_counts[h_rank];
    min_idx = 0;

    for (idx = 1; idx < MTCORE_ENV.num_h; idx++) {
        h_rank = uh_win->targets[target_rank].h_ranks_in_uh[idx];
        if (uh_win->h_ops_counts[h_rank] < min_count) {
            min_count = uh_win->h_ops_counts[h_rank];
            min_idx = idx;
        }
    }

    *target_h_rank_in_uh = uh_win->targets[target_rank].h_ranks_in_uh[min_idx];
    *target_h_offset = uh_win->targets[target_rank].base_h_offsets[min_idx];
    *target_h_rank_idx = min_idx;

    MTCORE_DBG_PRINT("[load_opt_op] choose lowest counting helper %d, off 0x%lx for target %d\n",
                     *target_h_rank_in_uh, *target_h_offset, target_rank);

    /* Count the number of operations issued to every helper */
    MTCORE_Inc_win_target_load_opt_op_counting(*target_h_rank_in_uh, uh_win);

    return;
}

void MTCORE_Get_helper_rank_load_byte_counting(int target_rank, int is_order_required, int size,
                                               MTCORE_Win * uh_win, int *target_h_rank_in_uh,
                                               int *target_h_rank_idx, MPI_Aint * target_h_offset)
{
    int idx, min_count, h_rank, min_idx;

    /* Choose the helper who has the lowest value of operation counting. */
    h_rank = uh_win->targets[target_rank].h_ranks_in_uh[0];
    min_count = uh_win->h_bytes_counts[h_rank];
    min_idx = 0;

    for (idx = 1; idx < MTCORE_ENV.num_h; idx++) {
        h_rank = uh_win->targets[target_rank].h_ranks_in_uh[idx];
        if (uh_win->h_bytes_counts[h_rank] < min_count) {
            min_count = uh_win->h_bytes_counts[h_rank];
            min_idx = idx;
        }
    }

    *target_h_rank_in_uh = uh_win->targets[target_rank].h_ranks_in_uh[min_idx];
    *target_h_offset = uh_win->targets[target_rank].base_h_offsets[min_idx];
    *target_h_rank_idx = min_idx;

    MTCORE_DBG_PRINT("[load_opt_byte] choose lowest counting helper %d, off 0x%lx for target %d\n",
                     *target_h_rank_in_uh, *target_h_offset, target_rank);

    /* Count the number of operations issued to every helper */
    MTCORE_Inc_win_target_load_opt_bytes_counting(*target_h_rank_in_uh, size, uh_win);

    return;
}
#endif

/*
 * get_helper.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

#if (MTCORE_LOAD_OPT == MTCORE_LOAD_OPT_COUNTING)
void MTCORE_Get_helper_rank_load_opt_counting(int target_rank, int is_order_required,
                                              MTCORE_Win * uh_win, int *target_h_rank_in_uh,
                                              MPI_Aint * target_h_offset)
{
    /* Upgrade main lock status of target if it is the first operation of that target. */
    if (uh_win->targets[target_rank].main_lock_stat == MTCORE_MAIN_LOCK_RESET) {
        uh_win->targets[target_rank].main_lock_stat = MTCORE_MAIN_LOCK_OP_ISSUED;
    }

    /* If lock has not been granted yet, we can only use the main helper. */
    if (!(uh_win->targets[target_rank].remote_lock_assert & MPI_MODE_NOCHECK) &&
        uh_win->targets[target_rank].main_lock_stat != MTCORE_MAIN_LOCK_GRANTED) {
        /* Both serial async and byte tracking options specify the first helper as
         * the main helper of that user process.*/
        *target_h_rank_in_uh = uh_win->targets[target_rank].h_ranks_in_uh[0];
        *target_h_offset = uh_win->targets[target_rank].base_h_offsets[0];

        MTCORE_DBG_PRINT("[opt_cnt] use main helper %d, off 0x%lx for target %d\n",
                         *target_h_rank_in_uh, *target_h_offset, target_rank);
        goto fn_exit;
    }

    /* For ordering required operations, just return the helper chosen in the
     * first time. */
    if (!uh_win->info_args.no_accumulate_ordering &&
        is_order_required && uh_win->targets[target_rank].order_h_index != -1) {
        int h_idx = uh_win->targets[target_rank].order_h_index;
        *target_h_rank_in_uh = uh_win->targets[target_rank].h_ranks_in_uh[h_idx];
        *target_h_offset = uh_win->targets[target_rank].base_h_offsets[h_idx];

        MTCORE_DBG_PRINT("[opt_cnt] use first ordered helper %d, off 0x%lx for target %d\n",
                         *target_h_rank_in_uh, *target_h_offset, target_rank);
        goto fn_exit;
    }

    /* Choose the helper who has the lowest value of operation counting. */
    int idx, min_count, h_rank, min_idx;

    h_rank = uh_win->targets[target_rank].h_ranks_in_uh[0];
    min_count = uh_win->h_ops_counts[h_rank];
    min_idx = 0;

    for (idx = 1; idx < MTCORE_NUM_H; idx++) {
        h_rank = uh_win->targets[target_rank].h_ranks_in_uh[idx];
        if (uh_win->h_ops_counts[h_rank] < min_count) {
            min_count = uh_win->h_ops_counts[h_rank];
            min_idx = idx;
        }
    }

    *target_h_rank_in_uh = uh_win->targets[target_rank].h_ranks_in_uh[min_idx];
    *target_h_offset = uh_win->targets[target_rank].base_h_offsets[min_idx];

    MTCORE_DBG_PRINT("[opt_cnt] choose lowest counting helper %d, off 0x%lx for target %d\n",
                     *target_h_rank_in_uh, *target_h_offset, target_rank);

    /* Remember the helper for ordering required operations to a given target.
     * Because both not-lock-granted and not-first-ordered targets do not need remember,
     * we put it before fn_exit.*/
    if (!uh_win->info_args.no_accumulate_ordering && is_order_required) {
        uh_win->targets[target_rank].order_h_index = min_idx;
    }

  fn_exit:
    /* Count the number of operations issued to every helper */
    uh_win->h_ops_counts[*target_h_rank_in_uh]++;

    return;
}


#elif (MTCORE_LOAD_OPT == MTCORE_LOAD_BYTE_COUNTING)
void MTCORE_Get_helper_rank_load_byte_counting(int target_rank, int is_order_required, int size,
                                               MTCORE_Win * uh_win, int *target_h_rank_in_uh,
                                               MPI_Aint * target_h_offset)
{
    /* Upgrade main lock status of target if it is the first operation of that target. */
    if (uh_win->targets[target_rank].main_lock_stat == MTCORE_MAIN_LOCK_RESET) {
        uh_win->targets[target_rank].main_lock_stat = MTCORE_MAIN_LOCK_OP_ISSUED;
    }

    /* If lock has not been granted yet, we can only use the main helper. */
    if (!(uh_win->targets[target_rank].remote_lock_assert & MPI_MODE_NOCHECK) &&
        uh_win->targets[target_rank].main_lock_stat != MTCORE_MAIN_LOCK_GRANTED) {
        /* Both serial async and byte tracking options specify the first helper as
         * the main helper of that user process.*/
        *target_h_rank_in_uh = uh_win->targets[target_rank].h_ranks_in_uh[0];
        *target_h_offset = uh_win->targets[target_rank].base_h_offsets[0];

        MTCORE_DBG_PRINT("[byte_cnt] use main helper %d, off 0x%lx for target %d\n",
                         *target_h_rank_in_uh, *target_h_offset, target_rank);
        goto fn_exit;
    }

    /* For ordering required operations, just return the helper chosen in the
     * first time. */
    if (!uh_win->info_args.no_accumulate_ordering &&
        is_order_required && uh_win->targets[target_rank].order_h_index != -1) {
        int h_idx = uh_win->targets[target_rank].order_h_index;
        *target_h_rank_in_uh = uh_win->targets[target_rank].h_ranks_in_uh[h_idx];
        *target_h_offset = uh_win->targets[target_rank].base_h_offsets[h_idx];

        MTCORE_DBG_PRINT("[byte_cnt] use first ordered helper %d, off 0x%lx for target %d\n",
                         *target_h_rank_in_uh, *target_h_offset, target_rank);
        goto fn_exit;
    }

    /* Choose the helper who has the lowest value of operation counting. */
    int idx, min_count, h_rank, min_idx;

    h_rank = uh_win->targets[target_rank].h_ranks_in_uh[0];
    min_count = uh_win->h_bytes_counts[h_rank];
    min_idx = 0;

    for (idx = 1; idx < MTCORE_NUM_H; idx++) {
        h_rank = uh_win->targets[target_rank].h_ranks_in_uh[idx];
        if (uh_win->h_bytes_counts[h_rank] < min_count) {
            min_count = uh_win->h_bytes_counts[h_rank];
            min_idx = idx;
        }
    }

    *target_h_rank_in_uh = uh_win->targets[target_rank].h_ranks_in_uh[min_idx];
    *target_h_offset = uh_win->targets[target_rank].base_h_offsets[min_idx];

    MTCORE_DBG_PRINT("[byte_cnt] choose lowest counting helper %d, off 0x%lx for target %d\n",
                     *target_h_rank_in_uh, *target_h_offset, target_rank);

    /* Remember the helper for ordering required operations to a given target.
     * Because both not-lock-granted and not-first-ordered targets do not need remember,
     * we put it before fn_exit.*/
    if (!uh_win->info_args.no_accumulate_ordering && is_order_required) {
        uh_win->targets[target_rank].order_h_index = min_idx;
    }

  fn_exit:
    /* Count the number of operations issued to every helper */
    uh_win->h_bytes_counts[*target_h_rank_in_uh] += size;

    return;
}
#endif

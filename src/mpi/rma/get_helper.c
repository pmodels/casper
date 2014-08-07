/*
 * get_helper.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */

#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"

void MTCORE_Get_helper_rank(int target_rank, int is_order_required,
                            MTCORE_Win * uh_win, int *target_h_rank_in_uh)
{
#if (MTCORE_LOAD_OPT != MTCORE_LOAD_OPT_NON)
    /* Upgrade main lock status of target if it is the first operation of that target. */
    if (uh_win->is_main_lock_granted[target_rank] == MTCORE_MAIN_LOCK_RESET) {
        uh_win->is_main_lock_granted[target_rank] = MTCORE_MAIN_LOCK_OP_ISSUED;
    }
#endif

#if (MTCORE_LOCK_OPTION != MTCORE_LOCK_OPTION_FORCE_LOCK)
    /* If lock has not been granted yet, we can only use the main helper. */
    if (uh_win->is_main_lock_granted[target_rank] != MTCORE_MAIN_LOCK_GRANTED) {
        /* Both serial async and byte tracking options specify the first helper as
         * the main helper of that user process.*/
        *target_h_rank_in_uh = uh_win->h_ranks_in_uh[target_rank * MTCORE_NUM_H];
        goto fn_exit;
    }
#endif

    /* Either Force Locked or Lock Granted targets can be load balanced using multiple
     * helpers. */

    /* For ordering required operations, just return the helper chosen in the
     * first time. */
    if (is_order_required && uh_win->order_h_ranks_in_uh[target_rank] != -1) {
        *target_h_rank_in_uh = uh_win->order_h_ranks_in_uh[target_rank];
        goto fn_exit;
    }

#if (MTCORE_LOAD_OPT == MTCORE_LOAD_OPT_RANDOM)
    /* Randomly change helper offset every time using a window-level global recorder */
    int off = (uh_win->prev_h_off + 1) % MTCORE_NUM_H;  /* jump to next helper offset */
    uh_win->prev_h_off = off;

    *target_h_rank_in_uh = uh_win->h_ranks_in_uh[target_rank * MTCORE_NUM_H + off];

#elif (MTCORE_LOAD_OPT == MTCORE_LOAD_OPT_COUNTING)
    /* Count the number of operations issued to every helper, choose the helper
     * who has the lowest value. */
    int i, min_h_rank = 0, min_count;
    int h_rank;

    h_rank = uh_win->h_ranks_in_uh[target_rank * MTCORE_NUM_H];
    min_count = uh_win->h_ops_counts[h_rank];
    min_h_rank = h_rank;

    for (i = 1; i < MTCORE_NUM_H; i++) {
        h_rank = uh_win->h_ranks_in_uh[target_rank * MTCORE_NUM_H + i];
        if (uh_win->h_ops_counts[h_rank] < min_count) {
            min_count = uh_win->h_ops_counts[h_rank];
            min_h_rank = h_rank;
        }
    }

    *target_h_rank_in_uh = min_h_rank;
#endif

    /* Remember the helper for ordering required operations to a given target.
     * Because both not-lock-granted and not-first-ordered targets do not need remember,
     * we put it before fn_exit.*/
    if (is_order_required) {
        uh_win->order_h_ranks_in_uh[target_rank] = *target_h_rank_in_uh;
    }

  fn_exit:

#if (MTCORE_LOAD_OPT == MTCORE_LOAD_OPT_COUNTING)
    uh_win->h_ops_counts[*target_h_rank_in_uh]++;
#endif

    return;
}

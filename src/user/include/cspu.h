/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSPU_H_
#define CSPU_H_

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "csp.h"

/* ======================================================================
 * Casper user structures.
 * ====================================================================== */

/* used in runtime load balancing */
typedef enum {
    CSP_MAIN_LOCK_RESET,
    CSP_MAIN_LOCK_OP_ISSUED,
    CSP_MAIN_LOCK_GRANTED
} CSP_main_lock_stat;

typedef enum {
    CSP_TARGET_NO_EPOCH,
    CSP_TARGET_EPOCH_LOCK,
    CSP_TARGET_EPOCH_PSCW
} CSP_target_epoch_stat;

typedef enum {
    CSP_WIN_NO_EPOCH,
    CSP_WIN_EPOCH_FENCE,
    CSP_WIN_EPOCH_LOCK_ALL,
    CSP_WIN_EPOCH_PER_TARGET
} CSP_win_epoch_stat;

typedef enum {
    CSP_WIN_NO_EXP_EPOCH,
    CSP_WIN_EXP_EPOCH_FENCE,
    CSP_WIN_EXP_EPOCH_PSCW
} CSP_win_exp_epoch_stat;

struct CSP_win_info_args {
    unsigned short no_local_load_store;
    int epoch_type;
    CSP_async_config async_config;
    char win_name[MPI_MAX_OBJECT_NAME + 1];
};

typedef struct CSP_op_segment {
    void *origin_addr;
    int origin_count;
    MPI_Datatype origin_datatype;

    int target_rank;
    int target_seg_off;
    MPI_Aint target_disp;
    int target_count;
    int target_dtsize;
    MPI_Datatype target_datatype;

} CSP_op_segment;

typedef struct CSP_win_target_seg {
    MPI_Aint base_offset;
    int size;

    int main_g_off;
    MPI_Win ug_win;

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    CSP_main_lock_stat main_lock_stat;
#endif
} CSP_win_target_seg;

typedef struct CSP_win_target {
    MPI_Win ug_win;             /* Do not free the window, it is freed in ug_wins */
    int disp_unit;
    MPI_Aint size;

    MPI_Aint *base_g_offsets;   /* CSP_ENV.num_g */
    int *g_ranks_in_ug;         /* CSP_ENV.num_g */
    int remote_lock_assert;

    int local_user_rank;        /* rank in local user communicator */
    int local_user_nprocs;
    int world_rank;             /* rank in world communicator */
    int user_world_rank;        /* rank in user world communicator */
    int node_id;

    MPI_Aint wait_counter_offset;       /* counter for complete-wait synchronization. allocated in main ghost. */
    MPI_Aint post_flg_offset;   /* flag for post-start synchronization. allocated in main ghost. */

    /* Only contain 1 segment in rank binding */
    CSP_win_target_seg *segs;
    int num_segs;

    CSP_target_epoch_stat epoch_stat;   /* indicate which access epoch is opened for the target. */

} CSP_win_target;

typedef struct CSP_win {
    /* communicator including local process and ghosts */
    MPI_Comm local_ug_comm;
    MPI_Group local_ug_group;
    MPI_Win local_ug_win;

    int num_g_ranks_in_ug;      /* number of unique ghost ranks */
    int *g_ranks_in_ug;         /* unique ghost ranks in world, used in lockall only epoches. */
    int my_rank_in_ug_comm;     /* remember my rank in internal ug_comm for local RMA. Specified in win_allocate. */
    unsigned short is_self_locked;

    /* communicator including all the user processes and ghosts */
    MPI_Comm ug_comm;
    MPI_Group ug_group;
    MPI_Win *ug_wins;           /* every local process has separate window for permission control,
                                 * processes in different node share one window. */
    int num_ug_wins;            /* = max_local_user_nprocs */

    /* communicator including all the user processes */
    MPI_Comm user_comm;
    MPI_Group user_group;
    MPI_Comm user_root_comm;

    MPI_Comm local_user_comm;
    int max_local_user_nprocs;
    int num_nodes;
    int node_id;

    CSP_win_epoch_stat epoch_stat;      /* indicate which access epoch is opened. Thus operations
                                         * can send to the correct window. Note that only
                                         * change from PER_TARGET to NO_EPOCH when both lock counter
                                         * and start counter are equal to 0, otherwise should check
                                         * per-target epoch status. */
    CSP_win_exp_epoch_stat exp_epoch_stat;      /* indicate which exposure epoch is opened.
                                                 * For now only post-wait/test uses it to avoid duplicate receive.*/
    int lock_counter;
    int start_counter;

    MPI_Win active_win;

    MPI_Group start_group;
    MPI_Group post_group;
    int *start_ranks_in_win_group;
    int *post_ranks_in_win_group;
    MPI_Request *wait_reqs;     /* requests for receiving complete-wait synchronization messages. */

    void *base;
    MPI_Win win;
    CSP_win_target *targets;

    unsigned long *g_win_handles;

#ifdef CSP_ENABLE_GRANT_LOCK_HIDDEN_BYTE
    MPI_Aint grant_lock_g_offset;       /* Hidden byte for granting lock on Ghost0 */
#endif

    struct CSP_win_info_args info_args;

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    int prev_g_off;
    int *g_ops_counts;          /* cnt = g_ops_counts[g_rank_in_ug] */
    unsigned long *g_bytes_counts;      /* byte = g_ops_bytes[g_rank_in_ug] */
#endif

    /* constant flavor attribute to override real flavor when user queries. */
    MPIR_Win_flavor_t create_flavor;

} CSP_win;

/* ======================================================================
 * Window cache related routine.
 * ====================================================================== */

#define CSP_define_win_cache int UG_WIN_HANDLE_KEY = MPI_KEYVAL_INVALID
extern int UG_WIN_HANDLE_KEY;

#define CSP_init_win_cache() {    \
    mpi_errno = PMPI_Win_create_keyval(MPI_WIN_NULL_COPY_FN, \
            MPI_WIN_NULL_DELETE_FN, &UG_WIN_HANDLE_KEY, (void *) 0);    \
    if (mpi_errno != 0) \
        goto fn_fail;   \
}

#define CSP_destroy_win_cache() {    \
    if (UG_WIN_HANDLE_KEY != MPI_KEYVAL_INVALID) {  \
        mpi_errno = PMPI_Win_free_keyval(&UG_WIN_HANDLE_KEY);    \
        if (mpi_errno != MPI_SUCCESS){  \
            CSP_ERR_PRINT("Free UG_WIN_HANDLE_KEY %p\n", &UG_WIN_HANDLE_KEY);   \
        }   /*Do not jump to fn_fail, because it is also used in fn_fail processing */ \
    }   \
}

#define CSP_fetch_ug_win_from_cache(win, ug_win) { \
    int fetch_ug_win_flag = 0;   \
    mpi_errno = PMPI_Win_get_attr(win, UG_WIN_HANDLE_KEY, &ug_win, &fetch_ug_win_flag);   \
    if (!fetch_ug_win_flag || mpi_errno != MPI_SUCCESS){  \
        CSP_DBG_PRINT("Cannot fetch ug_win from win 0x%x\n", win);   \
        ug_win = NULL; \
    }   \
}

#define CSP_cache_ug_win(win, ug_win) { \
    mpi_errno = PMPI_Win_set_attr(win, UG_WIN_HANDLE_KEY, ug_win);  \
    if (mpi_errno != MPI_SUCCESS){  \
        CSP_ERR_PRINT("Cannot cache ug_win %p for win 0x%x\n", ug_win, win);   \
        goto fn_fail;   \
    }   \
    CSP_DBG_PRINT("cache ug_win %p into win 0x%x \n", ug_win, win);  \
}

#define CSP_remove_ug_win_from_cache(win)  {\
    mpi_errno = PMPI_Win_delete_attr(win, UG_WIN_HANDLE_KEY);   \
    if (mpi_errno != MPI_SUCCESS){  \
        CSP_ERR_PRINT("Cannot remove ug_win cache for win 0x%x\n", win);   \
        goto fn_fail;   \
    }   \
}

extern const char *CSP_target_epoch_stat_name[4];       /* for debug */
extern const char *CSP_win_epoch_stat_name[4];

/* Get appropriate window for the target on the current epoch.
 * The epoch status can be per-target (pscw, lock), or global (fence, lockall). */
#define CSP_target_get_epoch_win(seg, target, ug_win, win_ptr) { \
    if (ug_win->epoch_stat == CSP_WIN_EPOCH_PER_TARGET) {    \
        switch (target->epoch_stat) {   \
            case CSP_TARGET_EPOCH_PSCW:    \
                win_ptr = &ug_win->active_win;   \
                break;  \
            case CSP_TARGET_EPOCH_LOCK:    \
                win_ptr = &target->segs[seg].ug_win;   \
                break;  \
            case CSP_TARGET_NO_EPOCH:   \
                win_ptr = NULL; \
                break;  \
        }   \
    } else {    \
        switch (ug_win->epoch_stat) {   \
            case CSP_WIN_EPOCH_FENCE:    \
                win_ptr = &ug_win->active_win;   \
                break;  \
            case CSP_WIN_EPOCH_LOCK_ALL:    \
                if (ug_win->info_args.epoch_type & CSP_EPOCH_LOCK) {  \
                    win_ptr = &target->segs[seg].ug_win;   \
                } else {    \
                    win_ptr = &ug_win->active_win;   \
                }   \
                break;  \
            case CSP_WIN_NO_EPOCH:   \
            case CSP_WIN_EPOCH_PER_TARGET: /* never go here */  \
                win_ptr = NULL; \
                break;  \
        }   \
    }   \
}

/* Check access epoch status per operation.*/
#define CSP_target_check_epoch_per_op(target, ug_win) {   \
    if (ug_win->epoch_stat == CSP_WIN_NO_EPOCH && target->epoch_stat == CSP_TARGET_NO_EPOCH) {  \
        CSP_ERR_PRINT("Wrong synchronization call! "    \
                "No opening epoch in %s\n", __FUNCTION__);  \
        mpi_errno = -1; \
        goto fn_fail;   \
    }   \
}

/* Return name of current epoch status (for debug).*/
static inline const char *CSP_target_get_epoch_stat_name(CSP_win_target * target, CSP_win * ug_win)
{
    if (ug_win->epoch_stat == CSP_WIN_EPOCH_PER_TARGET) {
        return CSP_target_epoch_stat_name[target->epoch_stat];
    }
    else {
        return CSP_win_epoch_stat_name[ug_win->epoch_stat];
    }
}


/* ======================================================================
 * Runtime load balancing related routine.
 * ====================================================================== */

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
#define CSP_reset_target_opload_op_counting(target_rank, ug_win) {  \
        int g_off, g_rank;  \
        for (g_off = 0; g_off < CSP_ENV.num_g; g_off++) {    \
            g_rank = ug_win->targets[target_rank].g_ranks_in_ug[g_off]; \
            ug_win->g_ops_counts[g_rank] = 0;    \
        }   \
        CSP_DBG_PRINT("[load_opt_op] reset target %d op counting \n", target_rank); \
    }

#define CSP_reset_target_opload_bytes_counting(target_rank, ug_win) {  \
        int g_off, g_rank;  \
        for (g_off = 0; g_off < CSP_ENV.num_g; g_off++) {    \
            g_rank = ug_win->targets[target_rank].g_ranks_in_ug[g_off]; \
            ug_win->g_bytes_counts[g_rank] = 0;    \
        }   \
        CSP_DBG_PRINT("[load_opt_byte] reset target %d byte counting \n", target_rank); \
    }

#define CSP_reset_target_opload(target_rank, ug_win) { \
        if (CSP_ENV.load_opt == CSP_LOAD_OPT_COUNTING){ \
            CSP_reset_target_opload_op_counting(target_rank, ug_win) ; \
        } else if (CSP_ENV.load_opt == CSP_LOAD_BYTE_COUNTING){  \
            CSP_reset_target_opload_bytes_counting(target_rank, ug_win) ; \
        }   \
    }


#define CSP_inc_target_opload_op_counting(g_rank_in_ug, ug_win) {  \
        ug_win->g_ops_counts[g_rank_in_ug]++;   \
        CSP_DBG_PRINT("[load_opt_op] increment ghost %d\n", g_rank_in_ug); \
    }

#define CSP_inc_target_opload_bytes_counting(g_rank_in_ug, size, ug_win) {  \
        ug_win->g_bytes_counts[g_rank_in_ug] += size;   \
        CSP_DBG_PRINT("[load_opt_byte] increment ghost %d\n", g_rank_in_ug); \
    }

static inline int CSP_win_grant_lock(int target_rank, int target_seg_off, CSP_win * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int main_g_off = ug_win->targets[target_rank].segs[target_seg_off].main_g_off;

    mpi_errno = PMPI_Win_flush(ug_win->targets[target_rank].g_ranks_in_ug[main_g_off],
                               ug_win->targets[target_rank].segs[target_seg_off].ug_win);
    if (mpi_errno == MPI_SUCCESS) {
        ug_win->targets[target_rank].segs[target_seg_off].main_lock_stat = CSP_MAIN_LOCK_GRANTED;

        CSP_DBG_PRINT("grant lock(Ghost(%d), ug_wins 0x%x) for target %d seg %d\n",
                      ug_win->targets[target_rank].g_ranks_in_ug[main_g_off],
                      ug_win->targets[target_rank].segs[target_seg_off].ug_win,
                      target_rank, target_seg_off);
    }

    return mpi_errno;
}

static inline void CSP_target_get_ghost_opload_by_random(int target_rank, int is_order_required,
                                                         CSP_win * ug_win,
                                                         int *target_g_rank_in_ug,
                                                         int *target_g_rank_idx,
                                                         MPI_Aint * target_g_offset)
{
    /* Randomly change ghost offset every time using a window-level global recorder */
    int idx = (ug_win->prev_g_off + 1) % CSP_ENV.num_g; /* jump to next ghost offset */
    ug_win->prev_g_off = idx;

    *target_g_rank_in_ug = ug_win->targets[target_rank].g_ranks_in_ug[idx];
    *target_g_offset = ug_win->targets[target_rank].base_g_offsets[idx];
    *target_g_rank_idx = idx;

    CSP_DBG_PRINT("[load_opt_random] randomly choose ghost %d, off 0x%lx for target %d\n",
                  *target_g_rank_in_ug, *target_g_offset, target_rank);

}

extern void CSP_target_get_ghost_opload_by_op(int target_rank, int is_order_required,
                                              CSP_win * ug_win, int *target_g_rank_in_ug,
                                              int *target_g_rank_idx, MPI_Aint * target_g_offset);
extern void CSP_target_get_ghost_opload_by_byte(int target_rank, int is_order_required,
                                                int size, CSP_win * ug_win,
                                                int *target_g_rank_in_ug,
                                                int *target_g_rank_idx, MPI_Aint * target_g_offset);

/**
 * Get ghost with dynamic load balancing.
 */
static inline int CSP_target_get_ghost(int target_rank, int target_seg_off,
                                       int is_order_required,
                                       int size, CSP_win * ug_win,
                                       int *target_g_rank_in_ug, MPI_Aint * target_g_offset)
{
    int mpi_errno = MPI_SUCCESS;
    int main_g_off = ug_win->targets[target_rank].segs[target_seg_off].main_g_off;
    int g_idx = 0;

    /* Force lock when the first operation is issued. Note that nocheck epoch
     * does not need it because no conflicting lock.*/
    if (CSP_ENV.load_lock == CSP_LOAD_LOCK_FORCE &&
        !(ug_win->targets[target_rank].remote_lock_assert & MPI_MODE_NOCHECK) &&
        ug_win->targets[target_rank].segs[target_seg_off].main_lock_stat ==
        CSP_MAIN_LOCK_OP_ISSUED) {
        mpi_errno = CSP_win_grant_lock(target_rank, target_seg_off, ug_win);
        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;
    }

    /* Upgrade main lock status of target if it is the first operation of that target. */
    if (ug_win->targets[target_rank].segs[target_seg_off].main_lock_stat == CSP_MAIN_LOCK_RESET) {
        ug_win->targets[target_rank].segs[target_seg_off].main_lock_stat = CSP_MAIN_LOCK_OP_ISSUED;
    }

    /* If lock has not been granted yet, we can only use the main ghost.
     * Accumulate operations have to be always sent to main ghost in order to
     * guarantee atomicity and ordering.*/
    if ((!(ug_win->targets[target_rank].remote_lock_assert & MPI_MODE_NOCHECK) &&
         ug_win->targets[target_rank].segs[target_seg_off].main_lock_stat !=
         CSP_MAIN_LOCK_GRANTED) || is_order_required) {
        /* Both serial async and byte tracking options specify the first ghost as
         * the main ghost of that user process.*/
        *target_g_rank_in_ug = ug_win->targets[target_rank].g_ranks_in_ug[main_g_off];
        *target_g_offset = ug_win->targets[target_rank].base_g_offsets[main_g_off];
        CSP_DBG_PRINT("[load_opt] use main ghost %d, off 0x%lx for target %d "
                      "seg %d (main h off %d)\n",
                      *target_g_rank_in_ug, *target_g_offset, target_rank,
                      target_seg_off, main_g_off);

        /* Need increase counters */
        if (CSP_ENV.load_opt == CSP_LOAD_OPT_COUNTING) {
            CSP_inc_target_opload_op_counting(*target_g_rank_in_ug, ug_win);
        }
        else if (CSP_ENV.load_opt == CSP_LOAD_BYTE_COUNTING) {
            CSP_inc_target_opload_bytes_counting(*target_g_rank_in_ug, size, ug_win);
        }

        return mpi_errno;
    }

    /* Runtime load balancing */
    if (CSP_ENV.load_opt == CSP_LOAD_OPT_RANDOM) {
        CSP_target_get_ghost_opload_by_random(target_rank, is_order_required, ug_win,
                                              target_g_rank_in_ug, &g_idx, target_g_offset);
    }
    else if (CSP_ENV.load_opt == CSP_LOAD_OPT_COUNTING) {
        CSP_target_get_ghost_opload_by_op(target_rank, is_order_required, ug_win,
                                          target_g_rank_in_ug, &g_idx, target_g_offset);
    }
    else if (CSP_ENV.load_opt == CSP_LOAD_BYTE_COUNTING) {
        CSP_target_get_ghost_opload_by_byte(target_rank, is_order_required, size,
                                            ug_win, target_g_rank_in_ug, &g_idx, target_g_offset);
    }

    return mpi_errno;
}
#else
/**
 * Get ghost that is statically bound with the target.
 */
static inline int CSP_target_get_ghost(int target_rank, int target_seg_off, int is_order_required CSP_ATTRIBUTE((unused)),      /* arguments used only in dynamic load */
                                       int size CSP_ATTRIBUTE((unused)), CSP_win * ug_win,
                                       int *target_g_rank_in_ug, MPI_Aint * target_g_offset)
{
    int mpi_errno = MPI_SUCCESS;
    int main_g_off = ug_win->targets[target_rank].segs[target_seg_off].main_g_off;

    *target_g_rank_in_ug = ug_win->targets[target_rank].g_ranks_in_ug[main_g_off];
    *target_g_offset = ug_win->targets[target_rank].base_g_offsets[main_g_off];
    CSP_DBG_PRINT("[opt_non] use main ghost %d, off 0x%lx for target %d seg %d\n",
                  *target_g_rank_in_ug, *target_g_offset, target_rank, target_seg_off);
    return mpi_errno;
}
#endif


/* ======================================================================
 * Other prototypes
 * ====================================================================== */

extern int CSP_cmd_issue(CSP_cmd_pkt_t * pkt);
extern int CSP_op_segments_decode(const void *origin_addr, int origin_count,
                                  MPI_Datatype origin_datatype,
                                  int target_rank, MPI_Aint target_disp,
                                  int target_count, MPI_Datatype target_datatype,
                                  CSP_win * ug_win, CSP_op_segment ** decoded_ops_ptr,
                                  int *num_segs);
extern int CSP_op_segments_decode_basic_datatype(const void *origin_addr, int origin_count,
                                                 MPI_Datatype origin_datatype,
                                                 int target_rank, MPI_Aint target_disp,
                                                 int target_count, MPI_Datatype target_datatype,
                                                 CSP_win * ug_win,
                                                 CSP_op_segment ** decoded_ops_ptr, int *num_segs);
extern void CSP_op_segments_destroy(CSP_op_segment ** decoded_ops_ptr);

extern int CSP_win_bind_ghosts(CSP_win * ug_win);

extern int CSP_win_release(CSP_win * ug_win);

#endif /* CSPU_H_ */

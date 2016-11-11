/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSPU_H_INCLUDED
#define CSPU_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "csp.h"
#include "csp_util.h"
#include "csp_cwp.h"
#include "csp_mlock.h"
#include "cspu_cwp.h"
#include "cspu_thread.h"
#include "cspu_errhan.h"

/* ======================================================================
 * CASPER user constants definition.
 * ====================================================================== */
#define CSPU_PSCW_CW_TAG 900
#define CSPU_PSCW_PS_TAG 901

/* ======================================================================
 * CASPER user structures.
 * ====================================================================== */

/* used in runtime load balancing */
typedef enum {
    CSPU_MAIN_LOCK_RESET,
    CSPU_MAIN_LOCK_OP_ISSUED,
    CSPU_MAIN_LOCK_GRANTED
} CSPU_main_lock_stat_t;

typedef enum {
    CSPU_TARGET_NO_EPOCH,
    CSPU_TARGET_EPOCH_LOCK,
    CSPU_TARGET_EPOCH_PSCW
} CSPU_target_epoch_stat_t;

typedef enum {
    CSPU_WIN_NO_EPOCH,
    CSPU_WIN_EPOCH_FENCE,
    CSPU_WIN_EPOCH_LOCK_ALL,
    CSPU_WIN_EPOCH_PER_TARGET
} CSPU_win_epoch_stat_t;

typedef enum {
    CSPU_WIN_NO_EXP_EPOCH,
    CSPU_WIN_EXP_EPOCH_FENCE,
    CSPU_WIN_EXP_EPOCH_PSCW
} CSPU_win_exp_epoch_stat_t;

typedef struct CSPU_win_info_args {
    unsigned short no_local_load_store;
    int epochs_used;
    CSP_async_config_t async_config;
    char win_name[MPI_MAX_OBJECT_NAME + 1];
} CSPU_win_info_args_t;

typedef struct CSPU_win_target {
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

    int main_g_off;
#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    CSPU_main_lock_stat_t main_lock_stat;
#endif
    CSPU_target_epoch_stat_t epoch_stat;        /* indicate which access epoch is opened for the target. */

} CSPU_win_target_t;

typedef struct CSPU_win {
#if defined(CSP_ENABLE_THREAD_SAFE)
    CSP_thread_cs_t cs;         /* per window critical section object,
                                 * used only when this process is multi-threaded. */
#endif

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

    CSPU_win_epoch_stat_t epoch_stat;   /* indicate which access epoch is opened. Thus operations
                                         * can send to the correct window. Note that only
                                         * change from PER_TARGET to NO_EPOCH when both lock counter
                                         * and start counter are equal to 0, otherwise should check
                                         * per-target epoch status. */
    CSPU_win_exp_epoch_stat_t exp_epoch_stat;   /* indicate which exposure epoch is opened.
                                                 * For now only post-wait/test uses it to avoid duplicate receive.*/
    int lock_counter;
    int start_counter;

    MPI_Win global_win;         /* global window used in active epochs, and lockall epoch in no-lock mode. */

    MPI_Group start_group;
    MPI_Group post_group;
    int *start_ranks_in_win_group;
    int *post_ranks_in_win_group;
    MPI_Request *wait_reqs;     /* requests for receiving complete-wait synchronization messages. */

    void *base;
    MPI_Win win;
    CSPU_win_target_t *targets;

    unsigned long *g_win_handles;

#ifdef CSP_ENABLE_GRANT_LOCK_HIDDEN_BYTE
    MPI_Aint grant_lock_g_offset;       /* Hidden byte for granting lock on Ghost0 */
#endif

    CSPU_win_info_args_t info_args;

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    int prev_g_off;
    int *g_ops_counts;          /* cnt = g_ops_counts[g_rank_in_ug] */
    unsigned long *g_bytes_counts;      /* byte = g_ops_bytes[g_rank_in_ug] */
#endif

    /* constant flavor attribute to override real flavor when user queries. */
    int create_flavor;

} CSPU_win_t;

#ifdef CSP_ENABLE_RMA_ERR_CHECK
/* Check valid target rank in operation and synchronization calls.
 * This check is required because invalid rank can result in segment fault in CASPER. */
#define CSPU_TARGET_CHECK_RANK(target_rank, ug_win) do {                            \
    int user_nprocs = 0;                                                            \
    CSP_CALLMPI(JUMP, PMPI_Comm_size(ug_win->user_comm, &user_nprocs));             \
    if ((target_rank) < MPI_PROC_NULL || (target_rank) >= user_nprocs) {            \
        CSP_msg_print(CSP_MSG_ERROR, "Invalid target rank %d in %s!\n",             \
                      (target_rank), __FUNCTION__);                                 \
        mpi_errno = MPI_ERR_RANK;                                                   \
        goto fn_fail;                                                               \
    }                                                                               \
} while (0)

/* Check access epoch status per operation.
 * This check is required because CASPER changed the synchronization model and
 * consequently MPI implementation cannot correctly detect RMA synchronize error.*/
#define CSPU_TARGET_CHECK_OP_EPOCH(target, ug_win) do {   \
    if (ug_win->epoch_stat == CSPU_WIN_NO_EPOCH && target->epoch_stat == CSPU_TARGET_NO_EPOCH) {  \
        CSP_msg_print(CSP_MSG_ERROR, "Wrong synchronization call! "    \
                      "No opening access epoch in %s\n", __FUNCTION__);       \
        mpi_errno = MPI_ERR_RMA_SYNC;                                  \
        goto fn_fail;                                                  \
    }   \
} while (0)

/* Check RMA operation target displacement.
 * Because we changed it at operation redirection, it would be more user-friendly
 * if we check invalid displacement value here rather than in MPI.*/
#define CSPU_TARGET_CHECK_OP_DISP(target_disp, target) do {                       \
        if (target_disp < 0 || target_disp * target->disp_unit > target->size) {  \
            CSP_msg_print(CSP_MSG_ERROR, "Wrong target displacement(%ld) in %s\n",\
                          target_disp, __FUNCTION__);                             \
            mpi_errno = MPI_ERR_DISP;                                             \
            goto fn_fail;                                                         \
        }   \
    } while (0)
#else
#define CSPU_TARGET_CHECK_RANK(target_rank, ug_win) do {} while (0)
#define CSPU_TARGET_CHECK_OP_EPOCH(target, ug_win) do {} while (0)
#define CSPU_TARGET_CHECK_OP_DISP(target_disp, target) do {} while (0)
#endif

/* ======================================================================
 * Window cache related routines.
 * ====================================================================== */

#define CSP_DEFINE_WIN_CACHE int UG_WIN_HANDLE_KEY = MPI_KEYVAL_INVALID
extern int UG_WIN_HANDLE_KEY;

static inline int CSPU_init_win_cache(void)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_CALLMPI(NOSTMT, PMPI_Win_create_keyval(MPI_WIN_NULL_COPY_FN, MPI_WIN_NULL_DELETE_FN,
                                               &UG_WIN_HANDLE_KEY, (void *) 0));
    return mpi_errno;
}

static inline int CSPU_destroy_win_cache(void)
{
    int mpi_errno = MPI_SUCCESS;
    if (UG_WIN_HANDLE_KEY != MPI_KEYVAL_INVALID) {
        CSP_CALLMPI(NOSTMT, PMPI_Win_free_keyval(&UG_WIN_HANDLE_KEY));
        if (mpi_errno != MPI_SUCCESS)
            CSP_DBG_PRINT("Cannot free UG_WIN_HANDLE_KEY %p\n", &UG_WIN_HANDLE_KEY);
    }
    return mpi_errno;
}

static inline int CSPU_fetch_ug_win_from_cache(MPI_Win win, CSPU_win_t ** ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int fetch_ug_win_flag = 0;

    CSP_CALLMPI(NOSTMT, PMPI_Win_get_attr(win, UG_WIN_HANDLE_KEY, ug_win, &fetch_ug_win_flag));
    if (!fetch_ug_win_flag || mpi_errno != MPI_SUCCESS) {
        CSP_DBG_PRINT("Cannot fetch ug_win from win 0x%x\n", win);
        (*ug_win) = NULL;
    }
    return mpi_errno;
}

static inline int CSPU_cache_ug_win(MPI_Win win, CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_CALLMPI(NOSTMT, PMPI_Win_set_attr(win, UG_WIN_HANDLE_KEY, ug_win));
    if (mpi_errno != MPI_SUCCESS) {
        CSP_DBG_PRINT("Cannot cache ug_win %p for win 0x%x\n", ug_win, win);
        return mpi_errno;
    }
    CSP_DBG_PRINT("cache ug_win %p into win 0x%x \n", ug_win, win);
    return mpi_errno;
}

static inline int CSPU_remove_ug_win_from_cache(MPI_Win win)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_CALLMPI(NOSTMT, PMPI_Win_delete_attr(win, UG_WIN_HANDLE_KEY));
    if (mpi_errno != MPI_SUCCESS)
        CSP_DBG_PRINT("Cannot remove ug_win cache for win 0x%x\n", win);
    return mpi_errno;
}

/* ======================================================================
 * Window related generic routines.
 * ====================================================================== */

extern const char *CSPU_target_epoch_stat_name[4];      /* for debug */
extern const char *CSPU_win_epoch_stat_name[4];

/* Get appropriate window for the target on the current epoch.
 * The epoch status can be per-target (pscw, lock), or global (fence, lockall). */
#define CSPU_TARGET_GET_EPOCH_WIN(target, ug_win, win_ptr) do {  \
    if (ug_win->epoch_stat == CSPU_WIN_EPOCH_PER_TARGET) {    \
        switch (target->epoch_stat) {   \
            case CSPU_TARGET_EPOCH_PSCW:    \
                win_ptr = &ug_win->global_win;   \
                break;  \
            case CSPU_TARGET_EPOCH_LOCK:    \
                win_ptr = &target->ug_win;   \
                break;  \
            case CSPU_TARGET_NO_EPOCH:   \
                win_ptr = NULL; \
                break;  \
        }   \
    } else {    \
        switch (ug_win->epoch_stat) {   \
            case CSPU_WIN_EPOCH_FENCE:    \
                win_ptr = &ug_win->global_win;   \
                break;  \
            case CSPU_WIN_EPOCH_LOCK_ALL:    \
                if (ug_win->info_args.epochs_used & CSP_EPOCH_LOCK) {  \
                    win_ptr = &target->ug_win;   \
                } else {    \
                    win_ptr = &ug_win->global_win;   \
                }   \
                break;  \
            case CSPU_WIN_NO_EPOCH:   \
            case CSPU_WIN_EPOCH_PER_TARGET: /* never go here */  \
                win_ptr = NULL; \
                break;  \
        }   \
    }   \
} while (0)

/* Get name of current epoch status on target (for debug).*/
#define CSPU_TARGET_GET_EPOCH_STAT_NAME(target, ug_win)          \
        ((ug_win->epoch_stat == CSPU_WIN_EPOCH_PER_TARGET) ?     \
            CSPU_target_epoch_stat_name[target->epoch_stat] :    \
            CSPU_win_epoch_stat_name[ug_win->epoch_stat])

/* Get name of current epoch status on window (for debug and error message).
 * PER-TARGET includes PSCW and LOCK.*/
#define CSPU_WIN_GET_EPOCH_STAT_NAME(ug_win) (CSPU_win_epoch_stat_name[ug_win->epoch_stat])

/* ======================================================================
 * Runtime load balancing related routine.
 * ====================================================================== */

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
static inline void CSPU_reset_target_opload_op_counting(int target_rank, CSPU_win_t * ug_win)
{
    int g_off, g_rank;
    for (g_off = 0; g_off < CSP_ENV.num_g; g_off++) {
        g_rank = ug_win->targets[target_rank].g_ranks_in_ug[g_off];
        ug_win->g_ops_counts[g_rank] = 0;
    }
    CSP_DBG_PRINT("[load_opt_op] reset target %d op counting \n", target_rank);
}

static inline void CSPU_reset_target_opload_bytes_counting(int target_rank, CSPU_win_t * ug_win)
{
    int g_off, g_rank;
    for (g_off = 0; g_off < CSP_ENV.num_g; g_off++) {
        g_rank = ug_win->targets[target_rank].g_ranks_in_ug[g_off];
        ug_win->g_bytes_counts[g_rank] = 0;
    }
    CSP_DBG_PRINT("[load_opt_byte] reset target %d byte counting \n", target_rank);
}

static inline void CSPU_reset_target_opload(int target_rank, CSPU_win_t * ug_win)
{
    if (CSP_ENV.load_opt == CSP_LOAD_OPT_COUNTING) {
        CSPU_reset_target_opload_op_counting(target_rank, ug_win);
    }
    else if (CSP_ENV.load_opt == CSP_LOAD_BYTE_COUNTING) {
        CSPU_reset_target_opload_bytes_counting(target_rank, ug_win);
    }
}

static inline void CSPU_inc_target_opload_op_counting(int g_rank_in_ug, CSPU_win_t * ug_win)
{
    ug_win->g_ops_counts[g_rank_in_ug]++;
    CSP_DBG_PRINT("[load_opt_op] increment ghost %d\n", g_rank_in_ug);
}

static inline void CSPU_inc_target_opload_bytes_counting(int g_rank_in_ug, int size,
                                                         CSPU_win_t * ug_win)
{
    ug_win->g_bytes_counts[g_rank_in_ug] += size;
    CSP_DBG_PRINT("[load_opt_byte] increment ghost %d\n", g_rank_in_ug);
}

static inline int CSPU_win_grant_lock(int target_rank, CSPU_win_t * ug_win)
{
    int mpi_errno = MPI_SUCCESS;
    int main_g_off = ug_win->targets[target_rank].main_g_off;

    CSP_CALLMPI(NOSTMT, PMPI_Win_flush(ug_win->targets[target_rank].g_ranks_in_ug[main_g_off],
                                       ug_win->targets[target_rank].ug_win));
    if (mpi_errno == MPI_SUCCESS) {
        ug_win->targets[target_rank].main_lock_stat = CSPU_MAIN_LOCK_GRANTED;

        CSP_DBG_PRINT("grant lock(Ghost(%d), ug_wins 0x%x) for target %d\n",
                      ug_win->targets[target_rank].g_ranks_in_ug[main_g_off],
                      ug_win->targets[target_rank].ug_win, target_rank);
    }

    return mpi_errno;
}

static inline void CSPU_target_get_ghost_opload_by_random(int target_rank, int is_order_required,
                                                          CSPU_win_t * ug_win,
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

extern void CSPU_target_get_ghost_opload_by_op(int target_rank, int is_order_required,
                                               CSPU_win_t * ug_win, int *target_g_rank_in_ug,
                                               int *target_g_rank_idx, MPI_Aint * target_g_offset);
extern void CSPU_target_get_ghost_opload_by_byte(int target_rank, int is_order_required,
                                                 int size, CSPU_win_t * ug_win,
                                                 int *target_g_rank_in_ug,
                                                 int *target_g_rank_idx,
                                                 MPI_Aint * target_g_offset);

/**
 * Get ghost with dynamic load balancing.
 */
static inline int CSPU_target_get_ghost(int target_rank, int is_order_required,
                                        int size, CSPU_win_t * ug_win,
                                        int *target_g_rank_in_ug, MPI_Aint * target_g_offset)
{
    int mpi_errno = MPI_SUCCESS;
    int main_g_off = ug_win->targets[target_rank].main_g_off;
    int g_idx = 0;

    /* Force lock when the first operation is issued. Note that nocheck epoch
     * does not need it because no conflicting lock.*/
    if (CSP_ENV.load_lock == CSP_LOAD_LOCK_FORCE &&
        !(ug_win->targets[target_rank].remote_lock_assert & MPI_MODE_NOCHECK) &&
        ug_win->targets[target_rank].main_lock_stat == CSPU_MAIN_LOCK_OP_ISSUED) {
        mpi_errno = CSPU_win_grant_lock(target_rank, ug_win);
        CSP_CHKMPIFAIL_RETURN(mpi_errno);
    }

    /* Upgrade main lock status of target if it is the first operation of that target. */
    if (ug_win->targets[target_rank].main_lock_stat == CSPU_MAIN_LOCK_RESET) {
        ug_win->targets[target_rank].main_lock_stat = CSPU_MAIN_LOCK_OP_ISSUED;
    }

    /* If lock has not been granted yet, we can only use the main ghost.
     * Accumulate operations have to be always sent to main ghost in order to
     * guarantee atomicity and ordering.*/
    if ((!(ug_win->targets[target_rank].remote_lock_assert & MPI_MODE_NOCHECK) &&
         ug_win->targets[target_rank].main_lock_stat != CSPU_MAIN_LOCK_GRANTED) ||
        is_order_required) {
        /* Both serial async and byte tracking options specify the first ghost as
         * the main ghost of that user process.*/
        *target_g_rank_in_ug = ug_win->targets[target_rank].g_ranks_in_ug[main_g_off];
        *target_g_offset = ug_win->targets[target_rank].base_g_offsets[main_g_off];
        CSP_DBG_PRINT("[load_opt] use main ghost %d, off 0x%lx for target %d "
                      "(main h off %d)\n",
                      *target_g_rank_in_ug, *target_g_offset, target_rank, main_g_off);

        /* Need increase counters */
        if (CSP_ENV.load_opt == CSP_LOAD_OPT_COUNTING) {
            CSPU_inc_target_opload_op_counting(*target_g_rank_in_ug, ug_win);
        }
        else if (CSP_ENV.load_opt == CSP_LOAD_BYTE_COUNTING) {
            CSPU_inc_target_opload_bytes_counting(*target_g_rank_in_ug, size, ug_win);
        }

        return mpi_errno;
    }

    /* Runtime load balancing */
    if (CSP_ENV.load_opt == CSP_LOAD_OPT_RANDOM) {
        CSPU_target_get_ghost_opload_by_random(target_rank, is_order_required, ug_win,
                                               target_g_rank_in_ug, &g_idx, target_g_offset);
    }
    else if (CSP_ENV.load_opt == CSP_LOAD_OPT_COUNTING) {
        CSPU_target_get_ghost_opload_by_op(target_rank, is_order_required, ug_win,
                                           target_g_rank_in_ug, &g_idx, target_g_offset);
    }
    else if (CSP_ENV.load_opt == CSP_LOAD_BYTE_COUNTING) {
        CSPU_target_get_ghost_opload_by_byte(target_rank, is_order_required, size,
                                             ug_win, target_g_rank_in_ug, &g_idx, target_g_offset);
    }

    return mpi_errno;
}
#else
/**
 * Get ghost that is statically bound with the target.
 */
static inline int CSPU_target_get_ghost(int target_rank, int is_order_required CSP_ATTRIBUTE((unused)), /* arguments used only in dynamic load */
                                        int size CSP_ATTRIBUTE((unused)), CSPU_win_t * ug_win,
                                        int *target_g_rank_in_ug, MPI_Aint * target_g_offset)
{
    int mpi_errno = MPI_SUCCESS;
    int main_g_off = ug_win->targets[target_rank].main_g_off;

    *target_g_rank_in_ug = ug_win->targets[target_rank].g_ranks_in_ug[main_g_off];
    *target_g_offset = ug_win->targets[target_rank].base_g_offsets[main_g_off];
    CSP_DBG_PRINT("[opt_non] use main ghost %d, off 0x%lx for target %d\n",
                  *target_g_rank_in_ug, *target_g_offset, target_rank);
    return mpi_errno;
}
#endif

/* ======================================================================
 * Other prototypes
 * ====================================================================== */

extern int CSPU_mlock_acquire(MPI_Comm user_root_comm);
extern int CSPU_mlock_init(void);
extern int CSPU_mlock_destroy(void);

extern int CSPU_win_bind_ghosts(CSPU_win_t * ug_win);
extern int CSPU_win_release(CSPU_win_t * ug_win);

#endif /* CSPU_H_INCLUDED */

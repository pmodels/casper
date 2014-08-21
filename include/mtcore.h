#ifndef MTCORE_H_
#define MTCORE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#define MTCORE_ENABLE_GRANT_LOCK_HIDDEN_BYTE
#define MTCORE_ENABLE_LOCAL_LOCK_OPT    /* Optimization for local target.
                                         * Lock/RMA/Flush/Unlock local target instead of helpers.
                                         * Only available when local lock is granted. */

#ifdef MTCORE_ENABLE_GRANT_LOCK_HIDDEN_BYTE
#define MTCORE_GRANT_LOCK_DATATYPE char
#define MTCORE_GRANT_LOCK_MPI_DATATYPE MPI_CHAR
#endif

/*FIXME: It is a workaround for shared window overlapping problem
 * when shared segment size of each helper is 0 */
#define MTCORE_HELPER_SHARED_SG_SIZE 4096

/* Options for lock permission controlling among multiple helpers.
 *
 * Since RMA Ops to a given target may be distributed to different helpers
 * and locks will be guaranteed to be acquired only when an Op happens,
 * two origins may access a target concurrently if their Ops are distributed
 * to different helpers.
 *
 *  Serial asynchronous:
 *      Statically specify single helper for each target, thus real locks/Ops
 *      to a given target will only be issued to the same helper.
 *
 *  Byte tracking:
 *      Statically specify single helper for each segment of shared memory,
 *      thus real locks/Ops to a given byte will only be issued to the same
 *      helper. Consequently, it eliminates the case that two origins concurrently
 *      access the same address of a target.
 *      This method has additional overhead especially for derived target datatype.
 *      But it is more fine-grained than Serial asynchronous.
 *
 *  Force lock:
 *      Force helper locks granted in lock calls, thus an origin will block in lock
 *      calls while another origin is holding the lock of that target.
 *      After open an epoch using this method, Ops can be safely distributed to
 *      multiple helpers. However, it disables MPI potential optimizations such
 *      as delaying locks and combining lock + Op packets, or ignoring locks with
 *      no operations.
 * */
#define MTCORE_LOCK_OPTION_SERIAL_ASYNC 1
#define MTCORE_LOCK_OPTION_BYTE_TRACK 2
#define MTCORE_LOCK_OPTION_FORCE_LOCK 3

#define MTCORE_LOCK_OPTION MTCORE_LOCK_OPTION_SERIAL_ASYNC


#define MTCORE_LOAD_OPT_NON 0
#define MTCORE_LOAD_OPT_RANDOM 1
#define MTCORE_LOAD_OPT_COUNTING 2
#define MTCORE_LOAD_BYTE_COUNTING 3

#ifdef MTCORE_ENABLE_LOAD_OPT_RANDOM
#define MTCORE_LOAD_OPT MTCORE_LOAD_OPT_RANDOM

#elif defined(MTCORE_ENABLE_LOAD_OPT_COUNTING)
#define MTCORE_LOAD_OPT MTCORE_LOAD_OPT_COUNTING

#elif defined(MTCORE_ENABLE_LOAD_OPT_BYTE_COUNTING)
#define MTCORE_LOAD_OPT MTCORE_LOAD_BYTE_COUNTING

#else

#define MTCORE_LOAD_OPT MTCORE_LOAD_OPT_NON
#endif

#ifdef HAVE_BUILTIN_EXPECT
#  define unlikely(x_) __builtin_expect(!!(x_),0)
#  define likely(x_)   __builtin_expect(!!(x_),1)
#else
#  define unlikely(x_) (x_)
#  define likely(x_)   (x_)
#endif

#ifdef DEBUG
#define MTCORE_DBG_PRINT(str,...) do { \
    fprintf(stdout, "[MTCORE][%d]"str, MTCORE_MY_RANK_IN_WORLD, ## __VA_ARGS__); \
    fflush(stdout); \
    } while (0)
#else
#define MTCORE_DBG_PRINT(str,...) {}
#endif

#define MTCORE_DBG_PRINT_FCNAME() MTCORE_DBG_PRINT("in %s\n", __FUNCTION__)
#define MTCORE_ERR_PRINT(str,...) do { \
    fprintf(stderr, "[%d]"str, MTCORE_MY_RANK_IN_WORLD, ## __VA_ARGS__); \
    fflush(stdout); \
    } while (0)

#define MTCORE_Assert(EXPR) do { if (unlikely(!(EXPR))){ \
            MTCORE_ERR_PRINT("[MTCORE][N-%d, %d]  assert fail in [%s:%d]: \"%s\"\n", \
                    MTCORE_MY_NODE_ID, MTCORE_MY_RANK_IN_WORLD, __FILE__, __LINE__, #EXPR); \
            PMPI_Abort(MPI_COMM_WORLD, -1); \
        }} while (0)

#if (MTCORE_LOAD_OPT != MTCORE_LOAD_OPT_NON)
typedef enum {
    MTCORE_MAIN_LOCK_RESET,
    MTCORE_MAIN_LOCK_OP_ISSUED,
    MTCORE_MAIN_LOCK_GRANTED
} MTCORE_Main_lock_stat;
#endif

typedef enum {
    MTCORE_FUNC_NULL,
    MTCORE_FUNC_WIN_ALLOCATE,
    MTCORE_FUNC_WIN_FREE,
    MTCORE_FUNC_LOCL_ALL,
    MTCORE_FUNC_UNLOCK_ALL,
    MTCORE_FUNC_ABORT,
    MTCORE_FUNC_FINALIZE,
    MTCORE_FUNC_MAX,
} MTCORE_Func;

typedef struct MTCORE_H_win_params {
    MPI_Aint size;
    int disp_unit;
} MTCORE_H_win_params;

struct MTCORE_Win_info_args {
    unsigned short no_local_load_store;
};

typedef struct MTCORE_Win {
    MPI_Aint *base_h_offsets;
    int *disp_units;

    /* communicator including root user processes and all helpers,
     * used for internal information exchange between users and helpers */
    MPI_Comm ur_h_comm;

    /* communicator including local process and helpers */
    MPI_Comm local_uh_comm;
    MPI_Group local_uh_group;
    MPI_Win local_uh_win;
    MTCORE_H_win_params *local_uh_win_param;

    /* communicator including all the user processes and helpers */
    MPI_Comm uh_comm;
    MPI_Group uh_group;
    int *h_ranks_in_uh;         /* user_nprocs * MTCORE_NUM_H */
    MPI_Win *uh_wins;           /* every local process has separate window for permission control,
                                 * processes in different node share one window */

    /* communicator including all the user processes */
    MPI_Comm user_comm;
    MPI_Group user_group;

    MPI_Comm local_user_comm;
    int max_local_user_nprocs;
    int *local_user_ranks;      /* ranks in local user communicator,
                                 * gathered in win_allocate and used in lock_all/flush_all */

    void *base;
    MPI_Win win;

    int num_nodes;
    unsigned long *h_win_handles;

#ifdef MTCORE_ENABLE_GRANT_LOCK_HIDDEN_BYTE
    MPI_Aint grant_lock_h_offset;       /* Hidden byte for granting lock on Helper0 */
#endif

    struct MTCORE_Win_info_args info_args;

#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
    unsigned short is_self_locked;
#endif

#if (MTCORE_LOAD_OPT != MTCORE_LOAD_OPT_NON)
    MTCORE_Main_lock_stat *is_main_lock_granted;
    int *order_h_ranks_in_uh;
#endif

#if (MTCORE_LOAD_OPT == MTCORE_LOAD_OPT_RANDOM)
    int prev_h_off;
#endif

#if (MTCORE_LOAD_OPT == MTCORE_LOAD_OPT_COUNTING)
    int *h_ops_counts;          /* cnt = h_ops_counts[h_rank_in_uh] */
#endif

#if (MTCORE_LOAD_OPT == MTCORE_LOAD_BYTE_COUNTING)
    unsigned long *h_bytes_counts;      /* byte = h_ops_bytes[h_rank_in_uh] */
#endif

} MTCORE_Win;

typedef struct MTCORE_Func_info {
    MTCORE_Func FUNC;
    int user_nprocs;
    int user_local_nprocs;
} MTCORE_Func_info;

#define MTCORE_FUNC_TAG 9889

#define MTCORE_Define_win_cache int UH_WIN_HANDLE_KEY = MPI_KEYVAL_INVALID
extern int UH_WIN_HANDLE_KEY;

#define MTCORE_Init_win_cache() {    \
    mpi_errno = PMPI_Win_create_keyval(MPI_WIN_NULL_COPY_FN, \
            MPI_WIN_NULL_DELETE_FN, &UH_WIN_HANDLE_KEY, (void *) 0);    \
    if (mpi_errno != 0) \
        goto fn_fail;   \
}

#define MTCORE_Destroy_win_cache() {    \
    if (UH_WIN_HANDLE_KEY != MPI_KEYVAL_INVALID) {  \
        mpi_errno = PMPI_Win_free_keyval(&UH_WIN_HANDLE_KEY);    \
        if (mpi_errno != MPI_SUCCESS){  \
            MTCORE_ERR_PRINT("Free UH_WIN_HANDLE_KEY %p\n", &UH_WIN_HANDLE_KEY);   \
        }   /*Do not jump to fn_fail, because it is also used in fn_fail processing */ \
    }   \
}

#define MTCORE_Fetch_uh_win_from_cache(win, uh_win) { \
    int flag = 0;   \
    mpi_errno = PMPI_Win_get_attr(win, UH_WIN_HANDLE_KEY, &uh_win, &flag);   \
    if (!flag || mpi_errno != MPI_SUCCESS){  \
        MTCORE_ERR_PRINT("Cannot fetch uh_win from win 0x%lx\n", win);   \
         goto fn_fail;   \
    }   \
    /* MTCORE_DBG_PRINT("fetch uh_win %p from win 0x%lx \n", uh_win, win);  */ \
}

#define MTCORE_Cache_uh_win(win, uh_win) { \
    mpi_errno = PMPI_Win_set_attr(win, UH_WIN_HANDLE_KEY, uh_win);  \
    if (mpi_errno != MPI_SUCCESS){  \
        MTCORE_ERR_PRINT("Cannot cache uh_win %p for win 0x%lx\n", uh_win, win);   \
        goto fn_fail;   \
    }   \
    MTCORE_DBG_PRINT("cache uh_win %p into win 0x%lx \n", uh_win, win);  \
}

#define MTCORE_Remove_uh_win_from_cache(win)  {\
    mpi_errno = PMPI_Win_delete_attr(win, UH_WIN_HANDLE_KEY);   \
    if (mpi_errno != MPI_SUCCESS){  \
        MTCORE_ERR_PRINT("Cannot remove uh_win cache for win 0x%lx\n", win);   \
        goto fn_fail;   \
    }   \
}

extern MPI_Comm MTCORE_COMM_USER_WORLD;
extern MPI_Comm MTCORE_COMM_LOCAL;
extern MPI_Comm MTCORE_COMM_USER_LOCAL;
extern MPI_Comm MTCORE_COMM_UR_WORLD;
extern MPI_Comm MTCORE_COMM_HELPER_LOCAL;
extern MPI_Group MTCORE_GROUP_WORLD;
extern MPI_Group MTCORE_GROUP_LOCAL;
extern MPI_Group MTCORE_GROUP_USER_WORLD;

extern int MTCORE_NUM_H;
extern int *MTCORE_H_RANKS_IN_WORLD;
extern int *MTCORE_H_RANKS_IN_LOCAL;
extern int *MTCORE_ALL_H_RANKS_IN_WORLD;
extern int *MTCORE_USER_RANKS_IN_WORLD;
extern int MTCORE_NUM_NODES;
extern int MTCORE_MY_NODE_ID;
extern int *MTCORE_ALL_NODE_IDS;
extern int MTCORE_MY_RANK_IN_WORLD;

static inline int MTCORE_Get_node_ids(MPI_Group group, int n, const int ranks[], int node_ids[])
{
    int mpi_errno = MPI_SUCCESS;
    int *ranks_in_world = NULL;
    int i;

    if (n == 0)
        return mpi_errno;

    ranks_in_world = calloc(n, sizeof(int));

    mpi_errno = PMPI_Group_translate_ranks(group, n, ranks, MTCORE_GROUP_WORLD, ranks_in_world);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    for (i = 0; i < n; i++) {
        node_ids[i] = MTCORE_ALL_NODE_IDS[ranks_in_world[i]];
    }

  fn_exit:
    if (ranks_in_world)
        free(ranks_in_world);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

#if (MTCORE_LOAD_OPT != MTCORE_LOAD_OPT_NON)
#define MTCORE_Reset_win_target_ordering(target_rank, uh_win) {  \
        uh_win->order_h_ranks_in_uh[target_rank] = -1; \
    }
#endif

#if (MTCORE_LOAD_OPT == MTCORE_LOAD_OPT_COUNTING)
#define MTCORE_Reset_win_target_load_opt_op_counting(target_rank, uh_win) {  \
        int h_off, h_rank;  \
        for (h_off = 0; h_off < MTCORE_NUM_H; h_off++) {    \
            h_rank = uh_win->h_ranks_in_uh[target_rank * MTCORE_NUM_H + h_off]; \
            uh_win->h_ops_counts[h_rank] = 0;    \
        }   \
        MTCORE_DBG_PRINT("\t reset target %d op counting \n", target_rank); \
    }

#define MTCORE_Reset_win_target_load_opt(target_rank, uh_win) \
        MTCORE_Reset_win_target_load_opt_op_counting(target_rank, uh_win)
#endif

#if (MTCORE_LOAD_OPT == MTCORE_LOAD_BYTE_COUNTING)
#define MTCORE_Reset_win_target_bytes_counting(target_rank, uh_win) {  \
        int h_off, h_rank;  \
        for (h_off = 0; h_off < MTCORE_NUM_H; h_off++) {    \
            h_rank = uh_win->h_ranks_in_uh[target_rank * MTCORE_NUM_H + h_off]; \
            uh_win->h_bytes_counts[h_rank] = 0;    \
        }   \
        MTCORE_DBG_PRINT("\t reset target %d byte counting \n", target_rank); \
    }

#define MTCORE_Reset_win_target_load_opt(target_rank, uh_win) \
        MTCORE_Reset_win_target_bytes_counting(target_rank, uh_win)
#endif

static inline int MTCORE_Is_in_shrd_mem(int target_rank, MPI_Group group, int *node_id,
                                        int *is_shared)
{
    int mpi_errno = MPI_SUCCESS;
    int target_node_id = -1;
    *is_shared = 0;

    /* If target is in the same node, use shared window instead */
    mpi_errno = MTCORE_Get_node_ids(group, 1, &target_rank, &target_node_id);
    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;

    if (target_node_id == MTCORE_ALL_NODE_IDS[MTCORE_MY_RANK_IN_WORLD]) {
        *is_shared = 1;
    }

    *node_id = target_node_id;

    return mpi_errno;
}

static inline int MTCORE_Win_grant_local_lock(int target_rank, int lock_type, int assert,
                                              MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank, local_user_rank;

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);
    PMPI_Comm_rank(uh_win->local_user_comm, &local_user_rank);

#ifdef MTCORE_ENABLE_GRANT_LOCK_HIDDEN_BYTE
    MTCORE_GRANT_LOCK_DATATYPE buf[1];
    mpi_errno = PMPI_Get(buf, 1, MTCORE_GRANT_LOCK_MPI_DATATYPE, target_rank,
                         uh_win->grant_lock_h_offset, 1, MTCORE_GRANT_LOCK_MPI_DATATYPE,
                         uh_win->uh_wins[local_user_rank]);
#else
    /* Simply get 1 byte from start, it does not affect the result of other updates */
    char buf[1];
    mpi_errno = PMPI_Get(buf, 1, MPI_CHAR, target_rank, 0,
                         1, MPI_CHAR, uh_win->uh_wins[local_user_rank]);
#endif
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    mpi_errno = PMPI_Win_flush(target_rank, uh_win->uh_wins[local_user_rank]);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    MTCORE_DBG_PRINT("[%d]grant local lock(Helper(%d), uh_wins[%d])\n", user_rank,
                     target_rank, local_user_rank);
  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

extern int run_h_main(void);

extern int MTCORE_Func_start(MTCORE_Func FUNC, int user_nprocs, int user_local_nprocs);
extern int MTCORE_Func_new_ur_h_comm(MPI_Comm * ur_h_comm);
extern int MTCORE_Func_set_param(char *func_params, int size, MPI_Comm ur_h_comm);

#if (MTCORE_LOAD_OPT == MTCORE_LOAD_OPT_NON)
static inline void MTCORE_Get_helper_rank_load_opt_non(int target_rank, MTCORE_Win * uh_win,
                                                       int *target_h_rank_in_uh)
{
    *target_h_rank_in_uh = uh_win->h_ranks_in_uh[target_rank * MTCORE_NUM_H];
    MTCORE_DBG_PRINT("[opt_non] use main helper %d for target %d\n", *target_h_rank_in_uh,
                     target_rank);
}

#define MTCORE_Get_helper_rank(target_rank, is_order_required, size, uh_win, target_h_rank_in_uh)  \
        MTCORE_Get_helper_rank_load_opt_non(target_rank, uh_win, target_h_rank_in_uh)

#elif (MTCORE_LOAD_OPT == MTCORE_LOAD_OPT_RANDOM)
static inline void MTCORE_Get_helper_rank_load_opt_random(int target_rank, int is_order_required,
                                                          MTCORE_Win * uh_win,
                                                          int *target_h_rank_in_uh)
{
    /* Upgrade main lock status of target if it is the first operation of that target. */
    if (uh_win->is_main_lock_granted[target_rank] == MTCORE_MAIN_LOCK_RESET) {
        uh_win->is_main_lock_granted[target_rank] = MTCORE_MAIN_LOCK_OP_ISSUED;
    }

    /* If lock has not been granted yet, we can only use the main helper. */
    if (uh_win->is_main_lock_granted[target_rank] != MTCORE_MAIN_LOCK_GRANTED) {
        /* Both serial async and byte tracking options specify the first helper as
         * the main helper of that user process.*/
        *target_h_rank_in_uh = uh_win->h_ranks_in_uh[target_rank * MTCORE_NUM_H];

        MTCORE_DBG_PRINT("[opt_random] use main helper %d for target %d\n", *target_h_rank_in_uh,
                         target_rank);
    }
    else {

        /* For ordering required operations, just return the helper chosen in the
         * first time. */
        if (is_order_required && uh_win->order_h_ranks_in_uh[target_rank] != -1) {
            *target_h_rank_in_uh = uh_win->order_h_ranks_in_uh[target_rank];

            MTCORE_DBG_PRINT("[opt_random] use first ordered helper %d for target %d\n",
                             *target_h_rank_in_uh, target_rank);
        }
        else {
            /* Randomly change helper offset every time using a window-level global recorder */
            int off = (uh_win->prev_h_off + 1) % MTCORE_NUM_H;  /* jump to next helper offset */
            uh_win->prev_h_off = off;

            *target_h_rank_in_uh = uh_win->h_ranks_in_uh[target_rank * MTCORE_NUM_H + off];

            /* Remember the helper for ordering required operations to a given target.
             * Note that both not-lock-granted and not-first-ordered targets do not need remember */
            if (is_order_required) {
                uh_win->order_h_ranks_in_uh[target_rank] = *target_h_rank_in_uh;
            }

            MTCORE_DBG_PRINT("[opt_random] randomly choose helper %d for target %d\n",
                             *target_h_rank_in_uh, target_rank);
        }
    }
}

#define MTCORE_Get_helper_rank(target_rank, is_order_required, size, uh_win, target_h_rank_in_uh)  \
    MTCORE_Get_helper_rank_load_opt_random(target_rank, is_order_required, uh_win, target_h_rank_in_uh)

#elif (MTCORE_LOAD_OPT == MTCORE_LOAD_OPT_COUNTING)

extern void MTCORE_Get_helper_rank_load_opt_counting(int target_rank, int is_order_required,
                                                     MTCORE_Win * uh_win, int *target_h_rank_in_uh);

#define MTCORE_Get_helper_rank(target_rank, is_order_required, size, uh_win, target_h_rank_in_uh)  \
    MTCORE_Get_helper_rank_load_opt_counting(target_rank, is_order_required, uh_win, target_h_rank_in_uh)

#elif (MTCORE_LOAD_OPT == MTCORE_LOAD_BYTE_COUNTING)

extern void MTCORE_Get_helper_rank_load_byte_counting(int target_rank, int is_order_required,
                                                      int size, MTCORE_Win * uh_win,
                                                      int *target_h_rank_in_uh);

#define MTCORE_Get_helper_rank(target_rank, is_order_required, size, uh_win, target_h_rank_in_uh)  \
    MTCORE_Get_helper_rank_load_byte_counting(target_rank, is_order_required, size, uh_win, target_h_rank_in_uh)

#endif

#endif /* MTCORE_H_ */

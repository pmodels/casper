/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#ifndef CSP_H_
#define CSP_H_

/* This header file defines generic MACROs/structures/function prototypes used on
 * both ghost side and user side. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <casperconf.h>

/* #define CSP_ENABLE_GRANT_LOCK_HIDDEN_BYTE */

/* #define CSP_ENABLE_LOCAL_RMA_OP_OPT
 *
 * Optimization for RMA operations issued on local target.
 * PUT/GET can be issued to local target instead of remote ghost process, but ACC
 * operations are always issued to the main ghost because of atomicity and ordering
 * issue existing with other remote origin processes. To enable PUT/GET optimization,
 * additional synchronization needs to be also issued on the local process in all
 * synchronization calls . */

/* #define CSP_ENABLE_SYNC_ALL_OPT
 *
 * Optimization that benefits from MPI optimized global synchronization.
 * Some synchronization calls like lock/flush/unlock issued to a target process,
 * must be translated to synchronizations on every ghost process on the target window
 * in default mode.
 * In this optimization, such pre-ghost calls can be changed to single global
 * synchronization call, thus potentially benefiting from optimized global synchronization
 * that can be provided in lower MPI layer.
 * However, user should also note that, if MPI implementation chooses to issue
 * sync message to every target even if it does not receive any operation, this
 * optimization could result in loss of asynchronous progress in synchronization
 * calls. */

/* About optimization for intra-node operations.
 *
 * We do not use force flush + shared window for optimizing operations to intra-node
 * targets. Because: 1) we lose lock optimization on force flush; 2) Although most
 * implementation does shared-communication for operations on shared windows, MPI
 * standard doesn’t require it. Some implementation may use network even for
 * shared targets for shorter CPU occupancy. However, we do not have knowledge of
 * such low-level information. */

#ifdef CSP_ENABLE_GRANT_LOCK_HIDDEN_BYTE
#define CSP_GRANT_LOCK_DATATYPE char
#define CSP_GRANT_LOCK_MPI_DATATYPE MPI_CHAR
#endif

#define CSP_SEGMENT_UNIT 16

#define CSP_PSCW_CW_TAG 900
#define CSP_PSCW_PS_TAG 901

/* Using non-zero segment size as the workaround for shared window
 * overlapping problem. */
#define CSP_GP_SHARED_SG_SIZE 0


/* ======================================================================
 * Generic MACROs and inline functions.
 * ====================================================================== */

#ifndef CSP_unlikely
#ifdef HAVE_BUILTIN_EXPECT
#  define CSP_unlikely(x_) __builtin_expect(!!(x_),0)
#else
#  define CSP_unlikely(x_) (x_)
#endif
#endif /* CSP_unlikely */

#ifndef CSP_ATTRIBUTE
#ifdef HAVE_GCC_ATTRIBUTE
#define CSP_ATTRIBUTE(a_) __attribute__(a_)
#else
#define CSP_ATTRIBUTE(a_)
#endif
#endif /* CSP_ATTRIBUTE */

/* Note that, it is recommended to only pass single variables to the following MACROs.
 * Because these input arguments may be executed twice, thus it is risky to use
 * functions if it updates a global state. */
#ifndef CSP_max
#define CSP_max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef CSP_min
#define CSP_min(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef CSP_align
#define CSP_align(val, align) (((val) + (align) - 1) & ~((align) - 1))
#endif

static inline void *CSP_calloc(int n, size_t size)
{
    void *buf = NULL;
    buf = malloc(n * size);
    if (buf == NULL)
        return buf;

    memset(buf, 0, n * size);
    return buf;
}

/* ======================================================================
 * Casper debugging/info/warning/error MACROs.
 * ====================================================================== */

#ifdef CSP_DEBUG
#define CSP_DBG_PRINT(str,...) do { \
    fprintf(stdout, "[CSP][%d]"str, CSP_PROC.wrank, ## __VA_ARGS__); \
    fflush(stdout); \
    } while (0)
#else
#define CSP_DBG_PRINT(str,...) {}
#endif

/* #define WARN */
#ifdef CSP_WARN
#define CSP_WARN_PRINT(str,...) do { \
    fprintf(stdout, "[CSP][%d]"str, CSP_PROC.wrank, ## __VA_ARGS__); \
    fflush(stdout); \
    } while (0)
#else
#define CSP_WARN_PRINT(str,...) {}
#endif

#define CSP_INFO_PRINT(level, str, ...) do { \
    if (CSP_ENV.verbose > 0 && CSP_ENV.verbose >= level) { \
        fprintf(stdout, str, ## __VA_ARGS__); \
        fflush(stdout); \
    }   \
    } while (0)

#define CSP_DBG_PRINT_FCNAME() CSP_DBG_PRINT("in %s\n", __FUNCTION__)
#define CSP_ERR_PRINT(str,...) do { \
    fprintf(stderr, "[CSP][%d]"str, CSP_PROC.wrank, ## __VA_ARGS__); \
    fflush(stdout); \
    } while (0)

#define CSP_assert(EXPR) do { if (CSP_unlikely(!(EXPR))){ \
            CSP_ERR_PRINT("  assert fail in [%s:%d]: \"%s\"\n", \
                          __FILE__, __LINE__, #EXPR); \
            PMPI_Abort(MPI_COMM_WORLD, -1); \
        }} while (0)


 /* ======================================================================
  * Window related definitions.
  * ====================================================================== */

typedef enum {
    CSP_LOAD_OPT_STATIC,
    CSP_LOAD_OPT_RANDOM,
    CSP_LOAD_OPT_COUNTING,
    CSP_LOAD_BYTE_COUNTING
} CSP_load_opt;

typedef enum {
    CSP_LOAD_LOCK_NATURE,
    CSP_LOAD_LOCK_FORCE
} CSP_load_lock;

typedef enum {
    CSP_LOCK_BINDING_RANK,
    CSP_LOCK_BINDING_SEGMENT
} CSP_lock_binding;

typedef enum {
    CSP_ASYNC_CONFIG_ON = 0,
    CSP_ASYNC_CONFIG_OFF = 1
} CSP_async_config;

typedef enum {
    CSP_EPOCH_LOCK_ALL = 1,
    CSP_EPOCH_LOCK = 2,
    CSP_EPOCH_PSCW = 4,
    CSP_EPOCH_FENCE = 8
} CSP_epoch_type;


/* ======================================================================
 * Command related definition.
 * ====================================================================== */

#define CSP_CMD_TAG 9889        /* tag for command */
#define CSP_CMD_PARAM_TAG 9890  /* tag for any later command parameters */

typedef enum {
    CSP_CMD_UNSET = 0,
    CSP_CMD_FNC_WIN_ALLOCATE,
    CSP_CMD_FNC_WIN_FREE,
    CSP_CMD_FNC_FINALIZE,
    CSP_CMD_LOCK_ACQUIRE,
    CSP_CMD_LOCK_DISCARD,
    CSP_CMD_LOCK_RELEASE,
    CSP_CMD_LOCK_STATUS_SYNC,
    CSP_CMD_MAX
} CSP_cmd_t;

typedef enum {
    CSP_CMD_LOCK_STATUS_UNSET = 0,
    CSP_CMD_LOCK_STATUS_SUSPENDED_L,
    CSP_CMD_LOCK_STATUS_SUSPENDED_H,
    CSP_CMD_LOCK_STATUS_ACQUIRED,
    CSP_CMD_LOCK_STATUS_MAX
} CSP_cmd_lock_status_t;

typedef struct CSP_cmd_winalloc_pkt {
    int user_local_root;
    int user_nprocs;
    int max_local_user_nprocs;
    CSP_epoch_type epochs_used;
    int is_u_world;
    int info_npairs;
} CSP_cmd_fnc_winalloc_pkt_t;

typedef struct CSP_cmd_winfree_pkt {
    int user_local_root;
} CSP_cmd_fnc_winfree_pkt_t;

typedef struct CSP_cmd_lock_acquire_pkt {
    int group_id;               /* global unique id of user communicator. For now, it is safe to use
                                 * world rank of the lowest rank, since all blocking command must be issued
                                 * after all user processes arrived, thus two communicators within the same
                                 * global comm id must always issue command in sequential.*/
} CSP_cmd_lock_acquire_pkt_t;

typedef CSP_cmd_lock_acquire_pkt_t CSP_cmd_lock_discard_pkt_t;
typedef CSP_cmd_lock_acquire_pkt_t CSP_cmd_lock_release_pkt_t;

typedef struct CSP_cmd_lock_status_sync_pkt {
    CSP_cmd_lock_status_t status;
} CSP_cmd_lock_status_sync_pkt_t;

typedef struct CSP_cmd_pkt {
    CSP_cmd_t cmd_type;
    union {
        CSP_cmd_fnc_winalloc_pkt_t fnc_winalloc;
        CSP_cmd_fnc_winfree_pkt_t fnc_winfree;
        CSP_cmd_lock_acquire_pkt_t lock_acquire;
        CSP_cmd_lock_discard_pkt_t lock_discard;
        CSP_cmd_lock_release_pkt_t lock_release;
        CSP_cmd_lock_status_sync_pkt_t lock_status_sync;
    } u;
} CSP_cmd_pkt_t;


/* ======================================================================
 * Environment related definitions.
 * ====================================================================== */

#define CSP_DEFAULT_SEG_SIZE 4096;
#define CSP_DEFAULT_NG 1

typedef struct CSP_env_param {
    int num_g;
    int seg_size;               /* segment size in lock segment binding */
    CSP_load_opt load_opt;      /* runtime load balancing options */
    CSP_load_lock load_lock;    /* how to grant locks for runtime load balancing */

    /* Options for lock permission controlling among multiple ghosts.
     *
     * Since RMA Ops to a given target may be distributed to different ghosts
     * and locks will be guaranteed to be acquired only when an Op happens,
     * two origins may access a target concurrently if their Ops are distributed
     * to different ghosts.
     *
     *  Rank binding:
     *      Statically specify single ghost for each target, thus real locks/Ops
     *      to a given target will only be issued to the same ghost.
     *
     *  Segment binding:
     *      Statically specify single ghost for each segment of shared memory,
     *      thus real locks/Ops to a given byte will only be issued to the same
     *      ghost. This method has additional overhead especially for derived
     *      target datatype, but it is more fine-grained than Rank binding. */
    CSP_lock_binding lock_binding;

    int verbose;                /* verbose level. print configuration information. */
    CSP_async_config async_config;
} CSP_env_param;


/* ======================================================================
 * Process related definitions.
 * ====================================================================== */
typedef enum {
    CSP_PROC_INVALID,
    CSP_PROC_USER,
    CSP_PROC_GHOST
} CSP_proc_type;

typedef struct CSP_user_proc {
    int *g_lranks;
    int *g_wranks_per_user;     /* All user processes' ghost ranks.
                                 * Ghosts of user process x are stored as
                                 * [x*num_g : (x+1)*num_g-1]. */
    int *g_wranks_unique;       /* Unique ghost ranks in the world. */

    MPI_Comm u_local_comm;      /* Includes all users on local node. */
    MPI_Comm ur_comm;           /* Includes the first user(root) on every node. */
} CSP_user_proc;

typedef struct CSP_ghost_proc {
    MPI_Comm g_local_comm;      /* Includes all ghosts on local node. */
    int is_finalized;           /* Flag to notify all ghosts to exit from progress engine.
                                 * It is set to 1 in the finalize handler after all local
                                 * users have arrived at finalize. */
} CSP_ghost_proc;

typedef struct CSP_proc_info {
    /* Common */
    CSP_proc_type proc_type;

    int node_id;
    int num_nodes;
    int wrank;

    MPI_Comm local_comm;        /* Includes all processes on local node. */
    MPI_Group wgroup;

    /* User/Ghost-specific */
    union {
        CSP_user_proc user;
        CSP_ghost_proc ghost;
    };
} CSP_proc;

extern CSP_proc CSP_PROC;

#define CSP_IS_USER (CSP_PROC.proc_type == CSP_PROC_USER)
#define CSP_IS_GHOST (CSP_PROC.proc_type == CSP_PROC_GHOST)

/* Initialize global information objects. */
static inline void CSP_reset_proc(void)
{
    CSP_PROC.proc_type = CSP_PROC_INVALID;
    CSP_PROC.node_id = -1;
    CSP_PROC.num_nodes = 0;
    CSP_PROC.wrank = -1;
    CSP_PROC.wgroup = MPI_GROUP_NULL;
    CSP_PROC.local_comm = MPI_COMM_NULL;
}

/* Initialize user/ghost specific information objects. */
static inline void CSP_reset_typed_proc(void)
{
    if (CSP_IS_USER) {
        CSP_PROC.user.g_lranks = NULL;
        CSP_PROC.user.g_wranks_per_user = NULL;
        CSP_PROC.user.g_wranks_unique = NULL;
        CSP_PROC.user.u_local_comm = MPI_COMM_NULL;
        CSP_PROC.user.ur_comm = MPI_COMM_NULL;
    }
    else {
        CSP_PROC.ghost.g_local_comm = MPI_COMM_NULL;
        CSP_PROC.ghost.is_finalized = 0;
    }
}

/* ======================================================================
 * Global variables and prototypes.
 * ====================================================================== */

extern CSP_env_param CSP_ENV;
extern CSP_proc CSP_PROC;
extern MPI_Comm CSP_COMM_USER_WORLD;

extern int CSPG_init(void);
extern int CSP_init(void);

extern int CSP_setup_proc(void);
extern int CSP_destroy_proc(void);

extern int CSPG_setup_proc(void);
extern int CSPG_destroy_proc(void);

#ifdef CSP_DEBUG
extern void CSP_print_proc(void);
#else
#define CSP_print_proc(void) do {} while (0);
#endif

#ifdef CSPG_DEBUG
extern void CSPG_print_proc(void);
#else
#define CSPG_print_proc(void) do {} while (0);
#endif

#endif /* CSP_H_ */

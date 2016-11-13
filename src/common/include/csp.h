/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#ifndef CSP_H_INCLUDED
#define CSP_H_INCLUDED

/* This header file defines Casper structures/function prototypes used on
 * both ghost side and user side. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <casperconf.h>
#include "csp_util.h"
#include "csp_msg.h"
#include "csp_error.h"
#if defined(CSP_ENABLE_THREAD_SAFE)
#include "csp_thread.h"
#endif

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

/* Using non-zero segment size as the workaround for shared window
 * overlapping problem. */
#define CSP_GP_SHARED_SG_SIZE 0


/* ======================================================================
 * Internal debugging MACRO.
 * ====================================================================== */

#ifdef CSP_DEBUG
#define CSP_DBG_PRINT(str,...) do { \
    fprintf(stdout, "[CSP][%d] "str, CSP_PROC.wrank, ## __VA_ARGS__); \
    fflush(stdout); \
    } while (0)
#else
#define CSP_DBG_PRINT(str,...) do {} while (0)
#endif

#define CSP_ERROR_ABORT(mpi_errno)  do {                                        \
    int errstr_len = 0;                                                         \
    char err_string[MPI_MAX_ERROR_STRING];                                      \
    PMPI_Error_string(mpi_errno, err_string, &errstr_len);                      \
    CSP_msg_print(CSP_MSG_ERROR,                                                \
                  "Rank %d reported Error code %d, %s\n"                        \
                  "--------- Rank %d aborted at %s.\n"                          \
                  ,CSP_PROC.wrank, mpi_errno, err_string                        \
                  ,CSP_PROC.wrank, __FUNCTION__);                               \
    PMPI_Abort(MPI_COMM_WORLD, -1);                                             \
} while (0)
 /* ======================================================================
  * Window related definitions.
  * ====================================================================== */

typedef enum {
    CSP_LOAD_OPT_STATIC,
    CSP_LOAD_OPT_RANDOM,
    CSP_LOAD_OPT_COUNTING,
    CSP_LOAD_BYTE_COUNTING
} CSP_load_opt_t;

typedef enum {
    CSP_LOAD_LOCK_NATURE,
    CSP_LOAD_LOCK_FORCE
} CSP_load_lock_t;

typedef enum {
    CSP_ASYNC_CONFIG_ON = 0,
    CSP_ASYNC_CONFIG_OFF = 1
} CSP_async_config_t;

typedef enum {
    CSP_EPOCH_LOCK_ALL = 1,
    CSP_EPOCH_LOCK = 2,
    CSP_EPOCH_PSCW = 4,
    CSP_EPOCH_FENCE = 8
} CSP_epoch_type_t;


/* ======================================================================
 * Environment related definitions.
 * ====================================================================== */

#define CSP_DEFAULT_NG 1

typedef struct CSP_env_param {
    int num_g;
    CSP_load_opt_t load_opt;    /* runtime load balancing options */
    CSP_load_lock_t load_lock;  /* how to grant locks for runtime load balancing */

    int verbose;                /* verbose level. print configuration information. */
    CSP_async_config_t async_config;
} CSP_env_param_t;


/* ======================================================================
 * Process related definitions.
 * ====================================================================== */
typedef enum {
    CSP_PROC_INVALID,
    CSP_PROC_USER,
    CSP_PROC_GHOST
} CSP_proc_type_t;

typedef struct CSP_user_proc {
    int is_thread_multiple;     /* Set to 1 only when MPI_THREAD_MULTIPLE.
                                 * This enables internal critical sections. */
#if defined(CSP_ENABLE_THREAD_SAFE)
    CSP_thread_cs_t cs;         /* Global critical section object,
                                 * used only when this process is multi-threaded. */
#endif
    int *g_lranks;
    int *g_wranks_per_user;     /* All user processes' ghost ranks.
                                 * Ghosts of user process x are stored as
                                 * [x*num_g : (x+1)*num_g-1]. */
    int *g_wranks_unique;       /* Unique ghost ranks in the world. */

    MPI_Comm u_local_comm;      /* Includes all users on local node. */
    MPI_Comm ur_comm;           /* Includes the first user(root) on every node. */
    MPI_Errhandler comm_errhan_wrapper; /* Errhandler wrapper for any communicator.
                                         * see prototype of CSPU_comm_errhan_wrapper_fnc
                                         * for details.*/
} CSP_user_proc_t;

typedef struct CSP_ghost_proc {
    MPI_Comm g_local_comm;      /* Includes all ghosts on local node. */
} CSP_ghost_proc_t;

typedef struct CSP_proc {
    /* Common */
    CSP_proc_type_t proc_type;

    int node_id;
    int num_nodes;
    int wrank;

    MPI_Comm local_comm;        /* Includes all processes on local node. */
    MPI_Group wgroup;

    /* User/Ghost-specific */
    union {
        CSP_user_proc_t user;
        CSP_ghost_proc_t ghost;
    };
} CSP_proc_t;

extern CSP_proc_t CSP_PROC;

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
    }
}

/* ======================================================================
 * Global variables and prototypes.
 * ====================================================================== */

extern CSP_env_param_t CSP_ENV;
extern CSP_proc_t CSP_PROC;
extern MPI_Comm CSP_COMM_USER_WORLD;
extern MPI_Comm CSP_MPI_COMM_WORLD;

extern int CSPU_global_init(int is_threaded);
extern int CSPG_global_init(void);
extern int CSPG_main(void);

extern int CSPU_global_finalize(void);
extern int CSPG_global_finalize(void);

#endif /* CSP_H_INCLUDED */

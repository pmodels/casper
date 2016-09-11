/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#ifndef CSP_H_
#define CSP_H_

/* This header file defines Casper structures/function prototypes used on
 * both ghost side and user side. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <casperconf.h>
#include "util.h"

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

#define CSP_PSCW_CW_TAG 900
#define CSP_PSCW_PS_TAG 901

/* Using non-zero segment size as the workaround for shared window
 * overlapping problem. */
#define CSP_GP_SHARED_SG_SIZE 0


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
    int *g_lranks;
    int *g_wranks_per_user;     /* All user processes' ghost ranks.
                                 * Ghosts of user process x are stored as
                                 * [x*num_g : (x+1)*num_g-1]. */
    int *g_wranks_unique;       /* Unique ghost ranks in the world. */

    MPI_Comm u_local_comm;      /* Includes all users on local node. */
    MPI_Comm ur_comm;           /* Includes the first user(root) on every node. */
} CSP_user_proc_t;

typedef struct CSP_ghost_proc {
    MPI_Comm g_local_comm;      /* Includes all ghosts on local node. */
    int is_finalized;           /* Flag to notify all ghosts to exit from progress engine.
                                 * It is set to 1 in the finalize handler after all local
                                 * users have arrived at finalize. */
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
        CSP_PROC.ghost.is_finalized = 0;
    }
}

/* ======================================================================
 * Global variables and prototypes.
 * ====================================================================== */

extern CSP_env_param_t CSP_ENV;
extern CSP_proc_t CSP_PROC;
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

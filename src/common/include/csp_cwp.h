/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSP_CWP_H_INCLUDED
#define CSP_CWP_H_INCLUDED

#include "csp.h"
#include "csp_mlock.h"

/* ======================================================================
 * Command wire protocol (CWP) common definition (packet, tags).
 * ====================================================================== */

#define CSP_CWP_TAG 9890        /* tag for command */
#define CSP_CWP_PARAM_TAG 9891  /* tag for any later command parameters */
#define CSP_CWP_MLOCK_DEFAULT_SYNC_TAG 9892     /* default tag for mlock synchronization.
                                                 * To ensure multiple threads can receive
                                                 * the correct sync packet from ghost, we
                                                 * use (TAG + seqno) in threaded case.
                                                 * Both ghost and user code should always call
                                                 * wrapper macro CSP_CWP_MLOCK_SYNC_TAG to get
                                                 * the correct sync_tag. */

#if defined(CSP_ENABLE_THREAD_SAFE)
#define CSP_CWP_MLOCK_SYNC_TAG(gid) (CSP_CWP_MLOCK_DEFAULT_SYNC_TAG + gid.seqno)
#else
#define CSP_CWP_MLOCK_SYNC_TAG(gid) (CSP_CWP_MLOCK_DEFAULT_SYNC_TAG)
#endif

typedef enum {
    CSP_CWP_UNSET = 0,
    CSP_CWP_FNC_WIN_ALLOCATE,
    CSP_CWP_FNC_WIN_FREE,
    CSP_CWP_FNC_FINALIZE,
    CSP_MLOCK_ACQUIRE,
    CSP_MLOCK_DISCARD,
    CSP_MLOCK_RELEASE,
    CSP_MLOCK_STATUS_SYNC,
    CSP_CWP_MAX
} CSP_cwp_t;

typedef struct CSP_cwp_winalloc_pkt {
    int user_local_root;
    int user_nprocs;
    int max_local_user_nprocs;
    int epochs_used;
    int is_u_world;
    int info_npairs;
} CSP_cwp_fnc_winalloc_pkt_t;

typedef struct CSP_cwp_winfree_pkt {
    int user_local_root;
} CSP_cwp_fnc_winfree_pkt_t;

typedef struct CSP_cwp_mlock_acquire_pkt {
    CSP_mlock_gid_t group_id;   /* global unique id of user group. */
} CSP_cwp_mlock_acquire_pkt_t;

typedef CSP_cwp_mlock_acquire_pkt_t CSP_cwp_mlock_discard_pkt_t;
typedef CSP_cwp_mlock_acquire_pkt_t CSP_cwp_mlock_release_pkt_t;

typedef struct CSP_cwp_mlock_status_sync_pkt {
    CSP_mlock_status_t status;
} CSP_cwp_mlock_status_sync_pkt_t;

typedef struct CSP_cwp_pkt {
    CSP_cwp_t cmd_type;
    union {
        CSP_cwp_fnc_winalloc_pkt_t fnc_winalloc;
        CSP_cwp_fnc_winfree_pkt_t fnc_winfree;
        CSP_cwp_mlock_acquire_pkt_t lock_acquire;
        CSP_cwp_mlock_discard_pkt_t lock_discard;
        CSP_cwp_mlock_release_pkt_t lock_release;
        CSP_cwp_mlock_status_sync_pkt_t lock_status_sync;
    } u;
} CSP_cwp_pkt_t;

static inline void CSP_cwp_init_pkt(CSP_cwp_t cmd_type, CSP_cwp_pkt_t * pkt)
{
    memset(pkt, 0, sizeof(CSP_cwp_pkt_t));
    pkt->cmd_type = cmd_type;
}

#endif /* CSP_CWP_H_INCLUDED */

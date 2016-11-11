/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSP_MLOCK_H_INCLUDED
#define CSP_MLOCK_H_INCLUDED

#include <stdio.h>

/* ======================================================================
 * Multi-objects lock (MLOCK) common definition.
 * ====================================================================== */

typedef enum {
    CSP_MLOCK_STATUS_UNSET = 0,
    CSP_MLOCK_STATUS_SUSPENDED_L,
    CSP_MLOCK_STATUS_SUSPENDED_H,
    CSP_MLOCK_STATUS_ACQUIRED,
    CSP_MLOCK_STATUS_MAX
} CSP_mlock_status_t;

typedef struct CSP_mlock_gid {
    int rank;
#if defined(CSP_ENABLE_THREAD_SAFE)
    int seqno;
#endif
} CSP_mlock_gid_t;

#define CSP_MLOCK_GID_MAXLEN 64
static inline void CSP_mlock_gid_to_str(CSP_mlock_gid_t gid, char *str)
{
    memset(str, 0, CSP_MLOCK_GID_MAXLEN);

#if defined(CSP_ENABLE_THREAD_SAFE)
    sprintf(str, "%d/%d", gid.rank, gid.seqno);
#else
    sprintf(str, "%d", gid.rank);
#endif
}

#endif /* CSP_MLOCK_H_INCLUDED */

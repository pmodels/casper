/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSP_MLOCK_H_
#define CSP_MLOCK_H_

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

#endif /* CSP_MLOCK_H_ */

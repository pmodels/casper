/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSP_COMM_H_
#define CSP_COMM_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

typedef enum {
    CSP_COMM_INFO_WD_INVALID = 0,
    CSP_COMM_INFO_WD_ANYSRC = 1,        /* Using any_source and requires status. (default) */
    CSP_COMM_INFO_WD_ANYTAG_NOTAG = 2,  /* Using any_tag or ignoring tag (e.g., all 0). */
    CSP_COMM_INFO_WD_NONE = 4   /* Do not use wildcard. */
} CSP_comm_info_wildcard_t;

typedef enum {
    CSP_COMM_REFER = 0,         /* Reference comm, only creates reference info on user process.
                                 * E.g., MPI_COMM_WORLD. */
    CSP_COMM_SHMBUF = 1,        /* Shared buffer comm, creates only one ug_comm on both user and
                                 * ghost sides to allocate shared buffer. */
    CSP_COMM_ASYNC_DUP = 2,     /* Asynchronous comm, creates all duplicating ug_comms for
                                 * message offloading. */
    CSP_COMM_ASYNC_TAG = 3,     /* ignore_status_src & no_any_tag. Use tag translation
                                 * instead of duplicating.*/
    CSP_COMM_TYPE_MAX
} CSP_comm_type_t;

extern const char *CSP_ug_comm_type_name[CSP_COMM_TYPE_MAX];    /* Debug purpose only. */

#endif /* CSP_COMM_H_ */

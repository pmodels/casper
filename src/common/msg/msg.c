/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "csp_msg.h"

static CSP_msg_verbose_t verbose = CSP_MSG_VBS_UNSET;

void CSP_msg_init(CSP_msg_verbose_t vbs)
{
    verbose = vbs;
}

void CSP_info_print(CSP_msg_verbose_t vbs, const char format[], ...)
{
    va_list list;

    if (verbose > CSP_MSG_VBS_UNSET && verbose >= vbs) {
        va_start(list, format);
        vprintf(format, list);
        va_end(list);
        fflush(stdout);
    }
}

void CSP_err_print(const char format[], ...)
{
    va_list list;

    fprintf(stderr, "[CSP-ERROR]");
    va_start(list, format);
    vfprintf(stderr, format, list);
    va_end(list);
    fflush(stderr);
}

void CSP_warn_print(const char format[], ...)
{
    va_list list;

    if (verbose >= CSP_MSG_VBS_WARN) {
        fprintf(stderr, "[CSP-WARN]");
        va_start(list, format);
        vfprintf(stderr, format, list);
        va_end(list);
        fflush(stderr);
    }
}

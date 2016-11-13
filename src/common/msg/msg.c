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

#define MSG_FPRINTF(file, prefix, list, format) do {    \
        fprintf(file, prefix);                          \
        va_start(list, format);                         \
        vfprintf(file, format, list);                   \
        va_end(list);                                   \
        fflush(file);                                   \
} while (0);

static int verbose = (int) CSP_MSG_OFF;

void CSP_msg_init(int vbs)
{
    verbose = vbs;
}

void CSP_msg_print(CSP_msg_verbose_t vbs, const char format[], ...)
{
    va_list list;

    if (verbose & (int) vbs) {
        switch (vbs) {
        case CSP_MSG_ERROR:
            MSG_FPRINTF(stderr, "[CSP ERR] ", list, format);
            break;
        case CSP_MSG_WARN:
            MSG_FPRINTF(stderr, "[CSP WARN] ", list, format);
            break;
        case CSP_MSG_CONFIG_GLOBAL:
        case CSP_MSG_CONFIG_WIN:
            MSG_FPRINTF(stdout, "", list, format);
            break;
        case CSP_MSG_INFO:
            MSG_FPRINTF(stdout, "[CSP INFO] ", list, format);
            break;
        default:
            break;
        }
    }
}

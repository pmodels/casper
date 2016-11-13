/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSP_MSG_H_INCLUDED
#define CSP_MSG_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ======================================================================
 * Message output (INFO/WARNING/ERROR).
 * User controls message verbosity by setting environment variable.
 * The verbosity can be a combination of following message types:
 * stderr: ERROR, WARN;
 * stdout: CONFIG_GLOBAL, CONFIG_WIN, INFO.
 * ====================================================================== */

typedef enum {
    CSP_MSG_OFF = 0,
    CSP_MSG_ERROR = 1,  /* error */
    CSP_MSG_WARN = 2,   /* warning */
    CSP_MSG_CONFIG_GLOBAL = 4,  /* global configuration */
    CSP_MSG_CONFIG_WIN = 8,     /* window configuration */
    CSP_MSG_INFO = 16   /* any other information */
} CSP_msg_verbose_t;

extern void CSP_msg_init(int vbs);
extern void CSP_msg_print(CSP_msg_verbose_t vbs, const char format[], ...);

#endif /* CSP_MSG_H_INCLUDED */

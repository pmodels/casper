/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSP_MSG_H_
#define CSP_MSG_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ======================================================================
 * Message output (INFO/WARNING/ERROR).
 * Error messages are always printed. Info and warning messages are printed
 * only when user initialized appropriate verbose.
 * ====================================================================== */

typedef enum {
    CSP_MSG_VBS_UNSET = 0,
    CSP_MSG_VBS_INFO_GLOBAL = 1,
    CSP_MSG_VBS_INFO_WIN = 2,
    CSP_MSG_VBS_WARN = 3
} CSP_msg_verbose_t;

extern void CSP_msg_init(CSP_msg_verbose_t vbs);
extern void CSP_info_print(CSP_msg_verbose_t vbs, const char format[], ...);
extern void CSP_warn_print(const char format[], ...);
extern void CSP_err_print(const char format[], ...);

#endif /* CSP_MSG_H_ */

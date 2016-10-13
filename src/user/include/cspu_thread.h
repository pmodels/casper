/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSPU_THREAD_H_
#define CSPU_THREAD_H_

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "csp.h"
#include "csp_util.h"

#if defined(CSP_ENABLE_THREAD_SAFE)
#include "csp_thread.h"

/* Per-object critical section MACROs */
#define CSPU_THREAD_INIT_OBJ_CS(obj_ptr)  do {              \
    if (CSP_PROC.user.is_thread_multiple) {                 \
        if (CSP_thread_cs_init(&(obj_ptr)->cs) != 0) {      \
            mpi_errno = CSP_get_error_code(CSP_ERR_INTERN); \
            goto fn_fail;                                   \
        }                                                   \
    }                                                       \
} while (0)

#define CSPU_THREAD_DESTROY_OBJ_CS(obj_ptr)  do {           \
    if (CSP_PROC.user.is_thread_multiple &&                 \
            CSP_THREAD_CS_IS_INITIALIZED(&(obj_ptr)->cs)) { \
        if (CSP_thread_cs_destroy(&(obj_ptr)->cs) != 0) {   \
            mpi_errno = CSP_get_error_code(CSP_ERR_INTERN); \
            goto fn_fail;                                   \
        }                                                   \
    }                                                       \
} while (0)

#define CSPU_THREAD_OBJ_CS_LOCAL_DCL CSP_THREAD_CS_LOCAL_CTX_DCL

#define CSPU_THREAD_ENTER_OBJ_CS(obj_ptr)  do {                     \
    if (CSP_PROC.user.is_thread_multiple) {                         \
        int obj_cs_enter_err = 0;                                   \
        obj_cs_enter_err = CSP_THREAD_CS_ENTER(&(obj_ptr)->cs);     \
        CSP_ASSERT(obj_cs_enter_err == 0);                          \
    }                                                               \
} while (0)

#define CSPU_THREAD_EXIT_OBJ_CS(obj_ptr)  do {                      \
    if (CSP_PROC.user.is_thread_multiple) {                         \
        int obj_cs_exit_err = 0;                                    \
        obj_cs_exit_err = CSP_THREAD_CS_EXIT(&(obj_ptr)->cs);       \
        CSP_ASSERT(obj_cs_exit_err == 0);                           \
    }                                                               \
} while (0)

#else

#define CSPU_THREAD_INIT_OBJ_CS(obj_ptr)
#define CSPU_THREAD_DESTROY_OBJ_CS(obj_ptr)
#define CSPU_THREAD_OBJ_CS_LOCAL_DCL()
#define CSPU_THREAD_ENTER_OBJ_CS(obj_ptr)
#define CSPU_THREAD_EXIT_OBJ_CS(obj_ptr)
#endif

#endif /* CSPU_THREAD_H_ */

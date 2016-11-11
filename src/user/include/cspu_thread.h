/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSPU_THREAD_H_INCLUDED
#define CSPU_THREAD_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "csp.h"
#include "csp_util.h"

#if defined(CSP_ENABLE_THREAD_SAFE)
#include "csp_thread.h"
#endif

/* ======================================================================
 * Per-object critical section MACROs.
 * ====================================================================== */
#if defined(CSP_ENABLE_THREAD_SAFE)
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
/* undefined CSP_ENABLE_THREAD_SAFE */

#define CSPU_THREAD_INIT_OBJ_CS(obj_ptr)
#define CSPU_THREAD_DESTROY_OBJ_CS(obj_ptr)
#define CSPU_THREAD_OBJ_CS_LOCAL_DCL()
#define CSPU_THREAD_ENTER_OBJ_CS(obj_ptr)
#define CSPU_THREAD_EXIT_OBJ_CS(obj_ptr)
/* End of Per-object critical section MACROs with undefined CSP_ENABLE_THREAD_SAFE */
#endif


/* ======================================================================
 * TLS variable MACROs.
 * ====================================================================== */
#if defined(CSP_ENABLE_THREAD_SAFE)

#define CSPU_TLS_VAR_DCL(type, varname) CSP_TLS_VAR_DCL(type, varname)
#define CSPU_TLS_VAR_INIT(value, varname)   do {                                    \
    int tlsvar_err = 0;                                                             \
    CSP_TLS_VAR_INIT(value, varname, CSP_PROC.user.is_thread_multiple, &tlsvar_err);\
    if (tlsvar_err != 0) {                                                          \
        mpi_errno = CSP_get_error_code(CSP_ERR_INTERN);                             \
        goto fn_fail;                                                               \
    }                                                                               \
} while (0)

#define CSPU_TLS_VAR_DESTROY(varname)   do {                                            \
    int tlsvar_err = 0;                                                                 \
    CSP_TLS_VAR_DESTROY(varname, CSP_PROC.user.is_thread_multiple, &tlsvar_err);        \
    if (tlsvar_err != 0) {                                                              \
        mpi_errno = CSP_get_error_code(CSP_ERR_INTERN);                                 \
        goto fn_fail;                                                                   \
    }                                                                                   \
} while (0)

#define CSPU_TLS_VCOPY_LOCAL_DCL(valtype, cpname)  CSP_TLS_VCOPY_LOCAL_DCL(valtype, cpname)

#define CSPU_TLS_VCOPY_SET(cpname, valtype, val, varname)                     \
    CSP_TLS_VCOPY_SET(cpname, valtype, val, varname, CSP_PROC.user.is_thread_multiple)

#define CSPU_TLS_VCOPY_GET(valtype, varname, valueptr)                        \
    CSP_TLS_VCOPY_GET(valtype, varname, CSP_PROC.user.is_thread_multiple, valueptr)

#define CSPU_TLS_VCOPY_RESET(valtype, val, varname)                           \
    CSP_TLS_VCOPY_RESET(valtype, val, varname, CSP_PROC.user.is_thread_multiple)

#else
/* undefined CSP_ENABLE_THREAD_SAFE */

#define CSPU_TLS_VAR_DCL(type, varname) type varname

#define CSPU_TLS_VAR_INIT(value, varname) do {                            \
        varname = value;                                                  \
} while (0)

#define CSPU_TLS_VAR_DESTROY(varname)
#define CSPU_TLS_VCOPY_LOCAL_DCL(valtype, cpname)

#define CSPU_TLS_VCOPY_SET(cpname, valtype, val, varname) do {   \
    varname = val;                                               \
} while (0)

#define CSPU_TLS_VCOPY_GET(valtype, varname, valptr) do {   \
    *(valptr) = varname;                                    \
} while (0)

#define CSPU_TLS_VCOPY_RESET(valtype, val, varname) do {    \
    varname = val;                                          \
} while (0)

/* End of TLS variable MACROs with undefined CSP_ENABLE_THREAD_SAFE */
#endif

#endif /* CSPU_THREAD_H_INCLUDED */

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#ifndef CSP_UTIL_H_INCLUDED
#define CSP_UTIL_H_INCLUDED

/* This header file defines generic MACROs both ghost side and user side. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <casperconf.h>
#include "info.h"
#include "slist.h"

/* ======================================================================
 * Generic MACROs and inline functions.
 * ====================================================================== */

#ifndef CSP_UNLIKELY
#ifdef HAVE_BUILTIN_EXPECT
#  define CSP_UNLIKELY(x_) __builtin_expect(!!(x_),0)
#else
#  define CSP_UNLIKELY(x_) (x_)
#endif
#endif /* CSP_UNLIKELY */

#ifndef CSP_LIKELY
#ifdef HAVE_BUILTIN_EXPECT
#  define CSP_LIKELY(x_)   __builtin_expect(!!(x_),1)
#else
#  define CSP_LIKELY(x_)   (x_)
#endif
#endif /* CSP_LIKELY */

#ifndef CSP_ATTRIBUTE
#ifdef HAVE_GCC_ATTRIBUTE
#define CSP_ATTRIBUTE(a_) __attribute__(a_)
#else
#define CSP_ATTRIBUTE(a_)
#endif
#endif /* CSP_ATTRIBUTE */

/* Note that, it is recommended to only pass single variables to the following MACROs.
 * Because these input arguments may be executed twice, thus it is risky to use
 * functions if it updates a global state. */
#ifndef CSP_MAX
#define CSP_MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef CSP_MIN
#define CSP_MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef CSP_ALIGN
#define CSP_ALIGN(val, align) (((char*)(val) + (align) - 1) & ~((align) - 1))
#endif

#define CSP_ASSERT(EXPR) do { if (CSP_UNLIKELY(!(EXPR))){           \
            fprintf(stderr, "CSP assert fail in [%s:%d]: \"%s\"\n", \
                          __FILE__, __LINE__, #EXPR);               \
            fflush(stdout);                                         \
            PMPI_Abort(MPI_COMM_WORLD, -1);                         \
        }} while (0)

/* MPI error code checking MARCOs.
 * Can be used by both PMPI calls and other internal calls. */
#define CSP_CHKMPIFAIL_JUMP(mpi_errno) do {           \
            if (mpi_errno != MPI_SUCCESS)             \
                goto fn_fail;                         \
        } while (0)

#define CSP_CHKMPIFAIL_RETURN(mpi_errno) do {         \
            if (mpi_errno != MPI_SUCCESS)             \
                return mpi_errno;                     \
        } while (0)

#define CSP_CHKMPIFAIL_NOSTMT(mpi_errno) do { } while (0)

/*  PMPI call wrapper.
 *  For future PMPI management, PMPI calls should always try to use this wrapper
 *  instead of directly handling the error code. Following fail statement are defined:
 *  - JUMP  : goto fn_fail;
 *  - RETURN: return mpi_errno;
 *  - NOSTMT: do nothing (e.g., if the caller does other thing before jump) */
#define CSP_CALLMPI(fail_stmt, fnc_stmt) do {         \
            mpi_errno = fnc_stmt;                     \
            CSP_CHKMPIFAIL_##fail_stmt(mpi_errno);    \
        } while (0)

/* Special wrapper of PMPI calls at exit or fail routine.
 * Simple abort to avoid complexity.  */
#define CSP_CALLMPI_EXIT(fnc_stmt) do {                \
            int exit_mpi_errno = MPI_SUCCESS;          \
            exit_mpi_errno = fnc_stmt;                 \
            CSP_ASSERT(exit_mpi_errno == MPI_SUCCESS); \
        } while (0)

static inline void *CSP_calloc(int n, size_t size)
{
    void *buf = NULL;
    buf = malloc(n * size);
    if (buf == NULL)
        return buf;

    memset(buf, 0, n * size);
    return buf;
}

/* Wrap UTHASH generic routines before include
 * The callers should always include csp_util.h instead of uthash.h. */
#ifndef uthash_fatal
#define uthash_fatal(msg) do {                                      \
            fprintf(stderr, "%s", msg); fflush(stderr);             \
            CSP_ASSERT(0);                                          \
        } while (0)
#endif
#ifndef uthash_malloc
#define uthash_malloc(sz) CSP_calloc(1, sz)
#endif
#include "uthash.h"

#endif /* CSP_UTIL_H_INCLUDED */

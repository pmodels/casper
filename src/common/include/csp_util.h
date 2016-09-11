/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#ifndef CSP_UTIL_H_
#define CSP_UTIL_H_

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

static inline void *CSP_calloc(int n, size_t size)
{
    void *buf = NULL;
    buf = malloc(n * size);
    if (buf == NULL)
        return buf;

    memset(buf, 0, n * size);
    return buf;
}

#endif /* CSP_UTIL_H_ */

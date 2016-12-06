/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CTEST_H_
#define CTEST_H_

#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <mpi.h>
#include <ctestconf.h>

#if defined(CTEST_ENABLE_THREAD_TEST)
#include <pthread.h>
#endif

/* OS-dependent implementations */

#ifndef CTEST_ATTRIBUTE
#ifdef HAVE_GCC_ATTRIBUTE
#define CTEST_ATTRIBUTE(a_) __attribute__(a_)
#else
#define CTEST_ATTRIBUTE(a_)
#endif
#endif /* CTEST_ATTRIBUTE */


/* ==========================================
 * Generic routines
 * ========================================== */

static inline void CTEST_report_result(int errs)
{
    if (errs == 0) {
        fprintf(stdout, "PASS\n");
        fflush(stdout);
    }
    else {
        fprintf(stdout, "%d errors\n", errs);
        fflush(stdout);
    }
}

/* ==========================================
 * Generic functions for double test data
 * ========================================== */

#define DOUBLE_TOLERANCE (0.00001)

/* Precise double comparison
 *
 * Compare two double variables byte by byte.
 * Only use it when require 100% accuracy. */
static inline int CTEST_precise_double_diff(double a, double b)
{
    int err = 0, i = 0;
    int size = sizeof(double);
    char *c_a, *c_b;

    c_a = (char *) &a, c_b = (char *) &b;
    for (i = 0; i < size; i++) {
        if (c_a[i] != c_b[i]) {
            err = 1;
            break;
        }
    }
    return err;
}

/* Double comparison
 *
 * Compare two double variables with tolerance. */
static inline int CTEST_double_diff(double a, double b)
{
    return (fabs(a - b) > DOUBLE_TOLERANCE) ? 1 : 0;
}


/* Print double buffer
 */
static inline void CTEST_print_double_array(double *buffer, int size, const char *name)
{
    int i;
    int rank = -1;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    fprintf(stderr, "[%d] %s:\n", rank, name);
    for (i = 0; i < size; i++) {
        fprintf(stderr, "%.1lf ", buffer[i]);
    }
    fprintf(stderr, "\n");
}

static inline int CTEST_gen_errrank(void)
{
    int rank;

    /* a negative rank other than PROC_NULL */
    rank = MPI_PROC_NULL - 1;
    while (rank >= 0)
        rank--;

    return rank;
}

#if defined(CTEST_ENABLE_THREAD_TEST)
/* ==========================================
 * Generic functions for threads tests
 * ========================================== */

/* Common argument passed to threads'function. */
typedef struct CTEST_thread_tid_arg {
    int tid;
} CTEST_thread_tid_arg_t;

/* Wrapper for creating pthread */
static inline int CTEST_create_thread(pthread_t * thread, void *(*fn) (void *), void *arg)
{
    int err;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    err = pthread_create(thread, &attr, fn, arg);
    pthread_attr_destroy(&attr);

    return err;
}

/* Atomic variable wrapper */
typedef struct CTEST_atomic_var {
    void *ptr;
    pthread_mutex_t mutex;
} CTEST_atomic_var_t;

#define CTEST_ATOMIC_VAR_INIT(atomic_var, varptr) do {  \
    (atomic_var).ptr = varptr;                          \
    pthread_mutex_init(&(atomic_var).mutex, NULL);      \
} while (0)

#define CTEST_ATOMIC_VAR_DESTROY(atomic_var) pthread_mutex_destroy(&(atomic_var).mutex);

#define CTEST_ATOMIC_VAR_ADD(atomic_var, type, val) do {    \
    pthread_mutex_lock(&(atomic_var).mutex);                \
    *(type *)((atomic_var).ptr) += val;                     \
    pthread_mutex_unlock(&(atomic_var).mutex);              \
} while (0)

#define CTEST_ATOMIC_VAR_SUB(atomic_var, type, val) do {    \
    pthread_mutex_lock(&atomic_var.mutex);                  \
    *((type) *)((atomic_var).ptr) -= val;                   \
    pthread_mutex_unlock(&(atomic_var).mutex);              \
} while (0)

#define CTEST_ATOMIC_VAR_READ(atomic_var, type, val) do {    \
    pthread_mutex_lock(&(atomic_var).mutex);                 \
    (val) = *(type *)((atomic_var).ptr);                     \
    pthread_mutex_unlock(&(atomic_var).mutex);               \
} while (0)

#endif

#endif /* CTEST_H_ */

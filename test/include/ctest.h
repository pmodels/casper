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

/* OS-dependent implementations */

#ifndef CTEST_ATTRIBUTE
#ifdef HAVE_GCC_ATTRIBUTE
#define CTEST_ATTRIBUTE(a_) __attribute__(a_)
#else
#define CTEST_ATTRIBUTE(a_)
#endif
#endif /* CTEST_ATTRIBUTE */

/* Generic functions for double test data */

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


#endif /* CTEST_H_ */

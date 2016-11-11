/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSP_ERROR_H_INCLUDED
#define CSP_ERROR_H_INCLUDED

/* Define CASPER errors that cannot match any MPI predefined error.
 * Any error that matches an MPI predefined error, should always return
 * the predefined one (e.g., internal RMA sync check failure should return
 * MPI_ERR_RMA_SYNC). */
typedef enum {
    CSP_ERR_INTERN = 0,         /* Internal error that is not reported by MPI call.
                                 * Mostly used on ghost side. */
    CSP_ERR_NG, /* Invalid CSP_NG or number of processes.
                 * E.g., mpiexec -np 2 with CSP_NG=2, or CSP_NG < 0.*/
    CSP_ERR_ENV,        /* Invalid environment variable setting. */
    CSP_ERR_MAX /* This is not an error. */
} CSP_error_t;

extern int CSP_get_error_code(CSP_error_t err_type);
extern int CSP_error_init(void);

#endif /* CSP_ERROR_H_INCLUDED */

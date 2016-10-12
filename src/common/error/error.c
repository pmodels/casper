/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <mpi.h>
#include "csp_util.h"
#include "csp_error.h"

static int CSP_error_classes[CSP_ERR_MAX];
static int CSP_error_code[CSP_ERR_MAX];
static const char *CSP_error_string[CSP_ERR_MAX] = {
    /* CSP_ERR_INTERN */ "CASPER internal error.",
    /* CSP_ERR_NG */ "Invalid number of ghost processes (CSP_NG).",
    /* CSP_ERR_ENV */ "Invalid environment variable setting.",
};

int CSP_get_error_code(CSP_error_t err_type)
{
    if (err_type >= CSP_ERR_MAX)
        return MPI_SUCCESS;

    return CSP_error_code[err_type];
}

/* Initialize MPI errors for CASPER internal errors (not MPI internal error).
 * It allows the MPI error handlers to process these errors.*/
int CSP_error_init(void)
{
    int mpi_errno = MPI_SUCCESS;
    int i;

    for (i = 0; i < CSP_ERR_MAX; i++) {
        CSP_CALLMPI(JUMP, PMPI_Add_error_class(&CSP_error_classes[i]));

        CSP_CALLMPI(JUMP, PMPI_Add_error_code(CSP_error_classes[i], &CSP_error_code[i]));

        CSP_CALLMPI(JUMP, PMPI_Add_error_string(CSP_error_classes[i], CSP_error_string[i]));

        CSP_CALLMPI(JUMP, PMPI_Add_error_string(CSP_error_code[i], CSP_error_string[i]));
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

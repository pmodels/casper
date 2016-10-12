/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csp_util.h"
#include "info.h"

#ifdef INFO_DEBUG
#define INFO_DBG_PRINT(str,...) do { \
    fprintf(stdout, "[CSP] %s: "str, __FUNCTION__, ## __VA_ARGS__); \
    fflush(stdout); \
    } while (0)

#else
#define INFO_DBG_PRINT(str,...) do { } while (0)
#endif

/**
 * Deserialize an MPI_Info object to an array of key-value pairs.
 * The array is allocated in this function, but must be released by calling
 * routine.
 */
int CSP_info_deserialize(MPI_Info info, CSP_info_keyval_t ** keyvals, int *npairs)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_info_keyval_t *tmp_keyvals = NULL;
    int i = 0;
    int info_flag = 0;
    int nkeys = 0;

    if (info == MPI_INFO_NULL)
        goto fn_exit;

    CSP_CALLMPI(JUMP, PMPI_Info_get_nkeys(info, &nkeys));

    if (nkeys == 0)
        goto fn_exit;

    tmp_keyvals = CSP_calloc(nkeys, sizeof(CSP_info_keyval_t));
    memset(tmp_keyvals, 0, nkeys * sizeof(CSP_info_keyval_t));

    for (i = 0; i < nkeys; i++) {
        CSP_CALLMPI(JUMP, PMPI_Info_get_nthkey(info, i, tmp_keyvals[i].key));

        CSP_CALLMPI(JUMP, PMPI_Info_get(info, (const char *) tmp_keyvals[i].key,
                                        MPI_MAX_INFO_VAL, tmp_keyvals[i].value, &info_flag));

        INFO_DBG_PRINT("deserialize info:    [%d/%d]%s:%s\n", i, nkeys,
                       tmp_keyvals[i].key, tmp_keyvals[i].value);
    }

  fn_exit:
    *npairs = nkeys;
    (*keyvals) = tmp_keyvals;
    return mpi_errno;

  fn_fail:
    if (tmp_keyvals != NULL)
        free(tmp_keyvals);
    tmp_keyvals = NULL;
    goto fn_exit;
}

/**
 * Serialize an array of key-value pairs to an MPI_Info object.
 * The MPI_Info object is created in this function, but must be released by
 * calling routine.
 */
int CSP_info_serialize(CSP_info_keyval_t * keyvals, int npairs, MPI_Info * info)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Info tmp_info = MPI_INFO_NULL;
    int i = 0;

    if (npairs < 1)
        goto fn_exit;

    CSP_CALLMPI(JUMP, PMPI_Info_create(&tmp_info));

    for (i = 0; i < npairs; i++) {
        CSP_CALLMPI(JUMP, PMPI_Info_set(tmp_info, (const char *) keyvals[i].key,
                                        (const char *) keyvals[i].value));

        INFO_DBG_PRINT("serialize info:    [%d/%d]%s:%s\n", i, npairs,
                       keyvals[i].key, keyvals[i].value);
    }

  fn_exit:
    (*info) = tmp_info;
    return mpi_errno;

  fn_fail:
    if (tmp_info != MPI_INFO_NULL)
        MPI_Info_free(&tmp_info);
    tmp_info = MPI_INFO_NULL;
    goto fn_exit;
}

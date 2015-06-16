/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csp.h"
#include "info.h"

/**
 * Deserialize an MPI_Info object to an array of key-value pairs.
 * The array is allocated in this function, but must be released by calling
 * routine.
 */
int CSP_Info_deserialize(MPI_Info info, CSP_Info_keyval_t ** keyvals, int *npairs)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_Info_keyval_t *_keyvals = NULL;
    int i = 0;
    int info_flag = 0;
    int nkeys = 0;

    if (info == MPI_INFO_NULL)
        goto fn_exit;

    mpi_errno = PMPI_Info_get_nkeys(info, &nkeys);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    if (nkeys == 0)
        goto fn_exit;

    _keyvals = CSP_Calloc(nkeys, sizeof(CSP_Info_keyval_t));
    memset(_keyvals, 0, nkeys * sizeof(CSP_Info_keyval_t));

    for (i = 0; i < nkeys; i++) {
        mpi_errno = PMPI_Info_get_nthkey(info, i, _keyvals[i].key);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        mpi_errno = PMPI_Info_get(info, (const char *) _keyvals[i].key,
                                  MPI_MAX_INFO_VAL, _keyvals[i].value, &info_flag);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        CSP_DBG_PRINT("deserialize info:    [%d/%d]%s:%s\n", i, nkeys,
                      _keyvals[i].key, _keyvals[i].value);
    }

  fn_exit:
    *npairs = nkeys;
    (*keyvals) = _keyvals;
    return mpi_errno;

  fn_fail:
    if (_keyvals != NULL)
        free(_keyvals);
    _keyvals = NULL;
    goto fn_exit;
}

/**
 * Serialize an array of key-value pairs to an MPI_Info object.
 * The MPI_Info object is created in this function, but must be released by
 * calling routine.
 */
int CSP_Info_serialize(CSP_Info_keyval_t * keyvals, int npairs, MPI_Info * info)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Info _info = MPI_INFO_NULL;
    int i = 0;

    if (npairs < 1)
        goto fn_exit;

    mpi_errno = PMPI_Info_create(&_info);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    for (i = 0; i < npairs; i++) {
        mpi_errno = PMPI_Info_set(_info, (const char *) keyvals[i].key,
                                  (const char *) keyvals[i].value);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        CSP_DBG_PRINT("serialize info:    [%d/%d]%s:%s\n", i, npairs,
                      keyvals[i].key, keyvals[i].value);
    }

  fn_exit:
    (*info) = _info;
    return mpi_errno;

  fn_fail:
    if (_info != MPI_INFO_NULL)
        MPI_Info_free(&_info);
    _info = MPI_INFO_NULL;
    goto fn_exit;
}

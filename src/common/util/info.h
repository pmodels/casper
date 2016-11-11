/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef INFO_H_INCLUDED
#define INFO_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

typedef struct CSP_info_keyval {
    char key[MPI_MAX_INFO_KEY];
    char value[MPI_MAX_INFO_VAL];
} CSP_info_keyval_t;


int CSP_info_deserialize(MPI_Info info, CSP_info_keyval_t ** keyvals, int *npairs);
int CSP_info_serialize(CSP_info_keyval_t * keyvals, int npairs, MPI_Info * info);

#endif /* INFO_H_INCLUDED */

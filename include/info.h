/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef INFO_H_
#define INFO_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

typedef struct CSP_Info_keyval {
    char key[MPI_MAX_INFO_KEY];
    char value[MPI_MAX_INFO_VAL];
} CSP_Info_keyval_t;


int CSP_Info_deserialize(MPI_Info info, CSP_Info_keyval_t ** keyvals, int *npairs);
int CSP_Info_serialize(CSP_Info_keyval_t * keyvals, int npairs, MPI_Info * info);

#endif /* INFO_H_ */

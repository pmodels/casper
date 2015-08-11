/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <casper.h>
#include "cspu.h"


/* This file defines Casper external function. */

/* Get the number of ghost processes. */
int CSP_ghost_size(int *ng)
{
    if (ng == NULL)
        return -1;

    (*ng) = CSP_ENV.num_g;
    return 0;
}

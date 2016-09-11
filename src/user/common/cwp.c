/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "csp.h"
#include "cspu.h"

/* Command wire protocol (CWP) component on user processes */

#ifdef CSPU_CWP_DEBUG
#define CSPU_CWP_DBG_PRINT(str,...) do { \
    fprintf(stdout, "[CSPU-CWP][%d]"str, CSP_PROC.wrank, ## __VA_ARGS__); \
    fflush(stdout); \
    } while (0)

#else
#define CSPU_CWP_DBG_PRINT(str,...)
#endif

/* Issue a function command to the local ghost processes (blocking call).
 * It is usually called by only the local user root process, except finalize. */
int CSPU_cwp_fnc_issue(CSP_cwp_pkt_t * pkt)
{
    int mpi_errno = MPI_SUCCESS;

    CSPU_CWP_DBG_PRINT(" send CMD FNC %d to local ghost %d\n", pkt->cmd_type,
                       CSP_PROC.user.g_lranks[0]);
    mpi_errno = PMPI_Send((char *) pkt, sizeof(CSP_cwp_pkt_t), MPI_CHAR,
                          CSP_PROC.user.g_lranks[0], CSP_CWP_TAG, CSP_PROC.local_comm);
    return mpi_errno;
}

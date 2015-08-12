/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "csp.h"
#include "cspu.h"

/* Issue a command to the local ghost processes (blocking call).
 * It is usually called by only the local user root process, except finalize. */
int CSP_cmd_issue(CSP_cmd_pkt_t * pkt)
{
    int mpi_errno = MPI_SUCCESS;

    CSP_DBG_PRINT(" send CMD %d to local ghost %d\n", pkt->cmd, CSP_G_RANKS_IN_LOCAL[0]);

    /* Only send to root ghost to avoid deadlock, then root ghost can bcast to other ghosts. */
    mpi_errno = PMPI_Send((char *) pkt, sizeof(CSP_cmd_pkt_t), MPI_CHAR,
                          CSP_G_RANKS_IN_LOCAL[0], CSP_CMD_TAG, CSP_COMM_LOCAL);
    return mpi_errno;
}

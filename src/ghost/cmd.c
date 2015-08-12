/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspg.h"

/* Receive command from any local user process (blocking call). */
int CSPG_cmd_recv(CSP_cmd_pkt_t * pkt)
{
    int mpi_errno = MPI_SUCCESS;
    int local_gp_rank = 0;

    PMPI_Comm_rank(CSP_COMM_GHOST_LOCAL, &local_gp_rank);
    memset(pkt, 0, sizeof(CSP_cmd_pkt_t));

    /* Only the first ghost receives command from any local user process.
     * Otherwise deadlock may happen if multiple user roots send request to
     * ghosts concurrently and some ghosts are locked in different communicator creation. */
    if (local_gp_rank == 0) {
        mpi_errno = PMPI_Recv((char *) pkt, sizeof(CSP_cmd_pkt_t), MPI_CHAR,
                              MPI_ANY_SOURCE, CSP_CMD_TAG, CSP_COMM_LOCAL, MPI_STATUS_IGNORE);
        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;

        CSPG_DBG_PRINT(" ghost 0 received CMD %d\n", (int) (pkt->cmd));
    }

    if (CSP_ENV.num_g > 1) {
        /* All other ghosts start from here */
        mpi_errno = PMPI_Bcast((char *) pkt, sizeof(CSP_cmd_pkt_t), MPI_CHAR, 0,
                               CSP_COMM_GHOST_LOCAL);
        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;
        CSPG_DBG_PRINT(" all ghosts received CMD %d\n", (int) (pkt->cmd));
    }

    return mpi_errno;
}

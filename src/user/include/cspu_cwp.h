/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSPU_CWP_H_INCLUDED
#define CSPU_CWP_H_INCLUDED

/* Command wire protocol (CWP) component on user processes */

#ifdef CSPU_CWP_DEBUG
#define CSPU_CWP_DBG_PRINT(str,...) do { \
    fprintf(stdout, "[CSPU-CWP][%d]"str, CSP_PROC.wrank, ## __VA_ARGS__); \
    fflush(stdout); \
    } while (0)

#else
#define CSPU_CWP_DBG_PRINT(str,...) do { } while (0)
#endif

/* Issue a command to the local ghost processes (blocking call).
 * It is usually called by only the local user root process, except finalize. */
static inline int CSPU_cwp_issue(CSP_cwp_pkt_t * pkt)
{
    int mpi_errno = MPI_SUCCESS;

    CSPU_CWP_DBG_PRINT(" send CMD %d to local ghost %d\n", pkt->cmd_type,
                       CSP_PROC.user.g_lranks[0]);
    CSP_CALLMPI(RETURN, PMPI_Send((char *) pkt, sizeof(CSP_cwp_pkt_t), MPI_CHAR,
                                  CSP_PROC.user.g_lranks[0], CSP_CWP_TAG, CSP_PROC.local_comm));
    return mpi_errno;
}

#endif /* CSPU_CWP_H_INCLUDED */

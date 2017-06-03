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


/* Broadcast parameters to all the local ghosts (blocking call).
 * It is usually called by only the local user root process after issued the header packet. */
static inline int CSPU_cwp_bcast_params(void *params, size_t size)
{
    int mpi_errno = MPI_SUCCESS;
    int i;
    MPI_Request *reqs = NULL;

    reqs = (MPI_Request *) CSP_calloc(CSP_ENV.num_g, sizeof(MPI_Request));

    /* ghosts are always start from rank 0 on local communicator. */
    for (i = 0; i < CSP_ENV.num_g; i++) {
        CSP_CALLMPI(JUMP, PMPI_Isend(params, size, MPI_CHAR, i, CSP_CWP_PARAM_TAG,
                                     CSP_PROC.local_comm, &reqs[i]));
    }

    CSP_CALLMPI(JUMP, PMPI_Waitall(CSP_ENV.num_g, reqs, MPI_STATUS_IGNORE));

  fn_exit:
    if (reqs)
        free(reqs);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

/* Gather parameters from all the local ghosts (blocking call).
 * It is usually called by only the local user root process after issued the header packet. */
static inline int CSPU_cwp_gather_params(void *params, size_t size)
{
    int mpi_errno = MPI_SUCCESS;
    int i;
    MPI_Request *reqs = NULL;
    size_t offset = 0;

    reqs = (MPI_Request *) CSP_calloc(CSP_ENV.num_g, sizeof(MPI_Request));

    /* ghosts are always start from rank 0 on local communicator. */
    for (i = 0; i < CSP_ENV.num_g; i++) {
        offset = i * size;
        CSP_CALLMPI(JUMP, PMPI_Irecv(((char *) params + offset), size, MPI_CHAR, i,
                                     CSP_CWP_PARAM_TAG, CSP_PROC.local_comm, &reqs[i]));
    }

    CSP_CALLMPI(JUMP, PMPI_Waitall(CSP_ENV.num_g, reqs, MPI_STATUS_IGNORE));

  fn_exit:
    if (reqs)
        free(reqs);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

/* Receive parameters from the specific local ghosts (blocking call).
 * It may be called by any local user process. */
static inline int CSPU_cwp_recv_params(void *params, size_t size, int g_lrank)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_CALLMPI(RETURN, PMPI_Recv(params, size, MPI_CHAR, g_lrank, CSP_CWP_PARAM_TAG,
                                  CSP_PROC.local_comm, MPI_STATUS_IGNORE));
    return mpi_errno;
}

#endif /* CSPU_CWP_H_INCLUDED */

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#ifndef CSPG_H_INCLUDED
#define CSPG_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "csp.h"
#include "csp_cwp.h"
#include "csp_mlock.h"
#include "csp_offload.h"
#include "csp_datatype.h"
#include "csp_comm.h"
#include "cspg_offload.h"

/* ======================================================================
 * Ghost error and internal debugging MACRO.
 * ====================================================================== */

#ifdef CSPG_DEBUG
#define CSPG_DBG_PRINT(str, ...) do {                                                    \
    fprintf(stdout, "[CSPG][%d] "str, CSP_PROC.wrank, ## __VA_ARGS__); \
    fflush(stdout);                                                                      \
    } while (0)
#else
#define CSPG_DBG_PRINT(str, ...) do { } while (0)
#endif

/* ======================================================================
 * Window related definitions.
 * ====================================================================== */

typedef struct CSPG_win_info_args {
    int epochs_used;
} CSPG_win_info_args_t;

typedef struct CSPG_win {
    MPI_Comm local_ug_comm;     /* including local user and ghost processes */
    MPI_Win local_ug_win;

    int max_local_user_nprocs;  /* max number of local processes in this window,
                                 * used for creating lock N-window */
    int user_nprocs;            /* number of user processes in this window */
    int user_local_root;        /* rank of local user root in comm_local. */

    int is_u_world;             /* whether user communicator is equal to USER WORLD. */

    MPI_Comm ug_comm;           /* including all user and ghosts processes */

    void *base;
    MPI_Win *ug_wins;
    int num_ug_wins;

    MPI_Win global_win;

    CSPG_win_info_args_t info_args;
    unsigned long csp_g_win_handle;
} CSPG_win_t;


/* ======================================================================
 * Communicator related definitions.
 * ====================================================================== */

typedef struct CSPG_comm {
    CSP_comm_type_t type;
    MPI_Comm ug_comm;
    MPI_Comm *dup_ug_comms;
    int num_ug_comms;           /* Number of dupcomms on user process. at least 1. */
    int wildcard_info;
    int user_nproc;

    int *g_ranks_bound;         /* Bound ghost rank of every user in ug_comm.
                                 * Indexed by user rank in user comm. User offset can be
                                 * easily got by u_ugrank - g_ugrank.*/
} CSPG_comm_t;

/* ======================================================================
 * CWP related definition (ghost side).
 * ====================================================================== */

typedef int (*CSPG_cwp_root_handler_t) (CSP_cwp_pkt_t * pkt, int user_local_rank);
typedef int (*CSPG_cwp_handler_t) (CSP_cwp_pkt_t * pkt);

extern void CSPG_cwp_register_root_handler(CSP_cwp_t cmd_type, CSPG_cwp_root_handler_t handler_fnc);
extern void CSPG_cwp_register_handler(CSP_cwp_t cmd_type, CSPG_cwp_handler_t handler_fnc);
extern int CSPG_cwp_do_progress(void);
extern void CSPG_cwp_terminate(void);

static inline int CSPG_cwp_try_bcast(CSP_cwp_pkt_t * pkt, MPI_Request * ibcast_req)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_CALLMPI(NOSTMT, PMPI_Ibcast((char *) pkt, sizeof(CSP_cwp_pkt_t), MPI_CHAR, 0,
                                    CSP_PROC.ghost.g_local_comm, ibcast_req));
    return mpi_errno;
}

static inline int CSPG_cwp_root_try_recv(CSP_cwp_pkt_t * pkt, MPI_Request * irecv_req)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_CALLMPI(NOSTMT, PMPI_Irecv((char *) pkt, sizeof(CSP_cwp_pkt_t), MPI_CHAR,
                                   MPI_ANY_SOURCE, CSP_CWP_TAG, CSP_PROC.local_comm, irecv_req));
    return mpi_errno;
}

/* Receive parameters from user root via local communicator (blocking call). */
static inline int CSPG_cwp_recv_param(void *params, size_t size, int user_local_root)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_CALLMPI(NOSTMT, PMPI_Recv(params, size, MPI_CHAR, user_local_root,
                                  CSP_CWP_PARAM_TAG, CSP_PROC.local_comm, MPI_STATUS_IGNORE));
    return mpi_errno;
}

/* Send parameters to any local user via local communicator (blocking call). */
static inline int CSPG_cwp_try_send_param(void *params, size_t size, int user_local_rank,
                                          MPI_Request * isend_req)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_CALLMPI(NOSTMT, PMPI_Isend(params, size, MPI_CHAR, user_local_rank,
                                   CSP_CWP_PARAM_TAG, CSP_PROC.local_comm, isend_req));
    return mpi_errno;
}

/* ======================================================================
 * CWP handler functions.
 * ====================================================================== */

extern int CSPG_win_allocate_cwp_root_handler(CSP_cwp_pkt_t * pkt, int user_local_rank);
extern int CSPG_win_free_cwp_root_handler(CSP_cwp_pkt_t * pkt, int user_local_rank);
extern int CSPG_finalize_cwp_root_handler(CSP_cwp_pkt_t * pkt, int user_local_rank);

extern int CSPG_win_allocate_cwp_handler(CSP_cwp_pkt_t * pkt);
extern int CSPG_win_free_cwp_handler(CSP_cwp_pkt_t * pkt);
extern int CSPG_finalize_cwp_handler(CSP_cwp_pkt_t * pkt);

extern int CSPG_datatype_regist_cwp_handler(CSP_cwp_pkt_t * pkt);

extern int CSPG_ugcomm_create_cwp_root_handler(CSP_cwp_pkt_t * pkt, int user_local_rank);
extern int CSPG_ugcomm_create_cwp_handler(CSP_cwp_pkt_t * pkt);
extern int CSPG_ugcomm_free_cwp_root_handler(CSP_cwp_pkt_t * pkt, int user_local_rank);
extern int CSPG_ugcomm_free_cwp_handler(CSP_cwp_pkt_t * pkt);

extern int CSPG_shmbuf_regist_cwp_root_handler(CSP_cwp_pkt_t * pkt, int user_local_rank);
extern int CSPG_shmbuf_regist_cwp_handler(CSP_cwp_pkt_t * pkt);
extern int CSPG_shmbuf_free_cwp_root_handler(CSP_cwp_pkt_t * pkt, int user_local_rank);
extern int CSPG_shmbuf_free_cwp_handler(CSP_cwp_pkt_t * pkt);

/* ======================================================================
 * MLOCK related definition (ghost side).
 * ====================================================================== */

extern void CSPG_mlock_init(void);
extern void CSPG_mlock_destory(void);
extern int CSPG_mlock_release(void);

/* ======================================================================
 * DATATYPE related definition (ghost side).
 * ====================================================================== */
extern int CSPG_datatype_init(void);
extern int CSPG_datatype_destory(void);
/* See CSPG_datatype_regist_cwp_handler above */

#endif /* CSPG_H_INCLUDED */

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#ifndef CSPG_H_
#define CSPG_H_

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "csp.h"

/* ======================================================================
 * Casper ghost debugging/error/assert MACROs.
 * ====================================================================== */

#ifdef CSPG_DEBUG
#define CSPG_DBG_PRINT(str, ...) do { \
    fprintf(stdout, "[CSPG][%d-%d]"str, CSP_MY_NODE_ID, CSP_MY_RANK_IN_WORLD,   \
            ## __VA_ARGS__); fflush(stdout); \
    } while (0)
#else
#define CSPG_DBG_PRINT(str, ...) {}
#endif

#define CSPG_ERR_PRINT(str,...) do { \
    fprintf(stderr, "[CSPG][%d]"str, CSP_MY_RANK_IN_WORLD, ## __VA_ARGS__); \
    fflush(stdout); \
    } while (0)

#define CSPG_assert(EXPR) do { if (CSP_unlikely(!(EXPR))){ \
            CSPG_ERR_PRINT("  assert fail in [%s:%d]: \"%s\"\n", \
                           __FILE__, __LINE__, #EXPR); \
            PMPI_Abort(MPI_COMM_WORLD, -1); \
        }} while (0)


/* ======================================================================
 * Window related definitions.
 * ====================================================================== */

struct CSPG_win_info_args {
    int epoch_type;
};

typedef struct CSPG_win {
    MPI_Aint *user_base_addrs_in_local;

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

    MPI_Win active_win;

    struct CSPG_win_info_args info_args;
    unsigned long csp_g_win_handle;
} CSPG_win;


/* ======================================================================
 * Command related definition.
 * ====================================================================== */


typedef int (*CSPG_cmd_handler_t) (CSP_cmd_pkt_t * pkt, int *exit_flag);

extern int CSPG_win_allocate(CSP_cmd_pkt_t * pkt, int *exit_flag);
extern int CSPG_win_free(CSP_cmd_pkt_t * pkt, int *exit_flag);
extern int CSPG_finalize(CSP_cmd_pkt_t * pkt, int *exit_flag);
extern int CSPG_cmd_recv(CSP_cmd_pkt_t * pkt);

#endif /* CSPG_H_ */

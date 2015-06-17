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

/* #define CSPG_DEBUG */
#ifdef CSPG_DEBUG
#define CSPG_DBG_PRINT(str, ...) do { \
    fprintf(stdout, "[CSP-H][N-%d]"str, CSP_MY_NODE_ID, ## __VA_ARGS__); fflush(stdout); \
    } while (0)
#else
#define CSPG_DBG_PRINT(str, ...) {}
#endif

#define CSPG_ERR_PRINT(str...) do {fprintf(stderr, str);fflush(stdout);} while (0)

#define CSPG_assert(EXPR) do { if (unlikely(!(EXPR))){ \
            CSPG_ERR_PRINT("[CSP-H][N-%d, %d]  assert fail in [%s:%d]: \"%s\"\n", \
                    CSP_MY_NODE_ID, CSP_MY_RANK_IN_WORLD, __FILE__, __LINE__, #EXPR); \
            PMPI_Abort(MPI_COMM_WORLD, -1); \
        }} while (0)

typedef struct CSPG_win {
    MPI_Aint *user_base_addrs_in_local;

    /* communicator including root user processes and all ghosts,
     * used for internal information exchange between users and ghosts */
    MPI_Comm ur_g_comm;

    /* communicator including local processes and ghosts */
    MPI_Comm local_ug_comm;
    MPI_Win local_ug_win;
    int max_local_user_nprocs;

    /* whether user communicator is equal to USER WORLD. */
    int is_u_world;

    /* communicator including all the user processes and ghosts */
    MPI_Comm ug_comm;

    void *base;
    MPI_Win *ug_wins;
    int num_ug_wins;

    MPI_Win active_win;

    struct CSP_win_info_args info_args;
    unsigned long csp_g_win_handle;
} CSPG_win;

typedef struct CSPG_func_info {
    CSP_func_info info;
    int user_root_in_local;
} CSPG_func_info;

extern int CSPG_win_allocate(int user_local_root, int user_nprocs);
extern int CSPG_win_free(int user_local_root);

extern int CSPG_finalize(void);

extern int CSPG_func_start(CSP_func * FUNC, int *user_local_root, int *user_nprocs,
                           int *user_local_nprocs);
extern int CSPG_func_new_ur_g_comm(int user_local_root, MPI_Comm * ur_g_comm);
extern int CSPG_func_get_param(char *func_params, int size, MPI_Comm ur_g_comm);

#endif /* CSPG_H_ */

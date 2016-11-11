/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspg.h"

#ifdef CSPG_DEBUG
static int dbg_print_proc(void)
{
    int mpi_errno = MPI_SUCCESS;

    int rank, nprocs, local_rank, local_nprocs;
    int local_ghost_rank, local_ghost_nprocs;

    CSP_ASSERT(CSP_IS_GHOST);

    CSP_CALLMPI(RETURN, PMPI_Comm_size(MPI_COMM_WORLD, &nprocs));
    CSP_CALLMPI(RETURN, PMPI_Comm_rank(MPI_COMM_WORLD, &rank));
    CSP_CALLMPI(RETURN, PMPI_Comm_size(CSP_PROC.local_comm, &local_nprocs));
    CSP_CALLMPI(RETURN, PMPI_Comm_rank(CSP_PROC.local_comm, &local_rank));
    CSP_CALLMPI(RETURN, PMPI_Comm_size(CSP_PROC.ghost.g_local_comm, &local_ghost_nprocs));
    CSP_CALLMPI(RETURN, PMPI_Comm_rank(CSP_PROC.ghost.g_local_comm, &local_ghost_rank));

    CSPG_DBG_PRINT("I am ghost: %d/%d in world, %d/%d in local\n"
                   "            %d/%d in ghost local\n"
                   "            node_id %d\n",
                   rank, nprocs, local_rank, local_nprocs,
                   local_ghost_rank, local_ghost_nprocs, CSP_PROC.node_id);

    return mpi_errno;
}
#endif

/* Setup global ghost-specific information. */
static void setup_proc(void)
{
#ifdef CSPG_DEBUG
    dbg_print_proc();
#endif
}

static void register_cwp_handlers(void)
{
    CSPG_cwp_register_root_handler(CSP_CWP_FNC_WIN_ALLOCATE, CSPG_win_allocate_cwp_root_handler);
    CSPG_cwp_register_root_handler(CSP_CWP_FNC_WIN_FREE, CSPG_win_free_cwp_root_handler);
    CSPG_cwp_register_root_handler(CSP_CWP_FNC_FINALIZE, CSPG_finalize_cwp_root_handler);

    CSPG_cwp_register_handler(CSP_CWP_FNC_WIN_ALLOCATE, CSPG_win_allocate_cwp_handler);
    CSPG_cwp_register_handler(CSP_CWP_FNC_WIN_FREE, CSPG_win_free_cwp_handler);
    CSPG_cwp_register_handler(CSP_CWP_FNC_FINALIZE, CSPG_finalize_cwp_handler);
}

int CSPG_global_init(void)
{
    int mpi_errno = MPI_SUCCESS;

    /* Initialization */
    setup_proc();
    register_cwp_handlers();
    CSPG_mlock_init();

    /* Disable MPI automatic error messages. */
    CSP_CALLMPI(JUMP, PMPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN));

  fn_exit:
    return mpi_errno;

  fn_fail:
    /* Do not release global objects, they are released at MPI_Init_thread. */
    goto fn_exit;
}

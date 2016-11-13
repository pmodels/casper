/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspg.h"

static int finalize_cnt = 0;

/* Common internal implementation of finalize handlers.*/
static int finalize_impl(void)
{
    int mpi_errno = MPI_SUCCESS;

    CSPG_DBG_PRINT(" All processes arrived finalize.\n");

    CSPG_global_finalize();

    CSPG_DBG_PRINT(" PMPI_Finalize\n");
    CSP_CALLMPI(JUMP, PMPI_Finalize());

    CSPG_cwp_terminate();

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

/* Destroy global ghost process object */
static int destroy_proc(void)
{
    int mpi_errno = MPI_SUCCESS;

    /* common objects. */
    if (CSP_PROC.local_comm && CSP_PROC.local_comm != MPI_COMM_NULL) {
        CSPG_DBG_PRINT(" free CSP_PROC.local_comm\n");
        CSP_CALLMPI(JUMP, PMPI_Comm_free(&CSP_PROC.local_comm));
    }

    if (CSP_PROC.wgroup && CSP_PROC.wgroup != MPI_GROUP_NULL) {
        CSP_CALLMPI(JUMP, PMPI_Group_free(&CSP_PROC.wgroup));
    }

    CSP_PROC.local_comm = MPI_COMM_NULL;
    CSP_PROC.wgroup = MPI_GROUP_NULL;

    /* ghost-specific objects */
    if (CSP_PROC.ghost.g_local_comm && CSP_PROC.ghost.g_local_comm != MPI_COMM_NULL) {
        CSPG_DBG_PRINT(" free CSP_PROC.ghost.g_local_comm\n");
        CSP_CALLMPI(JUMP, PMPI_Comm_free(&CSP_PROC.ghost.g_local_comm));
    }

    CSP_PROC.ghost.g_local_comm = MPI_COMM_NULL;

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

int CSPG_global_finalize(void)
{
    int mpi_errno = MPI_SUCCESS;

    CSPG_mlock_destory();

    mpi_errno = destroy_proc();

    return mpi_errno;
}

int CSPG_finalize_cwp_root_handler(CSP_cwp_pkt_t * pkt, int user_local_rank CSP_ATTRIBUTE((unused)))
{
    int mpi_errno = MPI_SUCCESS;
    int local_nprocs, local_user_nprocs;

    finalize_cnt++;
    CSP_CALLMPI(JUMP, PMPI_Comm_size(CSP_PROC.local_comm, &local_nprocs));
    local_user_nprocs = local_nprocs - CSP_ENV.num_g;

    CSPG_DBG_PRINT(" %d/%d processes already arrived finalize...\n",
                   finalize_cnt, local_user_nprocs);

    /* wait till all local processes arrive finalize.
     * Because every ghost is shared by multiple local user processes.*/
    if (finalize_cnt < local_user_nprocs)
        goto fn_exit;

    /* broadcast to all local ghost */
    mpi_errno = CSPG_cwp_bcast(pkt);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    mpi_errno = finalize_impl();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    return mpi_errno;

  fn_fail:
    /* Error is handled in CSPG_main. */
    goto fn_exit;
}

int CSPG_finalize_cwp_handler(CSP_cwp_pkt_t * pkt CSP_ATTRIBUTE((unused)))
{
    int mpi_errno = MPI_SUCCESS;

    mpi_errno = finalize_impl();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    return mpi_errno;

  fn_fail:
    /* Error is handled in CSPG_main. */
    goto fn_exit;
}

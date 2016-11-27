/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Dist_graph_create_adjacent(MPI_Comm comm_old,
                                   int indegree, const int sources[],
                                   const int sourceweights[],
                                   int outdegree, const int destinations[],
                                   const int destweights[],
                                   MPI_Info info, int reorder, MPI_Comm * comm_dist_graph)
{
    int mpi_errno = MPI_SUCCESS;

    if (comm_old == MPI_COMM_WORLD)
        comm_old = CSP_COMM_USER_WORLD;

    CSP_CALLMPI(JUMP, PMPI_Dist_graph_create_adjacent(comm_old, indegree, sources,
                                                      sourceweights, outdegree,
                                                      destinations, destweights,
                                                      info, reorder, comm_dist_graph));

    /* Inherit and cache the error handler wrapper for valid new communicator.
     * Note that MPI_COMM_NULL is returned on excluded process. */
    if ((*comm_dist_graph) != MPI_COMM_NULL) {
        mpi_errno = CSPU_comm_errhan_inherit(comm_old, *comm_dist_graph);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

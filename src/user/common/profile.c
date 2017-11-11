/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include "cspu.h"

#ifdef CSP_ENABLE_PROFILE

int CSPU_prof_rma_counters[CSPU_PROF_RMA_MAX_NFUNC * 2];
int CSPU_prof_pt2pt_counters[CSPU_PROF_PT2PT_MAX_NFUNC * 2];

const char *CSPU_prof_rma_func_names[CSPU_PROF_RMA_MAX_NFUNC] = {
    "GET",
    "PUT",
    "ACCUMULATE",
    "GET_ACCUMULATE",
    "RGET",
    "RPUT",
    "RACCUMULATE",
    "RGET_ACCUMULATE",
    "FETCH_AND_OP",
    "COMPARE_AND_SWAP",
};

const char *CSPU_prof_pt2pt_func_names[CSPU_PROF_PT2PT_MAX_NFUNC] = {
    "ISEND",
    "IRECV"
};

void CSPU_prof_init(void)
{
    CSPU_prof_rma_counter_reset();
    CSPU_prof_rma_counter_reset();
}

void CSPU_prof_destroy(void)
{
}

int CSPU_prof_ext_counter_print(int counter, const char *name)
{
    int uwrank, uwnprocs;
    int mpi_errno = MPI_SUCCESS;
    int avg = 0, min = 0, max = 0;

    if (!(CSP_ENV.verbose & CSP_MSG_INFO))
        return mpi_errno;

    CSP_CALLMPI(RETURN, PMPI_Comm_size(CSP_COMM_USER_WORLD, &uwnprocs));
    CSP_CALLMPI(RETURN, PMPI_Comm_rank(CSP_COMM_USER_WORLD, &uwrank));

    CSP_CALLMPI(RETURN, PMPI_Reduce(&counter, &avg, 1, MPI_INT, MPI_SUM, 0, CSP_COMM_USER_WORLD));
    CSP_CALLMPI(RETURN, PMPI_Reduce(&counter, &min, 1, MPI_INT, MPI_MIN, 0, CSP_COMM_USER_WORLD));
    CSP_CALLMPI(RETURN, PMPI_Reduce(&counter, &max, 1, MPI_INT, MPI_MAX, 0, CSP_COMM_USER_WORLD));

    if (uwrank == 0) {
        avg /= uwnprocs;
        CSP_msg_print(CSP_MSG_INFO, "EXT PROFILE: %s %d (min %d max %d)\n", name, avg, min, max);
    }
    return mpi_errno;
}

int CSPU_prof_async_counter_print(void)
{
    int i, uwrank, uwnprocs;
    int mpi_errno = MPI_SUCCESS;

    int rma_avg[CSPU_PROF_RMA_MAX_NFUNC * 2];
    int rma_min[CSPU_PROF_RMA_MAX_NFUNC * 2];
    int rma_max[CSPU_PROF_RMA_MAX_NFUNC * 2];
    int pt2pt_avg[CSPU_PROF_PT2PT_MAX_NFUNC * 2];
    int pt2pt_min[CSPU_PROF_PT2PT_MAX_NFUNC * 2];
    int pt2pt_max[CSPU_PROF_PT2PT_MAX_NFUNC * 2];

    if (!(CSP_ENV.verbose & CSP_MSG_INFO))
        return mpi_errno;

    CSP_CALLMPI(RETURN, PMPI_Comm_size(CSP_COMM_USER_WORLD, &uwnprocs));
    CSP_CALLMPI(RETURN, PMPI_Comm_rank(CSP_COMM_USER_WORLD, &uwrank));

    CSP_CALLMPI(RETURN, PMPI_Reduce(CSPU_prof_rma_counters, rma_avg,
                                    CSPU_PROF_RMA_MAX_NFUNC * 2, MPI_INT, MPI_SUM, 0,
                                    CSP_COMM_USER_WORLD));
    CSP_CALLMPI(RETURN, PMPI_Reduce(CSPU_prof_rma_counters, rma_min,
                                    CSPU_PROF_RMA_MAX_NFUNC * 2, MPI_INT,
                                    MPI_MIN, 0, CSP_COMM_USER_WORLD));
    CSP_CALLMPI(RETURN, PMPI_Reduce(CSPU_prof_rma_counters, rma_max,
                                    CSPU_PROF_RMA_MAX_NFUNC * 2, MPI_INT,
                                    MPI_MAX, 0, CSP_COMM_USER_WORLD));
    CSP_CALLMPI(RETURN, PMPI_Reduce(CSPU_prof_pt2pt_counters, pt2pt_avg,
                                    CSPU_PROF_PT2PT_MAX_NFUNC * 2, MPI_INT,
                                    MPI_SUM, 0, CSP_COMM_USER_WORLD));
    CSP_CALLMPI(RETURN, PMPI_Reduce(CSPU_prof_pt2pt_counters, pt2pt_min,
                                    CSPU_PROF_PT2PT_MAX_NFUNC * 2, MPI_INT,
                                    MPI_MIN, 0, CSP_COMM_USER_WORLD));
    CSP_CALLMPI(RETURN, PMPI_Reduce(CSPU_prof_pt2pt_counters, pt2pt_max,
                                    CSPU_PROF_PT2PT_MAX_NFUNC * 2, MPI_INT,
                                    MPI_MAX, 0, CSP_COMM_USER_WORLD));

    if (uwrank == 0) {
        for (i = 0; i < CSPU_PROF_RMA_MAX_NFUNC * 2; i++)
            rma_avg[i] /= uwnprocs;
        for (i = 0; i < CSPU_PROF_PT2PT_MAX_NFUNC * 2; i++)
            pt2pt_avg[i] /= uwnprocs;

        for (i = 0; i < CSPU_PROF_RMA_MAX_NFUNC; i++) {
            if (rma_avg[2 * i + CSPU_PROF_COUNTER_OFFSET_ON] == 0 &&
                rma_avg[2 * i + CSPU_PROF_COUNTER_OFFSET_OFF] == 0)
                continue;
            CSP_msg_print(CSP_MSG_INFO, "RMA PROFILE: issued %s to user %d (min %d max %d), "
                          "to ghost %d (min %d max %d)\n", CSPU_prof_rma_func_names[i],
                          rma_avg[2 * i + CSPU_PROF_COUNTER_OFFSET_OFF],
                          rma_min[2 * i + CSPU_PROF_COUNTER_OFFSET_OFF],
                          rma_max[2 * i + CSPU_PROF_COUNTER_OFFSET_OFF],
                          rma_avg[2 * i + CSPU_PROF_COUNTER_OFFSET_ON],
                          rma_min[2 * i + CSPU_PROF_COUNTER_OFFSET_ON],
                          rma_max[2 * i + CSPU_PROF_COUNTER_OFFSET_ON]);
        }

        for (i = 0; i < CSPU_PROF_PT2PT_MAX_NFUNC; i++) {
            if (pt2pt_avg[2 * i + CSPU_PROF_COUNTER_OFFSET_ON] == 0 &&
                pt2pt_avg[2 * i + CSPU_PROF_COUNTER_OFFSET_OFF] == 0)
                continue;
            CSP_msg_print(CSP_MSG_INFO, "PT2PT PROFILE: issued %s from user %d (min %d max %d), "
                          "from ghost %d (min %d max %d)\n", CSPU_prof_pt2pt_func_names[i],
                          pt2pt_avg[2 * i + CSPU_PROF_COUNTER_OFFSET_OFF],
                          pt2pt_min[2 * i + CSPU_PROF_COUNTER_OFFSET_OFF],
                          pt2pt_max[2 * i + CSPU_PROF_COUNTER_OFFSET_OFF],
                          pt2pt_avg[2 * i + CSPU_PROF_COUNTER_OFFSET_ON],
                          pt2pt_min[2 * i + CSPU_PROF_COUNTER_OFFSET_ON],
                          pt2pt_max[2 * i + CSPU_PROF_COUNTER_OFFSET_ON]);
        }
    }
    return mpi_errno;
}

#endif

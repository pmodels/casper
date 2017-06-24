/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSPU_PT2PT_H_INCLUDED
#define CSPU_PT2PT_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "csp.h"

typedef struct CSPU_comm_info_args {
    CSP_async_config_t pt2pt_async_config;      /* Internal info.
                                                 * Updated by the following user hints. */

    /* ignore_status_src && no_any_tag: tag_trans(recv_offset);
     * ignore_status_src: duplicate comm;
     * no_any_src_spec_tag: duplicate comm + trans(src_offset) (minimal). */

    unsigned short ignore_status_src;   /* Ignore stat.SOURCE. */
    unsigned short no_any_src_spec_tag; /* No ANY_SOURCE + specific TAG. */
    unsigned short no_any_tag;
} CSPU_comm_info_args_t;

typedef struct CSPU_comm {
#if defined(CSP_ENABLE_THREAD_SAFE)
    CSP_thread_cs_t cs;         /* per window critical section object,
                                 * used only when this process is multi-threaded. */
#endif

    MPI_Comm ug_comm;           /* Including both user and ghost processes */
    MPI_Comm comm;              /* Including all user processes, exposed to user. */
    MPI_Comm user_root_comm;    /* Used to acquire mlock. */
    MPI_Comm local_user_comm;   /* Used to cwp with ghost */

    CSPU_comm_info_args_t info_args;    /* Store real info controlling implementation. */
    CSPU_comm_info_args_t ref_info_args;        /* Store info passed by comm_set_info,
                                                 * transfer to impl_info at child comm creation.*/
    int num_ghosts_unique;

    int *g_ranks_bound;         /* Bound ghost rank of each user in ug_comm.
                                 * Ghost is already locally bound at MPI_init. */

    MPI_Aint g_ugcomm_bound;    /* ug_comm address on the bound ghost process */
    MPI_Aint *g_ugcomm_handles; /* ug_comm address on every ghost process.
                                 * Only used by local user root.*/
} CSPU_comm_t;

/* ======================================================================
 * Communicator cache related routines.
 * ====================================================================== */

#define CSP_DEFINE_COMM_CACHE int UG_COMM_HANDLE_KEY = MPI_KEYVAL_INVALID
extern int UG_COMM_HANDLE_KEY;

static inline int CSPU_init_comm_cache(void)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_CALLMPI(NOSTMT, PMPI_Comm_create_keyval(MPI_COMM_NULL_COPY_FN, MPI_COMM_NULL_DELETE_FN,
                                                &UG_COMM_HANDLE_KEY, (void *) 0));
    return mpi_errno;
}

static inline int CSPU_destroy_comm_cache(void)
{
    int mpi_errno = MPI_SUCCESS;
    if (UG_COMM_HANDLE_KEY != MPI_KEYVAL_INVALID) {
        CSP_CALLMPI(NOSTMT, PMPI_Comm_free_keyval(&UG_COMM_HANDLE_KEY));
        if (mpi_errno != MPI_SUCCESS)
            CSP_DBG_PRINT("Cannot free UG_COMM_HANDLE_KEY %p\n", &UG_COMM_HANDLE_KEY);
    }
    return mpi_errno;
}

static inline int CSPU_fetch_ug_comm_from_cache(MPI_Comm comm, CSPU_comm_t ** ug_comm_ptr)
{
    int mpi_errno = MPI_SUCCESS;
    int fetch_ug_comm_flag = 0;

    CSP_CALLMPI(NOSTMT, PMPI_Comm_get_attr(comm, UG_COMM_HANDLE_KEY,
                                           ug_comm_ptr, &fetch_ug_comm_flag));
    if (!fetch_ug_comm_flag || mpi_errno != MPI_SUCCESS) {
        CSP_DBG_PRINT("Cannot fetch ug_comm from comm 0x%x\n", comm);
        (*ug_comm_ptr) = NULL;
    }
    return mpi_errno;
}

static inline int CSPU_cache_ug_comm(MPI_Comm comm, CSPU_comm_t * ug_comm)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_CALLMPI(NOSTMT, PMPI_Comm_set_attr(comm, UG_COMM_HANDLE_KEY, ug_comm));
    if (mpi_errno != MPI_SUCCESS) {
        CSP_DBG_PRINT("Cannot cache ug_comm %p for comm 0x%x\n", ug_comm, comm);
        return mpi_errno;
    }
    CSP_DBG_PRINT("cache ug_comm %p into comm 0x%x \n", ug_comm, comm);
    return mpi_errno;
}

static inline int CSPU_remove_ug_comm_from_cache(MPI_Comm comm)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_CALLMPI(NOSTMT, PMPI_Comm_delete_attr(comm, UG_COMM_HANDLE_KEY));
    if (mpi_errno != MPI_SUCCESS)
        CSP_DBG_PRINT("Cannot remove ug_comm cache for comm 0x%x\n", comm);
    return mpi_errno;
}

/* ======================================================================
 * Other prototypes
 * ====================================================================== */

extern int CSPU_ugcomm_set_info(CSPU_comm_info_args_t * info_args, MPI_Info info);
extern int CSPU_ugcomm_free(MPI_Comm comm);
extern int CSPU_ugcomm_create(MPI_Comm comm, MPI_Info info, MPI_Comm user_newcomm);

#endif /* CSPU_PT2PT_H_INCLUDED */

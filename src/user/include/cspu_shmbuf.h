/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSPU_SHMBUF_H_
#define CSPU_SHMBUF_H_

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "csp.h"
#include "cspu_comm.h"

typedef struct CSPU_shmbuf_win {
    CSPU_comm_t *ug_comm;
    void *base;
    MPI_Aint g_base_bound;
    MPI_Win win;
    MPI_Aint size;

    MPI_Aint *g_win_handles;
} CSPU_shmbuf_win_t;

typedef struct CSPU_shmbuf_record {
    void *base;
    MPI_Aint g_base_bound;
    MPI_Aint size;
    struct CSPU_shmbuf_record *next, *prev;
} CSPU_shmbuf_record_t;

#define CSP_DEFINE_SHMBUF_WIN_CACHE int SHMBUF_WIN_HANDLE_KEY = MPI_KEYVAL_INVALID
extern int SHMBUF_WIN_HANDLE_KEY;

static inline int CSPU_init_shmbuf_win_cache(void)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_CALLMPI(NOSTMT, PMPI_Win_create_keyval(MPI_WIN_NULL_COPY_FN, MPI_WIN_NULL_DELETE_FN,
                                               &SHMBUF_WIN_HANDLE_KEY, (void *) 0));
    return mpi_errno;
}

static inline int CSPU_destroy_shmbuf_win_cache(void)
{
    int mpi_errno = MPI_SUCCESS;
    if (SHMBUF_WIN_HANDLE_KEY != MPI_KEYVAL_INVALID) {
        CSP_CALLMPI(NOSTMT, PMPI_Win_free_keyval(&SHMBUF_WIN_HANDLE_KEY));
        if (mpi_errno != MPI_SUCCESS)
            CSP_DBG_PRINT("Cannot free SHMBUF_WIN_HANDLE_KEY %p\n", &SHMBUF_WIN_HANDLE_KEY);
    }
    return mpi_errno;
}

static inline int CSPU_fetch_shmbuf_win_from_cache(MPI_Win win, CSPU_shmbuf_win_t ** shmbuf_win)
{
    int mpi_errno = MPI_SUCCESS;
    int fetch_shmbuf_win_flag = 0;

    CSP_CALLMPI(NOSTMT,
                PMPI_Win_get_attr(win, SHMBUF_WIN_HANDLE_KEY, shmbuf_win, &fetch_shmbuf_win_flag));
    if (!fetch_shmbuf_win_flag || mpi_errno != MPI_SUCCESS) {
        CSP_DBG_PRINT("Cannot fetch shmbuf_win from win 0x%x\n", win);
        (*shmbuf_win) = NULL;
    }
    return mpi_errno;
}


static inline int CSPU_cache_shmbuf_win(MPI_Win win, CSPU_shmbuf_win_t * shmbuf_win)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_CALLMPI(NOSTMT, PMPI_Win_set_attr(win, SHMBUF_WIN_HANDLE_KEY, shmbuf_win));
    if (mpi_errno != MPI_SUCCESS) {
        CSP_DBG_PRINT("Cannot cache shmbuf_win %p for win 0x%x\n", shmbuf_win, win);
        return mpi_errno;
    }
    CSP_DBG_PRINT("cache ug_win %p into win 0x%x \n", shmbuf_win, win);
    return mpi_errno;
}

static inline int CSPU_remove_shmbuf_win_from_cache(MPI_Win win)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_CALLMPI(NOSTMT, PMPI_Win_delete_attr(win, SHMBUF_WIN_HANDLE_KEY));
    if (mpi_errno != MPI_SUCCESS)
        CSP_DBG_PRINT("Cannot remove shmbuf_win cache for win 0x%x\n", win);
    return mpi_errno;
}

extern CSPU_shmbuf_record_t *CSPU_shmbuf_list;

static inline void CSPU_shmbuf_record_append(void *base, MPI_Aint size, MPI_Aint g_base_bound)
{
    CSPU_shmbuf_record_t *record = NULL;
    record = CSP_calloc(1, sizeof(CSPU_shmbuf_record_t));
    CSP_ASSERT(record != NULL);

    record->base = base;
    record->g_base_bound = g_base_bound;
    record->size = size;

    /* tail is internally stored at head->prev. */
    DL_APPEND(CSPU_shmbuf_list, record);
    CSP_DBG_PRINT("SHMBUF append record %p, base=%p, g_base_bound=0x%lx, size=0x%lx\n",
                  record, base, g_base_bound, size);
}

static inline int CSPU_shmbuf_record_contain(CSPU_shmbuf_record_t * shmbase,
                                             CSPU_shmbuf_record_t * buf)
{
    int contain = ((MPI_Aint) shmbase->base <= (MPI_Aint) buf->base)
        && ((MPI_Aint) buf->base + buf->size <= (MPI_Aint) shmbase->base + shmbase->size);
    return contain == 1 ? 0 : 1;
}

static inline void CSPU_shmbuf_record_find(void *addr, CSPU_shmbuf_record_t ** record_ptr)
{
    CSPU_shmbuf_record_t *record = NULL;
    CSPU_shmbuf_record_t like;

    *record_ptr = NULL;

    /* Find the record with base <= addr, addr + size <= base + size */
    like.base = addr;
    like.size = 0;
    DL_SEARCH(CSPU_shmbuf_list, record, &like, CSPU_shmbuf_record_contain);
    *record_ptr = record;

    if (record) {
        CSP_DBG_PRINT("SHMBUF found addr %p->record %p, base=%p, g_base_bound=0x%lx\n",
                      addr, record, record->base, record->g_base_bound);
    }
}

static inline void CSPU_shmbuf_record_remove(void *base)
{
    CSPU_shmbuf_record_t *record = NULL;

    /* Find the record with base */
    DL_SEARCH_SCALAR(CSPU_shmbuf_list, record, base, base);
    CSP_ASSERT(record != NULL);

    DL_DELETE(CSPU_shmbuf_list, record);

    CSP_DBG_PRINT("SHMBUF remove base %p->record %p\n", base, record);

    free(record);
}

static inline void CSPU_shmbuf_translate_g_addr(void *addr, MPI_Aint * g_addr_ptr, int *found)
{
    CSPU_shmbuf_record_t *record = NULL;

    /* We trust user pass valid addr and count if hint is set. Thus we do not check
     * the upper bound of buffer. */
    *found = 0;
    CSPU_shmbuf_record_find(addr, &record);
    if (record) {
        *g_addr_ptr = record->g_base_bound + (MPI_Aint) addr - (MPI_Aint) record->base;
        *found = 1;
    }
}

extern int CSPU_shmbuf_regist(CSPU_comm_t * ug_comm, MPI_Aint size, int disp_unit, MPI_Info info,
                              MPI_Comm comm, void *baseptr, MPI_Win * win);
extern int CSPU_shmbuf_free(MPI_Win * win, int *freed);

#endif /* CSPU_SHMBUF_H_ */

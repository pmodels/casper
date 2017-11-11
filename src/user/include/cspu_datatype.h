/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSPU_DATATYPE_H_
#define CSPU_DATATYPE_H_

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "csp.h"
#include "csp_datatype.h"

#ifdef CSP_DDT_DEBUG
#define CSP_DDT_DBG_PRINT CSP_DBG_PRINT
#else
#define CSP_DDT_DBG_PRINT(str,...) do {} while (0)
#endif

typedef struct CSPU_datatype_g_hash_record {
    UT_hash_handle hh;
    MPI_Datatype key;           /* Local datatype handle is the key */
    MPI_Datatype g_handle;
} CSPU_datatype_g_hash_record_t;

typedef struct CSPU_datatype_g_hash {
    CSPU_datatype_g_hash_record_t *record;
} CSPU_datatype_g_hash_t;

typedef struct CSPU_datatype_db {
    MPI_Win shm_win;

    /* predefined datatype mapping for each ghost process */
    CSPU_datatype_g_hash_t *g_predefined_hashs;
} CSPU_datatype_db_t;

extern CSPU_datatype_db_t CSPU_datatype_db;

static inline void CSPU_datatype_g_hash_add(CSPU_datatype_g_hash_t * hash, MPI_Datatype my_handle,
                                            MPI_Datatype g_handle)
{
    CSPU_datatype_g_hash_record_t *record = NULL;

    /* First try to find if the key already exists */
    HASH_FIND(hh, (hash->record), &my_handle, sizeof(MPI_Datatype), record);

    if (record == NULL) {
        /* Insert new record to hash.
         * Note that two datatypes may use the same value (e.g., long long int and
         * long long), so we simply ignore any duplicated key. */
        record = CSP_calloc(1, sizeof(CSPU_datatype_g_hash_record_t));
        record->key = my_handle;
        record->g_handle = g_handle;

        HASH_ADD(hh, (hash->record), key, sizeof(MPI_Datatype), record);

        CSP_DDT_DBG_PRINT("DATATYPE: hash add record %p key 0x%x, val 0x%x\n",
                          record, record->key, record->g_handle);
    }
}

static inline void CSPU_datatype_g_hash_get(CSPU_datatype_g_hash_t hash, MPI_Datatype my_handle,
                                            MPI_Datatype * g_handle_ptr, int *found)
{
    CSPU_datatype_g_hash_record_t *record = NULL;

    (*g_handle_ptr) = MPI_DATATYPE_NULL;
    (*found) = 0;

    HASH_FIND(hh, (hash.record), &my_handle, sizeof(MPI_Datatype), record);
    if (record) {
        (*g_handle_ptr) = record->g_handle;
        (*found) = 1;
    }
}

static inline void CSPU_datatype_g_hash_remove(CSPU_datatype_g_hash_t * hash,
                                               MPI_Datatype my_handle)
{
    CSPU_datatype_g_hash_record_t *record = NULL;

    HASH_FIND(hh, (hash->record), &my_handle, sizeof(MPI_Datatype), record);
    /* We ignore any not-found key, as we may ignore insertion for duplicated key. */
    if (record != NULL) {
        HASH_DEL((hash->record), record);

        CSP_DDT_DBG_PRINT("DATATYPE: hash remove record %p key 0x%x, val 0x%x\n",
                          record, record->key, record->g_handle);
        free(record);
    }
}

static inline int CSPU_datatype_g_hash_count(CSPU_datatype_g_hash_t hash)
{
    return HASH_COUNT(hash.record);
}

static inline int CSPU_datatype_get_g_handle(MPI_Datatype myhandle, int ghost_lrank,
                                             MPI_Datatype * g_handle_ptr)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Datatype g_handle = MPI_DATATYPE_NULL;
    int nints, naddrs, ndtypes, combiner;
    int found = 0;

    CSP_CALLMPI(JUMP, PMPI_Type_get_envelope(myhandle, &nints, &naddrs, &ndtypes, &combiner));

    if (combiner == MPI_COMBINER_NAMED) {
        CSPU_datatype_g_hash_get(CSPU_datatype_db.g_predefined_hashs[ghost_lrank],
                                 myhandle, &g_handle, &found);
    }

    /* Must be found in one of the hashes.
     * Not sure if DATATYPE_NULL is a valid datatype on other processes,
     * so we use a found flag instead.*/
    if (!found) {
        CSP_msg_print(CSP_MSG_WARN, "Found unknown or not supported datatype 0x%lx "
                      "in pt2pt message offloading\n", (unsigned long) myhandle);
        CSP_ASSERT(found);
    }

    (*g_handle_ptr) = g_handle;

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

#endif /* CSPU_DATATYPE_H_ */

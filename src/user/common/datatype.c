/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cspu.h"

CSPU_datatype_db_t CSPU_datatype_db;
static MPI_Datatype local_predefined_table[CSP_DATATYPE_MAX] = { 0 };

/* Destroy datatype database.
 * This must be called after sent cwp finalize to ghost.  */
int CSPU_datatype_destroy(void)
{
    int mpi_errno = MPI_SUCCESS;
    int i, dt;

    if (CSPU_datatype_db.g_predefined_hashs) {
        for (i = 0; i < CSP_ENV.num_g; i++) {
            /* Destroy all predefined hash records */
            for (dt = 0; dt < CSP_DATATYPE_MAX; dt++) {
                CSPU_datatype_g_hash_remove(&CSPU_datatype_db.g_predefined_hashs[i],
                                            local_predefined_table[dt]);
            }
        }
        free(CSPU_datatype_db.g_predefined_hashs);
        CSPU_datatype_db.g_predefined_hashs = NULL;
    }

    if (CSPU_datatype_db.g_ddt_hashs) {
        for (i = 0; i < CSP_ENV.num_g; i++) {
            /* Derived datatype should be removed at type free. */
            CSP_ASSERT(CSPU_datatype_g_hash_count(CSPU_datatype_db.g_ddt_hashs[i]) == 0);
        }
        free(CSPU_datatype_db.g_ddt_hashs);
        CSPU_datatype_db.g_ddt_hashs = NULL;
    }

    return mpi_errno;
}

int CSPU_datatype_init(void)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Datatype *g_predefined_tables = NULL;
    MPI_Request *reqs = NULL;
    int i, dt;

    memset(&CSPU_datatype_db, 0, sizeof(CSPU_datatype_db_t));
    CSP_datatype_fill_predefined_table(local_predefined_table);

    CSPU_datatype_db.g_predefined_hashs = CSP_calloc(CSP_ENV.num_g, sizeof(CSPU_datatype_g_hash_t));
    CSPU_datatype_db.g_ddt_hashs = CSP_calloc(CSP_ENV.num_g, sizeof(CSPU_datatype_g_hash_t));

    g_predefined_tables = CSP_calloc(CSP_ENV.num_g, CSP_DATATYPE_MAX * sizeof(MPI_Datatype));
    memset(g_predefined_tables, 0, CSP_ENV.num_g * CSP_DATATYPE_MAX * sizeof(MPI_Datatype));

    reqs = CSP_calloc(CSP_ENV.num_g, sizeof(MPI_Request));

    /* Receive predefined datatype table from each ghost process. */
    for (i = 0; i < CSP_ENV.num_g; i++) {
        int offset = i * CSP_DATATYPE_MAX;
        CSP_CALLMPI(JUMP, PMPI_Ibcast((char *) &g_predefined_tables[offset],
                                      CSP_DATATYPE_MAX * sizeof(MPI_Datatype),
                                      MPI_BYTE, i, CSP_PROC.local_comm, &reqs[i]));
    }
    CSP_CALLMPI(JUMP, PMPI_Waitall(CSP_ENV.num_g, reqs, MPI_STATUS_IGNORE));

    /* Insert received predefined datatypes into local hash */
    for (i = 0; i < CSP_ENV.num_g; i++) {
        int offset = i * CSP_DATATYPE_MAX;

        for (dt = 0; dt < CSP_DATATYPE_MAX; dt++) {

            CSPU_datatype_g_hash_add(&CSPU_datatype_db.g_predefined_hashs[i],
                                     local_predefined_table[dt], g_predefined_tables[offset + dt]);
        }
    }

    CSP_DBG_PRINT("DATATYPE: initialized g_predefined_hashs=%p, g_ddt_hashs=%p\n",
                  CSPU_datatype_db.g_predefined_hashs, CSPU_datatype_db.g_ddt_hashs);

  fn_exit:
    free(g_predefined_tables);
    free(reqs);

    return mpi_errno;
  fn_fail:
    /* Free global objects in main function. */
    goto fn_exit;
}

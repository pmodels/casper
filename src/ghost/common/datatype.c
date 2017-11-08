/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cspg.h"

typedef struct datatype_ddt_elem {
    MPI_Datatype handle;
    struct datatype_ddt_elem *next;
} datatype_ddt_elem_t;

typedef struct CSPG_datatype_db {
    struct {
        datatype_ddt_elem_t *head, *tail;
        int count;
    } regist_ddt_list;
} CSPG_datatype_db_t;

CSPG_datatype_db_t datatype_db;

static inline void datatype_regist_ddts_append(MPI_Datatype handle)
{
    datatype_ddt_elem_t *elem = NULL;
    elem = CSP_calloc(1, sizeof(datatype_ddt_elem_t));
    CSP_ASSERT(elem != NULL);
    elem->handle = handle;

    if (datatype_db.regist_ddt_list.head == NULL) {
        CSP_DBG_ASSERT(datatype_db.regist_ddt_list.tail == NULL &&
                       datatype_db.regist_ddt_list.count == 0);
        datatype_db.regist_ddt_list.head = elem;
        datatype_db.regist_ddt_list.tail = elem;
    }
    else {
        datatype_db.regist_ddt_list.tail->next = elem;
        datatype_db.regist_ddt_list.tail = elem;
    }
    datatype_db.regist_ddt_list.count++;
}

static void datatype_regist_ddts_init(void)
{
    datatype_db.regist_ddt_list.head = NULL;
    datatype_db.regist_ddt_list.tail = NULL;
    datatype_db.regist_ddt_list.count = 0;
}

static int datatype_regist_ddts_destroy(void)
{
    int mpi_errno = MPI_SUCCESS;
    datatype_ddt_elem_t *elem = NULL, *prev_elem = NULL;

    if (datatype_db.regist_ddt_list.head == NULL) {
        CSP_DBG_ASSERT(datatype_db.regist_ddt_list.tail == NULL &&
                       datatype_db.regist_ddt_list.count == 0);
        goto fn_exit;
    }

    /* Destroy all registered derived datatypes */
    elem = datatype_db.regist_ddt_list.head;
    while (elem != NULL) {
        prev_elem = elem;
        elem = elem->next;

        CSPG_DBG_PRINT("DATATYPE: free regist ddt 0x%x, count=%d\n", prev_elem->handle,
                       datatype_db.regist_ddt_list.count);

        CSP_CALLMPI(JUMP, PMPI_Type_free(&prev_elem->handle));
        free(prev_elem);
        datatype_db.regist_ddt_list.count--;
    }

    CSP_DBG_ASSERT(datatype_db.regist_ddt_list.count == 0);

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

int CSPG_datatype_regist_cwp_handler(CSP_cwp_pkt_t * pkt)
{
    int mpi_errno = MPI_SUCCESS;

    /* FIXME: implement ddt registration. */
    CSP_ASSERT(0);

    return mpi_errno;
}

int CSPG_datatype_destory(void)
{
    /* Terminate ensures all local users have been finalizing. */

    CSPG_DBG_PRINT("DATATYPE: destroy regist_ddt_list\n");
    return datatype_regist_ddts_destroy();
}

int CSPG_datatype_init(void)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Request *reqs = NULL;
    int i, local_rank = 0;
    MPI_Datatype local_predefined_table[CSP_DATATYPE_MAX], temp_buf[CSP_DATATYPE_MAX];

    CSP_datatype_fill_predefined_table(local_predefined_table);
    memset(&datatype_db, 0, sizeof(CSPG_datatype_db_t));
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_PROC.local_comm, &local_rank));

    reqs = CSP_calloc(CSP_ENV.num_g, sizeof(MPI_Request));

    /* Bcast my predefined datatype table, and join the other ghosts' bcast. */
    for (i = 0; i < CSP_ENV.num_g; i++) {
        char *buf = (char *) temp_buf;
        if (i == local_rank)
            buf = (char *) local_predefined_table;
        CSP_CALLMPI(JUMP, PMPI_Ibcast(buf, CSP_DATATYPE_MAX * sizeof(MPI_Datatype),
                                      MPI_BYTE, i, CSP_PROC.local_comm, &reqs[i]));
    }
    CSP_CALLMPI(JUMP, PMPI_Waitall(CSP_ENV.num_g, reqs, MPI_STATUS_IGNORE));

    datatype_regist_ddts_init();

    CSP_DBG_PRINT("DATATYPE: init done\n");

  fn_exit:
    free(reqs);
    return mpi_errno;
  fn_fail:
    /* Free global objects in main function. */
    goto fn_exit;
}

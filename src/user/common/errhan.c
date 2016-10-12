/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "cspu.h"

/* ======================================================================
 * Structure definition and declaration.
 * ====================================================================== */

typedef struct CSPU_errhan_hash_record {
    UT_hash_handle hh;
    MPI_Errhandler key;
    void *valptr;
} CSPU_errhan_hash_record_t;

typedef struct CSPU_errhan_hash {
#if defined(CSP_ENABLE_THREAD_SAFE)
    CSP_thread_cs_t cs;         /* critical section object,
                                 * used only when this process is multi-threaded. */
#endif
    CSPU_errhan_hash_record_t *record;
} CSPU_errhan_hash_t;

static CSPU_errhan_hash_t errhan_fnc_hash;
CSPU_errhan_extobj_t CSPU_errhan_extobj;        /*TODO : make thread safe */

/* ======================================================================
 * Common initialization and destroy.
 * ====================================================================== */

static inline void errhan_init_extobj(void)
{
    CSPU_errhan_extobj.type = CSPU_ERRHAN_EXT_UNSET;
}

static inline void errhan_destroy_extobj(void)
{
    if (CSPU_errhan_extobj.type != CSPU_ERRHAN_EXT_UNSET) {
        CSP_msg_print(CSP_MSG_WARN, "External error object is not reset yet, type = %d(%s) !\n",
                      CSPU_errhan_extobj.type,
                      CSPU_errhan_extobj.type == CSPU_ERRHAN_EXT_WIN ? "win" : "comm");
    }
}

int CSPU_errhan_init(void)
{
    int mpi_errno = MPI_SUCCESS;

    CSPU_THREAD_INIT_OBJ_CS(&errhan_fnc_hash);

    errhan_init_extobj();

    mpi_errno = CSPU_comm_errhan_init();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    mpi_errno = CSPU_win_errhan_init();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    return mpi_errno;
  fn_fail:
    /* Do not release global objects, they are released at MPI_Init_thread. */
    goto fn_exit;
}

int CSPU_errhan_destroy(void)
{
    int mpi_errno = MPI_SUCCESS;
    unsigned int nrecord = 0;

    mpi_errno = CSPU_win_errhan_destroy();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    mpi_errno = CSPU_comm_errhan_destroy();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    errhan_destroy_extobj();

    CSPU_THREAD_DESTROY_OBJ_CS(&errhan_fnc_hash);
    /* Release remaining hash records (incorrect user code, user should always
     * release each at errhandler_free). */
    nrecord = HASH_COUNT((errhan_fnc_hash.record));
    if (nrecord > 0) {
        CSP_msg_print(CSP_MSG_WARN, "%d errhandler record are not freed !\n", nrecord);

        CSPU_errhan_hash_record_t *record, *tmp;
        HASH_ITER(hh, (errhan_fnc_hash.record), record, tmp) {
            HASH_DEL((errhan_fnc_hash.record), record);
            free(record);
        }
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}


/* ======================================================================
 * [Error handler -> callback function] hash routines
 * ====================================================================== */

void CSPU_errhan_cache_fnc(MPI_Errhandler handler, void *fnc)
{
    CSPU_errhan_hash_record_t *record = NULL;

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();

    record = CSP_calloc(1, sizeof(CSPU_errhan_hash_record_t));
    CSP_ASSERT(record != NULL);
    record->key = handler;
    record->valptr = fnc;

    CSPU_THREAD_ENTER_OBJ_CS(&errhan_fnc_hash);
    HASH_ADD(hh, (errhan_fnc_hash.record), key, sizeof(MPI_Errhandler), record);
    CSPU_THREAD_EXIT_OBJ_CS(&errhan_fnc_hash);

    CSP_DBG_PRINT("%s: cached errhandler 0x%x -> fnc %p\n", __FUNCTION__, handler, fnc);
}

void CSPU_errhan_get_fnc(MPI_Errhandler handler, void **fncptr)
{
    CSPU_errhan_hash_record_t *record = NULL;

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();
    CSPU_THREAD_ENTER_OBJ_CS(&errhan_fnc_hash);
    HASH_FIND(hh, (errhan_fnc_hash.record), &handler, sizeof(MPI_Errhandler), record);
    CSPU_THREAD_EXIT_OBJ_CS(&errhan_fnc_hash);

    if (record != NULL)
        (*fncptr) = record->valptr;

    CSP_DBG_PRINT("%s: got errhandler 0x%x -> fnc %p\n", __FUNCTION__, handler, (*fncptr));
}

void CSPU_errhan_remove_fnc(MPI_Errhandler handler)
{
    CSPU_errhan_hash_record_t *record = NULL;

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();
    CSPU_THREAD_ENTER_OBJ_CS(&errhan_fnc_hash);
    HASH_FIND(hh, (errhan_fnc_hash.record), &handler, sizeof(MPI_Errhandler), record);
    if (record != NULL) {
        HASH_DEL((errhan_fnc_hash.record), record);
        free(record);
    }
    CSPU_THREAD_EXIT_OBJ_CS(&errhan_fnc_hash);

    CSP_DBG_PRINT("%s: removed errhandler 0x%x\n", __FUNCTION__, handler);
}

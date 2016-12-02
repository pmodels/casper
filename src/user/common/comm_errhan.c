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
 * [Communicator -> errhandler & callback function] internal hash routines
 * ====================================================================== */
typedef struct comm_errhan_hash_record {
    UT_hash_handle hh;
    MPI_Comm key;
    MPI_Errhandler errhandler;
    MPI_Comm_errhandler_function *fnc;
} comm_errhan_hash_record_t;

typedef struct CSPU_errhan_hash {
#if defined(CSP_ENABLE_THREAD_SAFE)
    CSP_thread_cs_t cs;         /* critical section object,
                                 * used only when this process is multi-threaded. */
#endif
    comm_errhan_hash_record_t *record;
} comm_errhan_hash_t;

static comm_errhan_hash_t comm_errhan_hash;

static inline void comm_errhan_hash_add(MPI_Comm comm, MPI_Errhandler handler,
                                        MPI_Comm_errhandler_function * fnc)
{
    comm_errhan_hash_record_t *record = NULL;

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();
    CSPU_THREAD_ENTER_OBJ_CS(&comm_errhan_hash);
    /* First try to find if the key already exists */
    HASH_FIND(hh, (comm_errhan_hash.record), &comm, sizeof(MPI_Comm), record);
    if (record == NULL) {
        /* If it is new, allocate new record and add to hash */
        record = CSP_calloc(1, sizeof(comm_errhan_hash_record_t));
        CSP_ASSERT(record != NULL);

        record->key = comm;
        HASH_ADD(hh, (comm_errhan_hash.record), key, sizeof(MPI_Comm), record);
    }

    /* Update record items */
    record->errhandler = handler;
    record->fnc = fnc;
    CSPU_THREAD_EXIT_OBJ_CS(&comm_errhan_hash);
}

static inline void comm_errhan_hash_get(MPI_Comm comm, MPI_Errhandler * handler,
                                        MPI_Comm_errhandler_function ** fncptr)
{
    comm_errhan_hash_record_t *record = NULL;

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();

    (*handler) = MPI_ERRHANDLER_NULL;
    (*fncptr) = NULL;

    CSPU_THREAD_ENTER_OBJ_CS(&comm_errhan_hash);
    HASH_FIND(hh, (comm_errhan_hash.record), &comm, sizeof(MPI_Comm), record);
    CSPU_THREAD_EXIT_OBJ_CS(&comm_errhan_hash);

    if (record != NULL) {
        (*fncptr) = record->fnc;
        (*handler) = record->errhandler;
    }
}

static inline void comm_errhan_hash_remove(MPI_Comm comm, MPI_Errhandler * handler,
                                           MPI_Comm_errhandler_function ** fncptr)
{
    comm_errhan_hash_record_t *record = NULL;

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();
    CSPU_THREAD_ENTER_OBJ_CS(&comm_errhan_hash);
    HASH_FIND(hh, (comm_errhan_hash.record), &comm, sizeof(MPI_Comm), record);
    if (record != NULL) {
        HASH_DEL((comm_errhan_hash.record), record);
        (*fncptr) = record->fnc;
        (*handler) = record->errhandler;
        free(record);
    }
    CSPU_THREAD_EXIT_OBJ_CS(&comm_errhan_hash);
}

/* ======================================================================
 * Other internal routines
 * ====================================================================== */

/* Common wrapped error handling routine.
 * - Do error handling following the cached handler and callback.
 * - If it is COMM_USER_WORLD, we should expose COMM_WORLD instead. */
static inline int comm_errhan_wrapper_impl(MPI_Comm errcomm, int *errcode, ...)
{
    MPI_Errhandler handler = MPI_ERRHANDLER_NULL;
    MPI_Comm_errhandler_function *fnc = NULL;
    int mpi_errno = MPI_SUCCESS;
    va_list list;

    /* We wrap up error handler for all communicators, including COMM_WORLDs
     * and all user communicators. The only case we cannot find cache record
     * is invalid errcomm. */
    comm_errhan_hash_get(errcomm, &handler, &fnc);

    /* Do do switch/case on handler because it might not be integer type in some
     * MPI implementations (e.g., openmpi). */
    if (handler == MPI_ERRHANDLER_NULL) {
        /* invalid errcomm, let MPI handle. */
        CSP_CALLMPI(RETURN, PMPI_Comm_call_errhandler(errcomm, *errcode));
    }
    else if (handler == MPI_ERRORS_ARE_FATAL) {
        /* abort. (no infinite recursion) */
        CSP_CALLMPI(RETURN, PMPI_Abort(errcomm, *errcode));
    }
    else if (handler == MPI_ERRORS_RETURN) {
        /* do nothing */
    }
    else {
        MPI_Comm expcomm_var;   /* to get valid object pointer */

        /* user defined callback function. */
        CSP_ASSERT(fnc != NULL);

        /* Expose MPI_COMM_WORLD instead of CSP_COMM_USER_WORLD */
        expcomm_var = errcomm;
        if (expcomm_var == CSP_COMM_USER_WORLD)
            expcomm_var = MPI_COMM_WORLD;

        /* Call error handler on expcomm */
        va_start(list, errcode);
        fnc(&expcomm_var, errcode, list);
        va_end(list);
    }

    return mpi_errno;
}

/* ======================================================================
 * Initialization & Destroy & Callback
 * ====================================================================== */

int CSPU_comm_errhan_init(void)
{
    int mpi_errno = MPI_SUCCESS;

    CSPU_THREAD_INIT_OBJ_CS(&comm_errhan_hash);

    CSP_CALLMPI(JUMP, PMPI_Comm_create_errhandler(CSPU_comm_errhan_wrapper_fnc,
                                                  &CSP_PROC.user.comm_errhan_wrapper));
  fn_exit:
    return mpi_errno;
  fn_fail:
    /* Do not release global objects, they are released at MPI_Init_thread. */
    goto fn_exit;
}

int CSPU_comm_errhan_destroy(void)
{
    int mpi_errno = MPI_SUCCESS;
    unsigned int nrecord = 0;

    CSPU_THREAD_DESTROY_OBJ_CS(&comm_errhan_hash);

    /* Release remaining hash records (incorrect user code, user should always
     * release each at errhandler_free). */
    nrecord = HASH_COUNT((comm_errhan_hash.record));
    if (nrecord > 0) {
        comm_errhan_hash_record_t *record, *tmp;

        CSP_msg_print(CSP_MSG_WARN, "%d comm:errhand record are not freed !\n", nrecord);
        HASH_ITER(hh, (comm_errhan_hash.record), record, tmp) {
            HASH_DEL((comm_errhan_hash.record), record);
            free(record);
        }
    }

    if (CSP_PROC.user.comm_errhan_wrapper != MPI_ERRHANDLER_NULL)
        CSP_CALLMPI(JUMP, PMPI_Errhandler_free(&CSP_PROC.user.comm_errhan_wrapper));

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

/* ======================================================================
 * Other Public Utilities
 * ====================================================================== */

/* The wrapper function of user communicator callback error handling function.
 * It checks if the current routine is handled by another object at upper layer
 * (CASPER overwritten routine). E.g., win_fence may incur an error on user
 * communicator, but the error is expected handled by the window object.
 * - If another object is handling, just return thus the upper layer handles it.
 * - If not, do error handling here . */
void CSPU_comm_errhan_wrapper_fnc(MPI_Comm * errcomm, int *errcode, ...)
{
    int setflag = 0;
    va_list list;

    /* handle error here only when no external error object is set. */
    CSPU_ERRHAN_CHECK_EXTOBJ(&setflag);
    if (!setflag) {
        va_start(list, errcode);
        comm_errhan_wrapper_impl(*errcomm, errcode, list);
        va_end(list);
    }
}

void CSPU_comm_errhan_cache(MPI_Comm comm, MPI_Errhandler handler,
                            MPI_Comm_errhandler_function * fnc)
{
    comm_errhan_hash_add(comm, handler, fnc);
    CSP_DBG_PRINT("%s: cached comm 0x%x -> errhandler 0x%x, fnc %p\n",
                  __FUNCTION__, comm, handler, fnc);
}

void CSPU_comm_errhan_get(MPI_Comm comm, MPI_Errhandler * handler,
                          MPI_Comm_errhandler_function ** fncptr)
{
    comm_errhan_hash_get(comm, handler, fncptr);
    CSP_DBG_PRINT("%s: got comm 0x%x -> errhandler 0x%x, fnc %p\n",
                  __FUNCTION__, comm, *handler, *fncptr);
}

/* Set the error handler wrapper to the communicator and cache the original one.
 * It is used to track any external error object when an internal MPI call
 * of CASPER overwriting functions produces error. */
int CSPU_comm_errhan_wrap(MPI_Comm comm, MPI_Errhandler errhandler,
                          MPI_Comm_errhandler_function * errhandler_fnc)
{
    int mpi_errno = MPI_SUCCESS;

    CSP_ASSERT(errhandler == MPI_ERRORS_ARE_FATAL ||
               errhandler == MPI_ERRORS_RETURN || errhandler_fnc != NULL);

    /* Set error handler wrapper to communicator. */
    CSP_CALLMPI(RETURN, PMPI_Comm_set_errhandler(comm, CSP_PROC.user.comm_errhan_wrapper));

    /* Skip invalid communicator. */
    if (comm != MPI_COMM_NULL) {
        /* Cache the user error handler and callback.
         * It is used at the error handling routine of CASPER overwritten calls. */
        comm_errhan_hash_add(comm, errhandler, errhandler_fnc);

        CSP_DBG_PRINT("%s: wrapped comm 0x%x -> errhandler 0x%x, fnc %p\n",
                      __FUNCTION__, comm, errhandler, errhandler_fnc);
    }

    return mpi_errno;
}

/* Inherit the error handler from parent communicator.
 * Note that MPI does only inherit the error handler wrapper in the new
 * communicator, thus we need *inherit* the original handler and callback
 * in CASPER cache. */
int CSPU_comm_errhan_inherit(MPI_Comm comm, MPI_Comm newcomm)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Errhandler errhandler = MPI_ERRHANDLER_NULL;
    MPI_Comm_errhandler_function *errhandler_fnc = NULL;

    /* We wrap up error handler for all communicators, including COMM_WORLDs
     * and all user communicators. */
    comm_errhan_hash_get(comm, &errhandler, &errhandler_fnc);
    CSP_ASSERT(errhandler != MPI_ERRHANDLER_NULL &&
               (errhandler == MPI_ERRORS_ARE_FATAL ||
                errhandler == MPI_ERRORS_RETURN || errhandler_fnc != NULL));

    /* Cache the user error handler and callback.
     * It is used at the error handling routine of CASPER overwritten calls. */
    comm_errhan_hash_add(newcomm, errhandler, errhandler_fnc);

    CSP_DBG_PRINT("%s: inherit comm 0x%x -> errhandler 0x%x, fnc %p from oldcomm 0x%x\n",
                  __FUNCTION__, newcomm, errhandler, errhandler_fnc, comm);

    return mpi_errno;
}

int CSPU_comm_errhan_reset(MPI_Comm comm)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Errhandler errhandler = MPI_ERRHANDLER_NULL;
    MPI_Comm_errhandler_function *errhandler_fnc = NULL;
    int reset_flag = 0;

    /* Remove cache record and get original handler. */
    comm_errhan_hash_remove(comm, &errhandler, &errhandler_fnc);
    CSP_ASSERT(errhandler != MPI_ERRHANDLER_NULL &&
               (errhandler == MPI_ERRORS_ARE_FATAL ||
                errhandler == MPI_ERRORS_RETURN || errhandler_fnc != NULL));

    if (errhandler == MPI_ERRORS_ARE_FATAL || errhandler == MPI_ERRORS_RETURN) {
        reset_flag = 1;
    }
    else {
        errhandler_fnc = NULL;
        CSPU_errhan_get_fnc(errhandler, (void **) &errhandler_fnc);
        if (errhandler_fnc != NULL)     /* no record if handler is freed. */
            reset_flag = 1;
    }

    CSP_DBG_PRINT("%s: reset comm 0x%x -> errhandler 0x%x, fnc %p, reset_flag=%d\n",
                  __FUNCTION__, comm, errhandler, errhandler_fnc, reset_flag);

    if (reset_flag) {
        /* Reset to user error handler in MPI if the handler is still valid. */
        CSP_CALLMPI(RETURN, PMPI_Comm_set_errhandler(comm, errhandler));
    }

    return mpi_errno;
}

/* Called at communicator call's error handling routine.
 * It triggers the error handler specified to the exposed communicator. */
int CSPU_comm_call_errhandler(MPI_Comm expcomm, int *errcode)
{
    int setflag = 0;
    int mpi_errno = MPI_SUCCESS;

    /* ensure external object is reset, thus no infinite recursion risk. */
    CSPU_ERRHAN_CHECK_EXTOBJ(&setflag);
    CSP_ASSERT(setflag == 0);

    mpi_errno = comm_errhan_wrapper_impl(expcomm, errcode);

    return mpi_errno;
}

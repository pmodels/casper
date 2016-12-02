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
 * [Window -> errhandler & callback function] internal hash routines
 * ====================================================================== */
typedef struct win_errhan_hash_record {
    UT_hash_handle hh;
    MPI_Win key;
    MPI_Errhandler errhandler;
    MPI_Win_errhandler_function *fnc;
} win_errhan_hash_record_t;

typedef struct CSPU_errhan_hash {
#if defined(CSP_ENABLE_THREAD_SAFE)
    CSP_thread_cs_t cs;         /* critical section object,
                                 * used only when this process is multi-threaded. */
#endif
    win_errhan_hash_record_t *record;
} win_errhan_hash_t;

static win_errhan_hash_t win_errhan_hash;


static inline void win_errhan_hash_add(MPI_Win win, MPI_Errhandler handler,
                                       MPI_Win_errhandler_function * fnc)
{
    win_errhan_hash_record_t *record = NULL;

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();
    CSPU_THREAD_ENTER_OBJ_CS(&win_errhan_hash);
    /* First try to find if the key already exists */
    HASH_FIND(hh, (win_errhan_hash.record), &win, sizeof(MPI_Win), record);
    if (record == NULL) {
        /* If it is new, allocate new record and add to hash */
        record = CSP_calloc(1, sizeof(win_errhan_hash_record_t));
        CSP_ASSERT(record != NULL);

        record->key = win;
        HASH_ADD(hh, (win_errhan_hash.record), key, sizeof(MPI_Win), record);
    }

    /* Update record items */
    record->errhandler = handler;
    record->fnc = fnc;
    CSPU_THREAD_EXIT_OBJ_CS(&win_errhan_hash);
}

static inline void win_errhan_hash_get(MPI_Win win, MPI_Errhandler * handler,
                                       MPI_Win_errhandler_function ** fncptr)
{
    win_errhan_hash_record_t *record = NULL;

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();

    (*handler) = MPI_ERRHANDLER_NULL;
    (*fncptr) = NULL;

    CSPU_THREAD_ENTER_OBJ_CS(&win_errhan_hash);
    HASH_FIND(hh, (win_errhan_hash.record), &win, sizeof(MPI_Win), record);
    CSPU_THREAD_EXIT_OBJ_CS(&win_errhan_hash);

    if (record != NULL) {
        (*fncptr) = record->fnc;
        (*handler) = record->errhandler;
    }
}

static inline void win_errhan_hash_remove(MPI_Win win, MPI_Errhandler * handler,
                                          MPI_Win_errhandler_function ** fncptr)
{
    win_errhan_hash_record_t *record = NULL;

    CSPU_THREAD_OBJ_CS_LOCAL_DCL();
    CSPU_THREAD_ENTER_OBJ_CS(&win_errhan_hash);
    HASH_FIND(hh, (win_errhan_hash.record), &win, sizeof(MPI_Win), record);
    if (record != NULL) {
        HASH_DEL((win_errhan_hash.record), record);
        (*fncptr) = record->fnc;
        (*handler) = record->errhandler;
        free(record);
    }
    CSPU_THREAD_EXIT_OBJ_CS(&win_errhan_hash);
}

/* ======================================================================
 * Initialization & Destroy & Callback
 * ====================================================================== */

int CSPU_win_errhan_init(void)
{
    int mpi_errno = MPI_SUCCESS;

    CSPU_THREAD_INIT_OBJ_CS(&win_errhan_hash);

  fn_exit:
    return mpi_errno;
  fn_fail:
    CSP_ATTRIBUTE((unused));
    /* Do not release global objects, they are released at MPI_Init_thread. */
    goto fn_exit;
}

int CSPU_win_errhan_destroy(void)
{
    int mpi_errno = MPI_SUCCESS;
    unsigned int nrecord = 0;

    CSPU_THREAD_DESTROY_OBJ_CS(&win_errhan_hash);

    /* Release remaining hash records (incorrect user code, user should always
     * release each at errhandler_free). */
    nrecord = HASH_COUNT((win_errhan_hash.record));
    if (nrecord > 0) {
        win_errhan_hash_record_t *record, *tmp;

        CSP_msg_print(CSP_MSG_WARN, "%d win:errhand record are not freed !\n", nrecord);
        HASH_ITER(hh, (win_errhan_hash.record), record, tmp) {
            HASH_DEL((win_errhan_hash.record), record);
            free(record);
        }
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    CSP_ATTRIBUTE((unused));
    goto fn_exit;
}

/* ======================================================================
 * Other Public Utilities
 * ====================================================================== */

void CSPU_win_errhan_cache(MPI_Win win, MPI_Errhandler handler, MPI_Win_errhandler_function * fnc)
{
    win_errhan_hash_add(win, handler, fnc);
    CSP_DBG_PRINT("%s: cached win 0x%x->errhandler 0x%x & fnc %p\n", __FUNCTION__,
                  win, handler, fnc);
}

void CSPU_win_errhan_get(MPI_Win win, MPI_Errhandler * handler,
                         MPI_Win_errhandler_function ** fncptr)
{
    win_errhan_hash_get(win, handler, fncptr);
    CSP_DBG_PRINT("%s: got win 0x%x->errhandler 0x%x & fnc %p\n", __FUNCTION__,
                  win, *handler, *fncptr);
}

int CSPU_win_errhan_reset(MPI_Win win)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Errhandler errhandler = MPI_ERRHANDLER_NULL;
    MPI_Win_errhandler_function *errhandler_fnc = NULL;
    int reset_flag = 0;

    /* Remove cache record and get original handler.
     * Different from communicator, we do not cache the default handler
     * for every user window (see more note in CSPU_win_call_errhandler).*/
    win_errhan_hash_remove(win, &errhandler, &errhandler_fnc);
    CSP_ASSERT(errhandler == MPI_ERRHANDLER_NULL ||
               errhandler == MPI_ERRORS_ARE_FATAL ||
               errhandler == MPI_ERRORS_RETURN || errhandler_fnc != NULL);

    CSP_DBG_PRINT("%s: reset win 0x%x->errhandler 0x%x & fnc %p\n", __FUNCTION__,
                  win, errhandler, errhandler_fnc);

    /* Reset to user error handler in MPI if the handler is still valid. */
    if (errhandler != MPI_ERRHANDLER_NULL) {
        if (errhandler == MPI_ERRORS_ARE_FATAL || errhandler == MPI_ERRORS_RETURN) {
            reset_flag = 1;
        }
        else {
            errhandler_fnc = NULL;
            CSPU_errhan_get_fnc(errhandler, (void **) &errhandler_fnc);
            if (errhandler_fnc != NULL) /* no record if handler is freed. */
                reset_flag = 1;
        }

        if (reset_flag)
            CSP_CALLMPI(RETURN, PMPI_Win_set_errhandler(win, errhandler));
    }

    return mpi_errno;
}

/* Called at window call's error handling routine.
 * It triggers the error handler specified to the exposed window. */
int CSPU_win_call_errhandler(MPI_Win expwin, int *errcode)
{
    MPI_Errhandler handler = MPI_ERRHANDLER_NULL;
    MPI_Win_errhandler_function *fnc = NULL;
    int setflag = 0;
    int mpi_errno = MPI_SUCCESS;

    /* ensure external object is reset, thus no infinite recursion risk. */
    CSPU_ERRHAN_CHECK_EXTOBJ(&setflag);
    CSP_ASSERT(setflag == 0);

    win_errhan_hash_get(expwin, &handler, &fnc);

    /* Do do switch/case on handler because it might not be integer type in some
     * MPI implementations (e.g., openmpi). */
    if (handler == MPI_ERRHANDLER_NULL) {
        /* Different from communicator, standard does not specify whether a window
         * has default handler, and whether the default COMM_WORLD error handler is
         * triggered if no window handler is set. Thus we only wrap up a window error
         * handler when user explicitly sets it, otherwise just let MPI handle it.
         * - If the MPI implementation falls back to the handler in COMM_WORLD,
         *   it then triggers CSPU_comm_errhan_wrapper_fnc.
         * - No infinite recursion concern, because we have reset external error
         *   object, thus error handling executes there.*/
        CSP_CALLMPI(RETURN, PMPI_Win_call_errhandler(expwin, *errcode));
    }
    else if (handler == MPI_ERRORS_ARE_FATAL) {
        CSP_CALLMPI(RETURN, PMPI_Abort(MPI_COMM_WORLD, *errcode));
    }
    else if (handler == MPI_ERRORS_RETURN) {
        /* Do noting */
    }
    else {
        MPI_Win expwin_var;     /* to get valid object pointer */

        expwin_var = expwin;
        CSP_ASSERT(fnc != NULL);

        fnc(&expwin_var, errcode);
    }

    return mpi_errno;
}

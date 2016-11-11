/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSPU_ERRHAN_H_INCLUDED
#define CSPU_ERRHAN_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "csp.h"
#include "cspu_thread.h"

typedef enum {
    CSPU_ERRHAN_EXT_UNSET,
    CSPU_ERRHAN_EXT_COMM,
    CSPU_ERRHAN_EXT_WIN,
} CSPU_errhan_extflag_t;

extern CSPU_TLS_VAR_DCL(CSPU_errhan_extflag_t, CSPU_errhan_extflag);

extern int CSPU_errhan_init(void);
extern int CSPU_errhan_destroy(void);

extern void CSPU_errhan_cache_fnc(MPI_Errhandler handler, void *fnc);
extern void CSPU_errhan_get_fnc(MPI_Errhandler handler, void **fncptr);
extern void CSPU_errhan_remove_fnc(MPI_Errhandler handler);

/* Communicator error handler */
extern int CSPU_comm_errhan_init(void);
extern int CSPU_comm_errhan_destroy(void);
extern void CSPU_comm_errhan_wrapper_fnc(MPI_Comm * errcomm, int *errcode, ...);
extern void CSPU_comm_errhan_cache(MPI_Comm comm, MPI_Errhandler handler,
                                   MPI_Comm_errhandler_function * fnc);
extern void CSPU_comm_errhan_get(MPI_Comm comm, MPI_Errhandler * handler,
                                 MPI_Comm_errhandler_function ** fncptr);
extern int CSPU_comm_errhan_wrap(MPI_Comm comm, MPI_Errhandler errhandler,
                                 MPI_Comm_errhandler_function * errhandler_fnc);
extern int CSPU_comm_errhan_inherit(MPI_Comm comm, MPI_Comm newcomm);
extern int CSPU_comm_errhan_reset(MPI_Comm comm);
extern int CSPU_comm_call_errhandler(MPI_Comm expcomm, int *errcode);

/* Window error handler */
extern int CSPU_win_errhan_init(void);
extern int CSPU_win_errhan_destroy(void);
extern void CSPU_win_errhan_cache(MPI_Win win, MPI_Errhandler handler,
                                  MPI_Win_errhandler_function * fnc);
extern void CSPU_win_errhan_get(MPI_Win win, MPI_Errhandler * handler,
                                MPI_Win_errhandler_function ** fncptr);
extern int CSPU_win_errhan_reset(MPI_Win win);
extern int CSPU_win_call_errhandler(MPI_Win expwin, int *errcode);

/* Set RETURN error handler to internal communicator|window objects.*/
#define CSPU_COMM_ERRHAN_SET_INTERN(comm) do {                        \
    CSP_CALLMPI(JUMP, PMPI_Comm_set_errhandler(comm, MPI_ERRORS_RETURN)); \
} while (0)

#define CSPU_WIN_ERRHAN_SET_INTERN(win) do {                            \
    CSP_CALLMPI(JUMP, PMPI_Win_set_errhandler(win, MPI_ERRORS_RETURN)); \
} while (0)

/* External error object setting (used in every RMA call) */
#define CSPU_ERRHAN_EXTOBJ_LOCAL_DCL() CSPU_TLS_VCOPY_LOCAL_DCL(CSPU_errhan_extflag_t, errhan_extflag)

#define CSPU_WIN_ERRHAN_SET_EXTOBJ()                              \
    CSPU_TLS_VCOPY_SET(errhan_extflag, CSPU_errhan_extflag_t,     \
                       CSPU_ERRHAN_EXT_WIN,  CSPU_errhan_extflag)

#define CSPU_COMM_ERRHAN_SET_EXTOBJ()                              \
    CSPU_TLS_VCOPY_SET(errhan_extflag, CSPU_errhan_extflag_t,      \
                       CSPU_ERRHAN_EXT_COMM, CSPU_errhan_extflag);

#define CSPU_ERRHAN_RESET_EXTOBJ()                                \
    CSPU_TLS_VCOPY_RESET(CSPU_errhan_extflag_t,                   \
                         CSPU_ERRHAN_EXT_UNSET, CSPU_errhan_extflag)

#define CSPU_ERRHAN_CHECK_EXTOBJ(setflag_ptr) do {                            \
    CSPU_errhan_extflag_t extflag = CSPU_ERRHAN_EXT_UNSET;                    \
    CSPU_TLS_VCOPY_GET(CSPU_errhan_extflag_t, CSPU_errhan_extflag, &extflag); \
    *(setflag_ptr) = (extflag != CSPU_ERRHAN_EXT_UNSET);                      \
} while (0)

#define CSPU_COMM_ERRHANLDING(comm, errcode_ptr) CSPU_comm_call_errhandler(comm, errcode_ptr)
#define CSPU_WIN_ERRHANLDING(win, errcode_ptr) CSPU_win_call_errhandler(win, errcode_ptr)


#endif /* CSPU_ERRHAN_H_INCLUDED */

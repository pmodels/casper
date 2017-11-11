/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef CSP_DATATYPE_H_
#define CSP_DATATYPE_H_
#include <mpi.h>

/* ======================================================================
 * Datatype registration related routines.
 * We need datatype registration for communication offloading mechanism.
 * - Predefined datatype may have different value on each process, we need
 *   maintain a shared table to map predefined datatype between processes.
 * - Derived datatype object is locally committed on each process, we need
 *   also commit the same datatype on the ghost process.
 * Also see csp_offload.h.
 * ====================================================================== */

typedef enum {
    /* Named Predefined Datatypes (copied from MPI-3.1 report A.1.1) */
    CSP_DATATYPE_MPI_CHAR = 0,
    CSP_DATATYPE_MPI_SHORT,
    CSP_DATATYPE_MPI_INT,
    CSP_DATATYPE_MPI_LONG,
    CSP_DATATYPE_MPI_LONG_LONG_INT,
    CSP_DATATYPE_MPI_LONG_LONG,
    CSP_DATATYPE_MPI_SIGNED_CHAR,
    CSP_DATATYPE_MPI_UNSIGNED_CHAR,
    CSP_DATATYPE_MPI_UNSIGNED_SHORT,
    CSP_DATATYPE_MPI_UNSIGNED,
    CSP_DATATYPE_MPI_UNSIGNED_LONG,
    CSP_DATATYPE_MPI_UNSIGNED_LONG_LONG,
    CSP_DATATYPE_MPI_FLOAT,
    CSP_DATATYPE_MPI_DOUBLE,
    CSP_DATATYPE_MPI_LONG_DOUBLE,
    CSP_DATATYPE_MPI_WCHAR,

    CSP_DATATYPE_MPI_C_BOOL,
    CSP_DATATYPE_MPI_INT8_T,
    CSP_DATATYPE_MPI_INT16_T,
    CSP_DATATYPE_MPI_INT32_T,
    CSP_DATATYPE_MPI_INT64_T,
    CSP_DATATYPE_MPI_UINT8_T,
    CSP_DATATYPE_MPI_UINT16_T,
    CSP_DATATYPE_MPI_UINT32_T,
    CSP_DATATYPE_MPI_UINT64_T,
    CSP_DATATYPE_MPI_AINT,
    CSP_DATATYPE_MPI_COUNT,
    CSP_DATATYPE_MPI_OFFSET,
    CSP_DATATYPE_MPI_C_COMPLEX,
    CSP_DATATYPE_MPI_C_FLOAT_COMPLEX,
    CSP_DATATYPE_MPI_C_DOUBLE_COMPLEX,
    CSP_DATATYPE_MPI_C_LONG_DOUBLE_COMPLEX,
    CSP_DATATYPE_MPI_BYTE,
    CSP_DATATYPE_MPI_PACKED,

    CSP_DATATYPE_MPI_INTEGER,
    CSP_DATATYPE_MPI_REAL,
    CSP_DATATYPE_MPI_DOUBLE_PRECISION,
    CSP_DATATYPE_MPI_COMPLEX,
    CSP_DATATYPE_MPI_LOGICAL,
    CSP_DATATYPE_MPI_CHARACTER,

    CSP_DATATYPE_MPI_CXX_BOOL,
    CSP_DATATYPE_MPI_CXX_FLOAT_COMPLEX,
    CSP_DATATYPE_MPI_CXX_DOUBLE_COMPLEX,
    CSP_DATATYPE_MPI_CXX_LONG_DOUBLE_COMPLEX,

    CSP_DATATYPE_MPI_FLOAT_INT,
    CSP_DATATYPE_MPI_DOUBLE_INT,
    CSP_DATATYPE_MPI_LONG_INT,
    CSP_DATATYPE_MPI_2INT,
    CSP_DATATYPE_MPI_SHORT_INT,
    CSP_DATATYPE_MPI_LONG_DOUBLE_INT,

    CSP_DATATYPE_MPI_2REAL,
    CSP_DATATYPE_MPI_2DOUBLE_PRECISION,
    CSP_DATATYPE_MPI_2INTEGER,

    /* Optional datatypes */
#ifdef HAVE_MPI_DOUBLE_COMPLEX
    CSP_DATATYPE_MPI_DOUBLE_COMPLEX,
#endif
#ifdef HAVE_MPI_INTEGER1
    CSP_DATATYPE_MPI_INTEGER1,
#endif
#ifdef HAVE_MPI_INTEGER2
    CSP_DATATYPE_MPI_INTEGER2,
#endif
#ifdef HAVE_MPI_INTEGER4
    CSP_DATATYPE_MPI_INTEGER4,
#endif
#ifdef HAVE_MPI_INTEGER8
    CSP_DATATYPE_MPI_INTEGER8,
#endif
#ifdef HAVE_MPI_INTEGER16
    CSP_DATATYPE_MPI_INTEGER16,
#endif
#ifdef HAVE_MPI_REAL2
    CSP_DATATYPE_MPI_REAL2,
#endif
#ifdef HAVE_MPI_REAL4
    CSP_DATATYPE_MPI_REAL4,
#endif
#ifdef HAVE_MPI_REAL8
    CSP_DATATYPE_MPI_REAL8,
#endif
#ifdef HAVE_MPI_REAL16
    CSP_DATATYPE_MPI_REAL16,
#endif
#ifdef HAVE_MPI_COMPLEX4
    CSP_DATATYPE_MPI_COMPLEX4,
#endif
#ifdef HAVE_MPI_COMPLEX8
    CSP_DATATYPE_MPI_COMPLEX8,
#endif
#ifdef HAVE_MPI_COMPLEX16
    CSP_DATATYPE_MPI_COMPLEX16,
#endif
#ifdef HAVE_MPI_COMPLEX32
    CSP_DATATYPE_MPI_COMPLEX32,
#endif

    CSP_DATATYPE_MAX
} CSPU_datatype_predefine_id_t;

static inline void CSP_datatype_fill_predefined_table(MPI_Datatype * table)
{
    table[CSP_DATATYPE_MPI_CHAR] = MPI_CHAR;
    table[CSP_DATATYPE_MPI_SHORT] = MPI_SHORT;
    table[CSP_DATATYPE_MPI_INT] = MPI_INT;
    table[CSP_DATATYPE_MPI_LONG] = MPI_LONG;
    table[CSP_DATATYPE_MPI_LONG_LONG_INT] = MPI_LONG_LONG_INT;
    table[CSP_DATATYPE_MPI_LONG_LONG] = MPI_LONG_LONG;
    table[CSP_DATATYPE_MPI_SIGNED_CHAR] = MPI_SIGNED_CHAR;
    table[CSP_DATATYPE_MPI_UNSIGNED_CHAR] = MPI_UNSIGNED_CHAR;
    table[CSP_DATATYPE_MPI_UNSIGNED_SHORT] = MPI_UNSIGNED_SHORT;
    table[CSP_DATATYPE_MPI_UNSIGNED] = MPI_UNSIGNED;
    table[CSP_DATATYPE_MPI_UNSIGNED_LONG] = MPI_UNSIGNED_LONG;
    table[CSP_DATATYPE_MPI_UNSIGNED_LONG_LONG] = MPI_UNSIGNED_LONG_LONG;
    table[CSP_DATATYPE_MPI_FLOAT] = MPI_FLOAT;
    table[CSP_DATATYPE_MPI_DOUBLE] = MPI_DOUBLE;
    table[CSP_DATATYPE_MPI_LONG_DOUBLE] = MPI_LONG_DOUBLE;
    table[CSP_DATATYPE_MPI_WCHAR] = MPI_WCHAR;

    table[CSP_DATATYPE_MPI_C_BOOL] = MPI_C_BOOL;
    table[CSP_DATATYPE_MPI_INT8_T] = MPI_INT8_T;
    table[CSP_DATATYPE_MPI_INT16_T] = MPI_INT16_T;
    table[CSP_DATATYPE_MPI_INT32_T] = MPI_INT32_T;
    table[CSP_DATATYPE_MPI_INT64_T] = MPI_INT64_T;
    table[CSP_DATATYPE_MPI_UINT8_T] = MPI_UINT8_T;
    table[CSP_DATATYPE_MPI_UINT16_T] = MPI_UINT16_T;
    table[CSP_DATATYPE_MPI_UINT32_T] = MPI_UINT32_T;
    table[CSP_DATATYPE_MPI_UINT64_T] = MPI_UINT64_T;
    table[CSP_DATATYPE_MPI_AINT] = MPI_AINT;
    table[CSP_DATATYPE_MPI_COUNT] = MPI_COUNT;
    table[CSP_DATATYPE_MPI_OFFSET] = MPI_OFFSET;
    table[CSP_DATATYPE_MPI_C_COMPLEX] = MPI_C_COMPLEX;
    table[CSP_DATATYPE_MPI_C_FLOAT_COMPLEX] = MPI_C_FLOAT_COMPLEX;
    table[CSP_DATATYPE_MPI_C_DOUBLE_COMPLEX] = MPI_C_DOUBLE_COMPLEX;
    table[CSP_DATATYPE_MPI_C_LONG_DOUBLE_COMPLEX] = MPI_C_LONG_DOUBLE_COMPLEX;
    table[CSP_DATATYPE_MPI_BYTE] = MPI_BYTE;
    table[CSP_DATATYPE_MPI_PACKED] = MPI_PACKED;

    table[CSP_DATATYPE_MPI_INTEGER] = MPI_INTEGER;
    table[CSP_DATATYPE_MPI_REAL] = MPI_REAL;
    table[CSP_DATATYPE_MPI_DOUBLE_PRECISION] = MPI_DOUBLE_PRECISION;
    table[CSP_DATATYPE_MPI_COMPLEX] = MPI_COMPLEX;
    table[CSP_DATATYPE_MPI_LOGICAL] = MPI_LOGICAL;
    table[CSP_DATATYPE_MPI_CHARACTER] = MPI_CHARACTER;


    table[CSP_DATATYPE_MPI_CXX_BOOL] = MPI_CXX_BOOL;
    table[CSP_DATATYPE_MPI_CXX_FLOAT_COMPLEX] = MPI_CXX_FLOAT_COMPLEX;
    table[CSP_DATATYPE_MPI_CXX_DOUBLE_COMPLEX] = MPI_CXX_DOUBLE_COMPLEX;
    table[CSP_DATATYPE_MPI_CXX_LONG_DOUBLE_COMPLEX] = MPI_CXX_LONG_DOUBLE_COMPLEX;

    table[CSP_DATATYPE_MPI_FLOAT_INT] = MPI_FLOAT_INT;
    table[CSP_DATATYPE_MPI_DOUBLE_INT] = MPI_DOUBLE_INT;
    table[CSP_DATATYPE_MPI_LONG_INT] = MPI_LONG_INT;
    table[CSP_DATATYPE_MPI_2INT] = MPI_2INT;
    table[CSP_DATATYPE_MPI_SHORT_INT] = MPI_SHORT_INT;
    table[CSP_DATATYPE_MPI_LONG_DOUBLE_INT] = MPI_LONG_DOUBLE_INT;

    table[CSP_DATATYPE_MPI_2REAL] = MPI_2REAL;
    table[CSP_DATATYPE_MPI_2DOUBLE_PRECISION] = MPI_2DOUBLE_PRECISION;
    table[CSP_DATATYPE_MPI_2INTEGER] = MPI_2INTEGER;

    /* Optional datatypes */
#ifdef HAVE_MPI_DOUBLE_COMPLEX
    table[CSP_DATATYPE_MPI_DOUBLE_COMPLEX] = MPI_DOUBLE_COMPLEX;
#endif
#ifdef HAVE_MPI_INTEGER1
    table[CSP_DATATYPE_MPI_INTEGER1] = MPI_INTEGER1;
#endif
#ifdef HAVE_MPI_INTEGER2
    table[CSP_DATATYPE_MPI_INTEGER2] = MPI_INTEGER2;
#endif
#ifdef HAVE_MPI_INTEGER4
    table[CSP_DATATYPE_MPI_INTEGER4] = MPI_INTEGER4;
#endif
#ifdef HAVE_MPI_INTEGER8
    table[CSP_DATATYPE_MPI_INTEGER8] = MPI_INTEGER8;
#endif
#ifdef HAVE_MPI_INTEGER16
    table[CSP_DATATYPE_MPI_INTEGER16] = MPI_INTEGER16;
#endif
#ifdef HAVE_MPI_REAL2
    table[CSP_DATATYPE_MPI_REAL2] = MPI_REAL2;
#endif
#ifdef HAVE_MPI_REAL4
    table[CSP_DATATYPE_MPI_REAL4] = MPI_REAL4;
#endif
#ifdef HAVE_MPI_REAL8
    table[CSP_DATATYPE_MPI_REAL8] = MPI_REAL8;
#endif
#ifdef HAVE_MPI_REAL16
    table[CSP_DATATYPE_MPI_REAL16] = MPI_REAL16;
#endif
#ifdef HAVE_MPI_COMPLEX4
    table[CSP_DATATYPE_MPI_COMPLEX4] = MPI_COMPLEX4;
#endif
#ifdef HAVE_MPI_COMPLEX8
    table[CSP_DATATYPE_MPI_COMPLEX8] = MPI_COMPLEX8;
#endif
#ifdef HAVE_MPI_COMPLEX16
    table[CSP_DATATYPE_MPI_COMPLEX16] = MPI_COMPLEX16;
#endif
#ifdef HAVE_MPI_COMPLEX32
    table[CSP_DATATYPE_MPI_COMPLEX32] = MPI_COMPLEX32;
#endif
}
#endif /* CSP_DATATYPE_H_ */

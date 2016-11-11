/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#ifndef CSP_THREAD_H_INCLUDED
#define CSP_THREAD_H_INCLUDED

#include <pthread.h>

#ifdef CSP_THREAD_DEBUG
#define CSP_TH_DBG_PRINT(str,...) do { \
    fprintf(stdout, "[CSP-TH]"str, ## __VA_ARGS__); \
    fflush(stdout); \
    } while (0)
#else
#define CSP_TH_DBG_PRINT(str,...) do { } while (0)
#endif

/* ======================================================================
 * Critical section related routines.
 * ====================================================================== */
#define CSP_THREAD_CS_LOCK__PTHREAD_MUTEX 1
#define CSP_THREAD_CS_LOCK__IZEM_MCS_LOCK 2

#if (CSP_THREAD_CS_LOCK == CSP_THREAD_CS_LOCK__PTHREAD_MUTEX)
typedef struct {
    pthread_mutex_t mutex;
    unsigned short is_initialied;
} CSP_thread_cs_t;

static inline int CSP_thread_cs_init(CSP_thread_cs_t * cs_ptr)
{
    int err = 0;
    err = pthread_mutex_init(&(cs_ptr)->mutex, NULL);
    if (err == 0)
        cs_ptr->is_initialied = 1;
    return err;
}

static inline int CSP_thread_cs_destroy(CSP_thread_cs_t * cs_ptr)
{
    int err = 0;
    err = pthread_mutex_destroy(&(cs_ptr)->mutex);
    if (err == 0)
        cs_ptr->is_initialied = 0;
    return err;
}

#define CSP_THREAD_CS_LOCAL_CTX_DCL()
#define CSP_THREAD_CS_ENTER(cs_ptr)  pthread_mutex_lock(&(cs_ptr)->mutex)
#define CSP_THREAD_CS_EXIT(cs_ptr)  pthread_mutex_unlock(&(cs_ptr)->mutex)
#define CSP_THREAD_CS_IS_INITIALIZED(cs_ptr)  ((cs_ptr)->is_initialied == 1)

/* End of CSP_THREAD_CS_LOCK__PTHREAD_MUTEX */

#elif (CSP_THREAD_CS_LOCK == CSP_THREAD_CS_LOCK__IZEM_MCS_LOCK)
#include <lock/zm_mcs.h>

typedef struct {
    zm_mcs_t lock;
    unsigned short is_initialied;
} CSP_thread_cs_t;

static inline int CSP_thread_cs_init(CSP_thread_cs_t * cs_ptr)
{
    int err = 0;
    err = zm_mcs_init(&(cs_ptr)->lock);
    if (err == 0)
        cs_ptr->is_initialied = 1;
    return err;
}

static inline int CSP_thread_cs_destroy(CSP_thread_cs_t * cs_ptr)
{
    int err = 0;
    /* no destroy needed */
    cs_ptr->is_initialied = 0;
    return err;
}

#define CSP_THREAD_CS_LOCAL_CTX_DCL()  zm_mcs_qnode_t csp_thread_cs_local_ctx
#define CSP_THREAD_CS_ENTER(cs_ptr)  zm_mcs_acquire(&(cs_ptr)->lock, &csp_thread_cs_local_ctx)
#define CSP_THREAD_CS_EXIT(cs_ptr)  zm_mcs_release(&(cs_ptr)->lock, &csp_thread_cs_local_ctx)
#define CSP_THREAD_CS_IS_INITIALIZED(cs_ptr)  ((cs_ptr)->is_initialied == 1)

/* End of CSP_THREAD_CS_LOCK__IZEM_MCS_LOCK */
#endif


/* ======================================================================
 * TLS related routines.
 * ====================================================================== */
#if defined(CSP_TLS_SPECIFIER)

#define CSP_TLS_VAR_DCL(type, varname) __thread type varname

#define CSP_TLS_VAR_INIT(value, varname, threaded, errptr) do {           \
        varname = value;                                                  \
} while (0)

#define CSP_TLS_VAR_DESTROY(varname, threaded, errptr)

#define CSP_TLS_VCOPY_LOCAL_DCL(valtype, name)

#define CSP_TLS_VCOPY_SET(cpname, valtype, value, varname, threaded) do { \
        varname = value;                                                  \
} while (0)

#define CSP_TLS_VCOPY_GET(valtype, varname, threaded, valueptr) do {  \
        *(valueptr) = varname;                                        \
} while (0)

#define CSP_TLS_VCOPY_RESET(valtype, value, varname, threaded) do {   \
        varname = value;                                              \
} while (0)

#else
/* undefined CSP_TLS_SPECIFIER */

#include <csp_util.h>

/* Internal structures of TLS variables.
 * Do not access them directly. */
typedef struct CSP_thread_tlsvar_copy {
    UT_hash_handle hh;
    pthread_t key;
    void *valptr;
} CSP_thread_tls_vcopy_t;

typedef struct CSP_thread_tls_var {
    CSP_thread_cs_t cs;
    CSP_thread_tls_vcopy_t *copy;
} CSP_thread_tls_var_t;

/* Initialize a global TLS variable (Internal routine). */
static inline int tls_var_init(CSP_thread_tls_var_t * var, int threaded)
{
    int err = 0;
    if (threaded) {
        err = CSP_thread_cs_init(&var->cs);
    }
    return err;
}

/* Destroy a global TLS variable (Internal routine). */
static inline int tls_var_destroy(CSP_thread_tls_var_t * var, int threaded)
{
    int err = 0;
    if (threaded && CSP_THREAD_CS_IS_INITIALIZED(&var->cs)) {
        err = CSP_thread_cs_destroy(&var->cs);
    }
    return err;
}

/* Initialize a TLS variable copy when multiple threads exist (Internal routine).
 * The vcopy is stored in global variable as a hash record. */
static inline void tls_vcopy_init_threaded(CSP_thread_tls_vcopy_t * copy,
                                           CSP_thread_tls_var_t * varptr)
{
    int cs_err = 0;
    CSP_THREAD_CS_LOCAL_CTX_DCL();

    copy->key = pthread_self();

    cs_err = CSP_THREAD_CS_ENTER(&varptr->cs);
    CSP_ASSERT(cs_err == 0);

    HASH_ADD(hh, (varptr->copy), key, sizeof(pthread_t), copy);

    cs_err = CSP_THREAD_CS_EXIT(&varptr->cs);
    CSP_ASSERT(cs_err == 0);
    CSP_TH_DBG_PRINT("%s: copy %p, tid=0x%lx\n", __FUNCTION__, copy, copy->key);
}

/* Load a TLS variable copy when multiple threads exist (Internal routine).
 * Find the vcopy by using pthread id and return its pointer. */
static inline void tls_vcopy_load_threaded(CSP_thread_tls_var_t * varptr,
                                           CSP_thread_tls_vcopy_t ** copyptr)
{
    pthread_t tid;
    int cs_err = 0;
    CSP_THREAD_CS_LOCAL_CTX_DCL();

    tid = pthread_self();
    cs_err = CSP_THREAD_CS_ENTER(&varptr->cs);
    CSP_ASSERT(cs_err == 0);

    HASH_FIND(hh, (varptr->copy), &tid, sizeof(pthread_t), *copyptr);

    cs_err = CSP_THREAD_CS_EXIT(&varptr->cs);
    CSP_ASSERT(cs_err == 0);

    CSP_TH_DBG_PRINT("%s: copy %p, tid=0x%lx\n", __FUNCTION__, *copyptr, tid);
}

/* Release a TLS variable copy when multiple threads exist (Internal routine).
 * Remove this thread's vcopy from global variable hash. Note that, a given
 * thread(tid) should only has a single copy as a time. */
static inline void tls_vcopy_release_threaded(CSP_thread_tls_var_t * varptr)
{
    pthread_t tid;
    CSP_thread_tls_vcopy_t *copy = NULL;
    int cs_err = 0;
    CSP_THREAD_CS_LOCAL_CTX_DCL();

    tid = pthread_self();
    cs_err = CSP_THREAD_CS_ENTER(&varptr->cs);
    CSP_ASSERT(cs_err == 0);

    HASH_FIND(hh, (varptr->copy), &tid, sizeof(pthread_t), copy);
    if (copy != NULL)
        HASH_DEL((varptr->copy), copy);

    cs_err = CSP_THREAD_CS_EXIT(&varptr->cs);
    CSP_ASSERT(cs_err == 0);

    CSP_TH_DBG_PRINT("%s: copy %p, tid=0x%lx, cnt(hash)=%d\n",
                     __FUNCTION__, copy, tid, HASH_COUNT((varptr->copy)));
}

/* Name replacement MACROs (internal) */
#define TLS_VAR(varname) varname##_tlsvar
#define TLS_VCOPY(cpname) cpname##_tls_vcopy
#define TLS_VALOBJ(valname) valname##_tls_valobj

/* TLS variable accessing MACROs (public) */
#define CSP_TLS_VAR_DCL(valtype, varname) CSP_thread_tls_var_t TLS_VAR(varname)

#define CSP_TLS_VAR_INIT(value, varname, threaded, errptr) do { \
    *(errptr) = tls_var_init(&TLS_VAR(varname), threaded);      \
} while (0)

#define CSP_TLS_VAR_DESTROY(varname, threaded, errptr) do {     \
    *(errptr) = tls_var_destroy(&TLS_VAR(varname), threaded);   \
} while (0)

/* TLS variable accessing MACROs using local variable as the thread-private copy.
 *
 * Using local variable instead of dynamic allocation can reduce the overhead
 * of TLS routines in performance-sensitive path. This can be useful when
 * a thread wants to have private copy of a ``global variable`` that is only
 * accessed in a callback function of the current routine. However, using this
 * routine can be dangerous:
 * - accessing to the local copy is legal only during the local scope, since
 *   it is still valid in stack memory.
 * - must remove the copy from global hash before local scope end, otherwise
 *   hash can be crashed because of invalid memory access. */

/* Declare the local thread-private copy of a TLS variable.
 * To support any type of value, we declare a value object in the same scope,
 * and store the pointer of that object in copy.*/
#define CSP_TLS_VCOPY_LOCAL_DCL(valtype, cpname)                  \
    valtype TLS_VALOBJ(cpname);                                   \
    CSP_thread_tls_vcopy_t TLS_VCOPY(cpname);                     \
    TLS_VCOPY(cpname).valptr = (void *)(&TLS_VALOBJ(cpname));     \
    CSP_THREAD_CS_LOCAL_CTX_DCL();

/* Store a new local thread-private copy of a global TLS variable (hash)
 * using tid as the key. Note that, we do not know whether a thread joins, thus
 * the hash record for a given thread should be always removed at end of each
 * MPI call.*/
#define CSP_TLS_VCOPY_SET(cpname, valtype, value, varname, threaded)    \
do {                                                                    \
    *(valtype *)(TLS_VCOPY(cpname).valptr) = value;                     \
    if (threaded) {                                                     \
        tls_vcopy_init_threaded(&TLS_VCOPY(cpname), &TLS_VAR(varname)); \
    } else {                                                            \
        (TLS_VAR(varname)).copy = &TLS_VCOPY(cpname);                   \
    }                                                                   \
} while (0)

/* Load the value of thread-private copy from a global TLS variable (hash).
 * This can be called before the end of scope where that copy is declared
 * (e.g., in a callback function). A reset call must be always issued before
 * return to caller, thus get NULL after that. */
#define CSP_TLS_VCOPY_GET(valtype, varname, threaded, valueptr) do {       \
    if (threaded) {                                                        \
        CSP_thread_tls_vcopy_t *vcopy = NULL;                              \
        tls_vcopy_load_threaded(&TLS_VAR(varname), &vcopy);                \
        if (vcopy != NULL)                                                 \
            *(valueptr) = *(valtype *)(vcopy->valptr);                     \
    } else if (TLS_VAR(varname).copy != NULL) {                            \
        *(valueptr) = *(valtype *)(TLS_VAR(varname).copy->valptr);         \
    }                                                                      \
} while (0)

/* Reset the local thread-private copy of a global TLS variable (hash).*/
#define CSP_TLS_VCOPY_RESET(valtype, value, varname, threaded) do {        \
    if (threaded) {                                                        \
        tls_vcopy_release_threaded(&TLS_VAR(varname));                     \
    } else {                                                               \
        TLS_VAR(varname).copy = NULL;                                      \
    }                                                                      \
} while (0)

/* End of undefined CSP_TLS_SPECIFIER */
#endif

#endif /* CSP_THREAD_H_INCLUDED */

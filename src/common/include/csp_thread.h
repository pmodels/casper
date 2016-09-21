/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#ifndef CSP_THREAD_H_
#define CSP_THREAD_H_

#define CSP_THREAD_CS_LOCK__PTHREAD_MUTEX 1
#define CSP_THREAD_CS_LOCK__IZEM_MCS_LOCK 2

#if (CSP_THREAD_CS_LOCK == CSP_THREAD_CS_LOCK__PTHREAD_MUTEX)
#include <pthread.h>

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

#define CSP_THREAD_CS_LOCAL_CTX_DCL
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

#define CSP_THREAD_CS_LOCAL_CTX_DCL  zm_mcs_qnode_t csp_thread_cs_local_ctx
#define CSP_THREAD_CS_ENTER(cs_ptr)  zm_mcs_acquire(&(cs_ptr)->lock, &csp_thread_cs_local_ctx)
#define CSP_THREAD_CS_EXIT(cs_ptr)  zm_mcs_release(&(cs_ptr)->lock, &csp_thread_cs_local_ctx)
#define CSP_THREAD_CS_IS_INITIALIZED(cs_ptr)  ((cs_ptr)->is_initialied == 1)

/* End of CSP_THREAD_CS_LOCK__IZEM_MCS_LOCK */
#endif

#endif /* CSP_THREAD_H_ */

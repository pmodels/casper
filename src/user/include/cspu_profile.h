/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#ifndef CSPU_PROFILE_H_
#define CSPU_PROFILE_H_
#include "mpi.h"

#ifdef CSP_ENABLE_PROFILE

typedef enum CSPU_prof_rma_func {
    CSPU_PROF_RMA_FUNC_GET,
    CSPU_PROF_RMA_FUNC_PUT,
    CSPU_PROF_RMA_FUNC_ACCUMULATE,
    CSPU_PROF_RMA_FUNC_GET_ACCUMULATE,
    CSPU_PROF_RMA_FUNC_RGET,
    CSPU_PROF_RMA_FUNC_RPUT,
    CSPU_PROF_RMA_FUNC_RACCUMULATE,
    CSPU_PROF_RMA_FUNC_RGET_ACCUMULATE,
    CSPU_PROF_RMA_FUNC_FETCH_AND_OP,
    CSPU_PROF_RMA_FUNC_COMPARE_AND_SWAP,
    CSPU_PROF_RMA_MAX_NFUNC
} CSPU_prof_rma_func_t;

typedef enum CSPU_prof_pt2pt_func {
    CSPU_PROF_PT2PT_FUNC_ISEND,
    CSPU_PROF_PT2PT_FUNC_IRECV,
    CSPU_PROF_PT2PT_MAX_NFUNC
} CSPU_prof_pt2pt_func_t;

/* each function has a pair of <on, off> counters */
#define CSPU_PROF_COUNTER_OFFSET_ON 0
#define CSPU_PROF_COUNTER_OFFSET_OFF 1
extern int CSPU_prof_rma_counters[CSPU_PROF_RMA_MAX_NFUNC * 2];
extern int CSPU_prof_pt2pt_counters[CSPU_PROF_PT2PT_MAX_NFUNC * 2];

#define CSPU_PROF_RMA_COUNTER_INC(func, stat) do {                              \
        if (CSPU_PROF_RMA_FUNC_##func >= 0 && CSPU_PROF_RMA_FUNC_##func < CSPU_PROF_RMA_MAX_NFUNC)            \
                CSPU_prof_rma_counters[CSPU_PROF_RMA_FUNC_##func * 2 + CSPU_PROF_COUNTER_OFFSET_##stat]++;    \
    } while (0)
#define CSPU_PROF_PT2PT_COUNTER_INC(func, stat) do {                            \
        if (CSPU_PROF_PT2PT_FUNC_##func >= 0 && CSPU_PROF_PT2PT_FUNC_##func < CSPU_PROF_PT2PT_MAX_NFUNC)      \
                CSPU_prof_pt2pt_counters[CSPU_PROF_PT2PT_FUNC_##func * 2 + CSPU_PROF_COUNTER_OFFSET_##stat]++;\
    } while (0)
#define CSPU_PROF_EXT_COUNTER_INC(counter) (counter)++

static inline void CSPU_prof_rma_counter_reset(void)
{
    int i;
    for (i = 0; i < CSPU_PROF_RMA_MAX_NFUNC * 2; i++) {
        CSPU_prof_rma_counters[i] = 0;
    }
}

static inline void CSPU_prof_pt2pt_counter_reset(void)
{
    int i;
    for (i = 0; i < CSPU_PROF_PT2PT_MAX_NFUNC * 2; i++) {
        CSPU_prof_pt2pt_counters[i] = 0;
    }
}

extern void CSPU_prof_init(void);
extern void CSPU_prof_destroy(void);
extern int CSPU_prof_async_counter_print(void);
extern int CSPU_prof_ext_counter_print(int counter, const char *name);

#else
#define CSPU_PROF_RMA_COUNTER_INC(func,stat)
#define CSPU_PROF_PT2PT_COUNTER_INC(func,stat)
#define CSPU_PROF_EXT_COUNTER_INC(counter)

#define CSPU_prof_init()
#define CSPU_prof_destroy()
#define CSPU_prof_async_counter_print() (MPI_SUCCESS)
#define CSPU_prof_ext_counter_print(counter, name) (MPI_SUCCESS)
#endif

#endif /* CSPU_PROFILE_H_ */

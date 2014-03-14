#ifndef MPIASP_H_
#define MPIASP_H_

#include "aspconf.h"
#include <mpi.h>

#define DEBUG
#ifdef DEBUG
#define MPIASP_DBG_PRINT(str...) {fprintf(stdout, str);fflush(stdout);}
#else
#define MPIASP_DBG_PRINT(str...) {}
#endif

#define MPIASP_DBG_PRINT_FCNAME() MPIASP_DBG_PRINT("[MPIASP]in %s\n", __FUNCTION__);

typedef enum {
    MPIASP_FUNC_NULL,
    MPIASP_FUNC_WIN_ALLOCATE,
    MPIASP_FUNC_WIN_FREE,
    MPIASP_FUNC_LOCL_ALL,
    MPIASP_FUNC_UNLOCK_ALL,
    MPIASP_FUNC_ABORT,
    MPIASP_FUNC_FINALIZE,
    MPIASP_FUNC_MAX,
} MPIASP_Func;

typedef struct MPIASP_Win {
    MPI_Aint *all_base_asp_addrs;
    MPI_Aint *all_base_addrs;

    // communicator including local processe and ASP
    MPI_Comm shrd_comm;
    MPI_Win shrd_win;

    // communicator including all the user processes and ASP
    MPI_Comm ua_comm;
    // communicator including all the user processes
    MPI_Comm user_comm;

    void *base;
    MPI_Win win;
} MPIASP_Win;

typedef struct ASP_Func_info{
    MPIASP_Func FUNC;
    int nprocs;
} ASP_Func_info;

extern MPIASP_Win *ua_win_table[2];
static inline MPIASP_Win* get_ua_win(int handle) {
    if (ua_win_table[0]->win == handle) {
        return ua_win_table[0];
    } else {
        return NULL;
    }
}
static inline MPIASP_Win* remove_ua_win(int handle) {
    MPIASP_Win *ret;
    if (ua_win_table[0]->win == handle) {
        ret = ua_win_table[0];
        ua_win_table[0] = NULL;
        return ret;
    } else {
        return NULL;
    }
}
static inline void put_ua_win(int handle, MPIASP_Win* ua_win) {
    ua_win_table[0] = ua_win;
}

extern MPI_Comm MPIASP_COMM_USER;
extern int MPIASP_RANK_IN_COMM_WORLD;


static inline int MPIASP_Asp_initialized(void) {
    return MPIASP_RANK_IN_COMM_WORLD > -1;
}

static inline int MPIASP_IsASP(int rank) {
    return rank == MPIASP_RANK_IN_COMM_WORLD;
}

static inline int MPIASP_Comm_rank_isasp() {
    int rank;
    PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
    return rank == MPIASP_RANK_IN_COMM_WORLD;
}

/**
 * The root process in current user communicator ask ASP to start a new function
 */
static inline int MPIASP_Func_start(MPIASP_Func FUNC, int nprocs, int ua_tag) {
    ASP_Func_info info;
    info.FUNC = FUNC;
    info.nprocs = nprocs;

    return PMPI_Send(&info, sizeof(ASP_Func_info), MPI_CHAR, MPIASP_RANK_IN_COMM_WORLD, ua_tag,
            MPI_COMM_WORLD);
}


extern int run_asp_main(void);

#endif /* MPIASP_H_ */

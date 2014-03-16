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
    MPI_Aint *base_asp_addrs;
    MPI_Aint *base_addrs;
    MPI_Aint *sizes;
    int *disp_units;

    // communicator including local processe and ASP
    MPI_Comm shrd_comm;
    MPI_Win shrd_win;

    // communicator including all the user processes and ASP
    MPI_Comm ua_comm;
    int asp_rank;

    // communicator including all the user processes
    MPI_Comm user_comm;

    void *base;
    MPI_Win win;
} MPIASP_Win;

typedef struct ASP_Func_info {
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

    return PMPI_Send((char*)&info, sizeof(ASP_Func_info), MPI_CHAR,
            MPIASP_RANK_IN_COMM_WORLD, ua_tag, MPI_COMM_WORLD);
}

static inline int MPIASP_Tag_format(int user_tag) {
    int tag_ub, flag;
    void *v;
    /*
     * TODO: is there a better solution to get a unique tag ?
     */
    // Invalid tag ERROR If ((tag) < 0 || (tag) > MPIR_Process.attrs.tag_ub))
    if (user_tag < 0)
        user_tag = (~user_tag + 1);

    PMPI_Comm_get_attr(MPI_COMM_WORLD, MPI_TAG_UB, &v, &flag);
    if (!flag) {
        MPIASP_DBG_PRINT("Error: Cannot get MPI_TAG_UB\n");
        return -1;
    }

    tag_ub = *(int*)v;
    MPIASP_DBG_PRINT("tag_ub=%d\n", tag_ub);

    user_tag = user_tag & tag_ub;
    return user_tag;
}

extern int run_asp_main(void);

extern int MPIASP_Init(int *argc, char ***argv);
extern int MPIASP_Finalize(void);
extern int MPIASP_Abort(MPI_Comm comm, int errorcode);
extern int MPIASP_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm);

extern int MPIASP_Win_allocate(MPI_Aint size, int disp_unit, MPI_Info info,
		MPI_Comm user_comm, void *baseptr, MPI_Win *win);
extern int MPIASP_Win_create(void *base, MPI_Aint size, int disp_unit,
		MPI_Info info, MPI_Comm comm, MPI_Win *win);
extern int MPIASP_Win_free(MPI_Win *win);

extern int MPIASP_Put(const void *origin_addr, int origin_count,
		MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp,
		int target_count, MPI_Datatype target_datatype, MPI_Win win);
extern int MPIASP_Get(void *origin_addr, int origin_count,
		MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp,
		int target_count, MPI_Datatype target_datatype, MPI_Win win);
extern int MPIASP_Accumulate(const void *origin_addr, int origin_count,
		MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp,
		int target_count, MPI_Datatype target_datatype, MPI_Op op, MPI_Win win);

#endif /* MPIASP_H_ */

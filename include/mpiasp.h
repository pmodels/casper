#ifndef MPIASP_H_
#define MPIASP_H_

#include <stdio.h>
#include <mpi.h>

//#define DEBUG
#ifdef DEBUG
#define MPIASP_DBG_PRINT(str...) do{fprintf(stdout, "[MPIASP]"str);fflush(stdout);}while(0)
#else
#define MPIASP_DBG_PRINT(str...) {}
#endif

#define MPIASP_DBG_PRINT_FCNAME() MPIASP_DBG_PRINT("in %s\n", __FUNCTION__)
#define MPIASP_ERR_PRINT(str...) do{fprintf(stderr, str);fflush(stdout);}while(0)

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

typedef struct ASP_Win_params {
    MPI_Aint size;
    int disp_unit;
} ASP_Win_params;

typedef struct MPIASP_Win {
    MPI_Aint *base_asp_offset;
    int *disp_units;

    // communicator including local process and ASP
    MPI_Comm local_ua_comm;
    MPI_Win local_ua_win;
    ASP_Win_params *local_ua_win_param;

    // communicator including all the user processes and ASP
    MPI_Comm ua_comm;
    int *asp_ranks_in_ua;

    // communicator including all the user processes
    MPI_Comm user_comm;
    int *user_ranks_in_world;
    int *user_ranks_in_user_world;

    MPI_Comm local_user_comm;

    void *base;
    MPI_Win win;

    int num_nodes;
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

extern MPI_Comm MPIASP_COMM_USER_WORLD;
extern MPI_Comm MPIASP_COMM_LOCAL;
extern MPI_Comm MPIASP_COMM_USER_LOCAL;
extern MPI_Comm MPIASP_COMM_USER_ROOTS;

extern int MPIASP_NUM_ASP_IN_LOCAL;
extern int MPIASP_RANK_IN_COMM_WORLD;
extern int MPIASP_RANK_IN_COMM_LOCAL;
extern int *MPIASP_ALL_ASP_IN_COMM_WORLD;
extern int MPIASP_NUM_UNIQUE_ASP;
extern int MPIASP_MY_NODE_ID;
extern int *MPIASP_ALL_NODE_IDS;

static inline int MPIASP_Asp_initialized(void) {
    return MPIASP_RANK_IN_COMM_WORLD > -1;
}

/**
 * The root process in current local user communicator ask ASP to start a new function
 */
static inline int MPIASP_Func_start(MPIASP_Func FUNC, int nprocs, int ua_tag,
        MPI_Comm user_local_comm) {
    ASP_Func_info info;
    int local_user_rank;
    info.FUNC = FUNC;
    info.nprocs = nprocs;

    PMPI_Comm_rank(user_local_comm, &local_user_rank);
    if (local_user_rank == 0) {
        return PMPI_Send((char*) &info, sizeof(ASP_Func_info), MPI_CHAR,
                MPIASP_RANK_IN_COMM_LOCAL, ua_tag, MPIASP_COMM_LOCAL);
    }
    return 0;
}

static inline int MPIASP_Func_set_param(char *func_params, int size, int ua_tag,
        MPI_Comm user_local_comm) {
    int local_user_rank, i;
    int mpi_errno = MPI_SUCCESS;

    PMPI_Comm_rank(user_local_comm, &local_user_rank);
    if (local_user_rank == 0) {
        mpi_errno = PMPI_Send(func_params, size, MPI_CHAR,
                MPIASP_RANK_IN_COMM_LOCAL, ua_tag, MPIASP_COMM_LOCAL);
    }

    return mpi_errno;
}

static inline int MPIASP_Tag_format(int org_tag, int *tag) {
    int mpi_errno = MPI_SUCCESS;
    int tag_ub, flag;
    void *v;
    /*
     * TODO: is there a better solution to get a unique tag ?
     */
    // Invalid tag ERROR If ((tag) < 0 || (tag) > MPIR_Process.attrs.tag_ub))
    if (org_tag < 0)
        org_tag = (~org_tag + 1);

    PMPI_Comm_get_attr(MPI_COMM_WORLD, MPI_TAG_UB, &v, &flag);
    if (!flag) {
        MPIASP_DBG_PRINT("Error: Cannot get MPI_TAG_UB\n");
        return -1;
    }

    tag_ub = *(int*)v;
    MPIASP_DBG_PRINT("tag_ub=%d\n", tag_ub);

    *tag = org_tag & tag_ub;
    return mpi_errno;
}

extern int run_asp_main(void);

#endif /* MPIASP_H_ */

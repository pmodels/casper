#ifndef ASP_H_
#define ASP_H_

#include "aspconf.h"
#include <mpi.h>
#include "mpiasp.h"

typedef struct ASP_Win {
    // communicators with each user processes
    MPI_Comm *all_shrd_comms;
    MPI_Win *all_shrd_wins;
    MPI_Aint *all_shrd_base_addrs;

    // communicator including all the user processes and ASP
    MPI_Comm ua_comm;

    void *base;
    MPI_Win win;
} ASP_Win;

extern ASP_Win *asp_win_table[2];
static inline ASP_Win* get_asp_win(int handle) {
    return asp_win_table[0];
}
static inline ASP_Win* remove_asp_win(int handle) {
    ASP_Win *ret;
    if (asp_win_table[0]) {
        ret = asp_win_table[0];
        asp_win_table[0] = NULL;
        return ret;
    } else {
        return NULL;
    }
}
static inline void put_asp_win(int handle, ASP_Win* win) {
    asp_win_table[0] = win;
}

/**
 * ASP receives a new function from user root process
 */
static inline int ASP_Func_start(MPIASP_Func *FUNC, int *root, int *nprocs,
        int *ua_tag) {
    int mpi_errno = MPI_SUCCESS;
    MPI_Status status;
    ASP_Func_info info;

    mpi_errno = PMPI_Recv((char*)&info, sizeof(ASP_Func_info), MPI_CHAR,
            MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

    *FUNC = info.FUNC;
    *nprocs = info.nprocs;
    *root = status.MPI_SOURCE;
    *ua_tag = status.MPI_TAG;

    return mpi_errno;
}

extern int ASP_Win_allocate(int user_root, int user_nprocs, int user_tag);
extern int ASP_Win_free(int user_root, int user_nprocs, int user_tag);

#endif /* ASP_H_ */

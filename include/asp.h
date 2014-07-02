#ifndef ASP_H_
#define ASP_H_

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "mpiasp.h"
#include "hash_table.h"

//#define ASP_DEBUG
#ifdef ASP_DEBUG
#define ASP_DBG_PRINT(str, ...) do { \
    fprintf(stdout, "[ASP][N-%d]"str, MPIASP_MY_NODE_ID, ## __VA_ARGS__); fflush(stdout); \
    } while (0)
#else
#define ASP_DBG_PRINT(str, ...) {}
#endif

#define ASP_ERR_PRINT(str...) do {fprintf(stderr, str);fflush(stdout);} while (0)

#define ASP_Assert(EXPR) do { if (unlikely(!(EXPR))){ \
            ASP_ERR_PRINT("[ASP][N-%d, %d]  assert fail in [%s:%d]: \"%s\"\n", \
                    MPIASP_MY_NODE_ID, MPIASP_MY_RANK_IN_WORLD, __FILE__, __LINE__, #EXPR); \
            PMPI_Abort(MPI_COMM_WORLD, -1); \
        }} while (0)

typedef struct ASP_Win {
    MPI_Aint *user_base_addrs_in_local;

    // communicator including local processe and ASP
    MPI_Comm local_ua_comm;
    MPI_Win local_ua_win;

    // communicator including all the user processes and ASP
    MPI_Comm ua_comm;

    void *base;
    MPI_Win *ua_wins;
    unsigned long asp_win_handle;
} ASP_Win;

extern hashtable_t *asp_win_ht;
#define ASP_WIN_HT_SIZE 256

static inline int get_asp_win(unsigned long handle, ASP_Win ** win)
{
    *win = (ASP_Win *) ht_get(asp_win_ht, (ht_key_t) handle);
    return 0;
}

static inline int put_asp_win(unsigned long key, ASP_Win * win)
{
    return ht_set(asp_win_ht, (ht_key_t) key, win);
}

static inline int init_asp_win_table()
{
    asp_win_ht = ht_create(ASP_WIN_HT_SIZE);

    if (asp_win_ht == NULL)
        return -1;

    return 0;
}

static inline void destroy_asp_win_table()
{
    return ht_destroy(asp_win_ht);
}

/**
 * ASP receives a new function from user root process
 */
static inline int ASP_Func_start(MPIASP_Func * FUNC, int *local_root, int *user_nprocs,
                                 int *user_local_nprocs, int *ua_tag)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Status status;
    ASP_Func_info info;

    mpi_errno = PMPI_Recv((char *) &info, sizeof(ASP_Func_info), MPI_CHAR,
                          MPI_ANY_SOURCE, MPI_ANY_TAG, MPIASP_COMM_LOCAL, &status);

    *FUNC = info.FUNC;
    *user_nprocs = info.user_nprocs;
    *user_local_nprocs = info.user_local_nprocs;
    *local_root = status.MPI_SOURCE;
    *ua_tag = status.MPI_TAG;

    return mpi_errno;
}

static inline int MPIASP_Func_get_param(char *func_params, int size,
                                        int user_local_root, int ua_tag)
{
    int local_user_rank, i;
    MPI_Status status;
    int mpi_errno = MPI_SUCCESS;

    return PMPI_Recv(func_params, size, MPI_CHAR, user_local_root, ua_tag,
                     MPIASP_COMM_LOCAL, &status);
}

extern int ASP_Win_allocate(int user_local_root, int user_nprocs, int user_local_nprocs,
                            int user_tag);
extern int ASP_Win_free(int user_local_root, int user_nprocs, int user_local_nprocs, int user_tag);

extern int ASP_Finalize(void);

#endif /* ASP_H_ */

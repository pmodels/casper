#ifndef MTCORE_H_
#define MTCORE_H_

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "hash_table.h"

#define MTCORE_ENABLE_GRANT_LOCK_HIDDEN_BYTE
#define MTCORE_ENABLE_LOCAL_LOCK_OPT /* Optimization for local target.
                                        Lock/RMA/Flush/Unlock local target instead of helpers.
                                        Only available when local lock is granted. */

#ifdef MTCORE_ENABLE_GRANT_LOCK_HIDDEN_BYTE
#define MTCORE_GRANT_LOCK_DATATYPE char
#define MTCORE_GRANT_LOCK_MPI_DATATYPE MPI_CHAR
#endif

#ifdef HAVE_BUILTIN_EXPECT
#  define unlikely(x_) __builtin_expect(!!(x_),0)
#  define likely(x_)   __builtin_expect(!!(x_),1)
#else
#  define unlikely(x_) (x_)
#  define likely(x_)   (x_)
#endif

/* #define DEBUG */
#ifdef DEBUG
#define MTCORE_DBG_PRINT(str...) do {fprintf(stdout, "[MTCORE]"str);fflush(stdout);} while (0)
#else
#define MTCORE_DBG_PRINT(str...) {}
#endif

#define MTCORE_DBG_PRINT_FCNAME() MTCORE_DBG_PRINT("in %s\n", __FUNCTION__)
#define MTCORE_ERR_PRINT(str...) do {fprintf(stderr, str);fflush(stdout);} while (0)

#define MTCORE_Assert(EXPR) do { if (unlikely(!(EXPR))){ \
            MTCORE_ERR_PRINT("[MTCORE][N-%d, %d]  assert fail in [%s:%d]: \"%s\"\n", \
                    MTCORE_MY_NODE_ID, MTCORE_MY_RANK_IN_WORLD, __FILE__, __LINE__, #EXPR); \
            PMPI_Abort(MPI_COMM_WORLD, -1); \
        }} while (0)

typedef enum {
    MTCORE_FUNC_NULL,
    MTCORE_FUNC_WIN_ALLOCATE,
    MTCORE_FUNC_WIN_FREE,
    MTCORE_FUNC_LOCL_ALL,
    MTCORE_FUNC_UNLOCK_ALL,
    MTCORE_FUNC_ABORT,
    MTCORE_FUNC_FINALIZE,
    MTCORE_FUNC_MAX,
} MTCORE_Func;

typedef struct MTCORE_H_win_params {
    MPI_Aint size;
    int disp_unit;
} MTCORE_H_win_params;

typedef struct MTCORE_Win {
    MPI_Aint *base_h_offsets;
    int *disp_units;

    /* communicator including local process and helpers */
    MPI_Comm local_uh_comm;
    MPI_Group local_uh_group;
    MPI_Win local_uh_win;
    MTCORE_H_win_params *local_uh_win_param;

    /* communicator including all the user processes and helpers */
    MPI_Comm uh_comm;
    MPI_Group uh_group;
    int *h_ranks_in_uh;
    MPI_Win *uh_wins;           /* every local process has separate window for permission control,
                                 * processes in different node share one window */

    /* communicator including all the user processes */
    MPI_Comm user_comm;
    MPI_Group user_group;

    MPI_Comm local_user_comm;
    int max_local_user_nprocs;
    int *local_user_ranks;      /* ranks in local user communicator,
                                 * gathered in win_allocate and used in lock_all/flush_all */

    void *base;
    MPI_Win win;

    int num_nodes;
    unsigned long h_win_handle;

#ifdef MTCORE_ENABLE_GRANT_LOCK_HIDDEN_BYTE
    MPI_Aint grant_lock_h_offset;     /* Hidden byte for granting lock on Helper0 */
#endif

    unsigned short is_self_lock_grant_required;
#ifdef MTCORE_ENABLE_LOCAL_LOCK_OPT
    unsigned short is_self_locked;
#endif
} MTCORE_Win;

typedef struct MTCORE_Func_info {
    MTCORE_Func FUNC;
    int user_nprocs;
    int user_local_nprocs;
} MTCORE_Func_info;

extern hashtable_t *uh_win_ht;
#define uh_win_HT_SIZE 256

static inline int get_uh_win(int handle, MTCORE_Win ** win)
{
    *win = (MTCORE_Win *) ht_get(uh_win_ht, (ht_key_t) handle);
    return 0;
}

static inline int put_uh_win(int key, MTCORE_Win * win)
{
    return ht_set(uh_win_ht, (ht_key_t) key, win);
}

static inline int init_uh_win_table()
{
    uh_win_ht = ht_create(uh_win_HT_SIZE);

    if (uh_win_ht == NULL)
        return -1;

    return 0;
}

static inline void destroy_uh_win_table()
{
    return ht_destroy(uh_win_ht);
}

extern MPI_Comm MTCORE_COMM_USER_WORLD;
extern MPI_Comm MTCORE_COMM_LOCAL;
extern MPI_Comm MTCORE_COMM_USER_LOCAL;
extern MPI_Comm MTCORE_COMM_USER_ROOTS;
extern MPI_Group MTCORE_GROUP_WORLD;
extern MPI_Group MTCORE_GROUP_LOCAL;

extern int MTCORE_NUM_H_IN_LOCAL;
extern int MTCORE_RANK_IN_COMM_WORLD;
extern int MTCORE_RANK_IN_COMM_LOCAL;
extern int *MTCORE_ALL_H_IN_COMM_WORLD;
extern int MTCORE_NUM_NODES;
extern int MTCORE_MY_NODE_ID;
extern int *MTCORE_ALL_NODE_IDS;
extern int MTCORE_MY_RANK_IN_WORLD;

/**
 * The root process in current local user communicator ask helper to start a new function
 */
static inline int MTCORE_Func_start(MTCORE_Func FUNC, int user_nprocs, int user_local_nprocs,
                                    int uh_tag, MPI_Comm user_local_comm)
{
    MTCORE_Func_info info;
    int local_user_rank;
    info.FUNC = FUNC;
    info.user_nprocs = user_nprocs;
    info.user_local_nprocs = user_local_nprocs;

    PMPI_Comm_rank(user_local_comm, &local_user_rank);
    if (local_user_rank == 0) {
        return PMPI_Send((char *) &info, sizeof(MTCORE_Func_info), MPI_CHAR,
                         MTCORE_RANK_IN_COMM_LOCAL, uh_tag, MTCORE_COMM_LOCAL);
    }
    return 0;
}

static inline int MTCORE_Func_set_param(char *func_params, int size, int uh_tag,
                                        MPI_Comm user_local_comm)
{
    int local_user_rank;
    int mpi_errno = MPI_SUCCESS;

    PMPI_Comm_rank(user_local_comm, &local_user_rank);
    if (local_user_rank == 0) {
        mpi_errno = PMPI_Send(func_params, size, MPI_CHAR,
                              MTCORE_RANK_IN_COMM_LOCAL, uh_tag, MTCORE_COMM_LOCAL);
    }

    return mpi_errno;
}

static inline int MTCORE_Tag_format(int org_tag, int *tag)
{
    int mpi_errno = MPI_SUCCESS;
    int tag_ub, flag;
    void *v;
    /*
     * TODO: is there a better solution to get a unique tag ?
     */
    /* Invalid tag ERROR If ((tag) < 0 || (tag) > MPIR_Process.attrs.tag_ub)) */
    if (org_tag < 0)
        org_tag = (~org_tag + 1);

    PMPI_Comm_get_attr(MPI_COMM_WORLD, MPI_TAG_UB, &v, &flag);
    if (!flag) {
        MTCORE_DBG_PRINT("Error: Cannot get MPI_TAG_UB\n");
        return -1;
    }

    tag_ub = *(int *) v;
    *tag = org_tag & tag_ub;

    MTCORE_DBG_PRINT("tag_ub=%d\n", *tag);

    return mpi_errno;
}


static inline int MTCORE_Get_node_ids(MPI_Group group, int n, const int ranks[], int node_ids[])
{
    int mpi_errno = MPI_SUCCESS;
    int *ranks_in_world = NULL;
    int i;

    if (n == 0)
        return mpi_errno;

    ranks_in_world = calloc(n, sizeof(int));

    mpi_errno = PMPI_Group_translate_ranks(group, n, ranks, MTCORE_GROUP_WORLD, ranks_in_world);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    for (i = 0; i < n; i++) {
        node_ids[i] = MTCORE_ALL_NODE_IDS[ranks_in_world[i]];
    }

  fn_exit:
    if (ranks_in_world)
        free(ranks_in_world);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static inline int MTCORE_Is_in_shrd_mem(int target_rank, MPI_Group group, int *node_id,
                                        int *is_shared)
{
    int mpi_errno = MPI_SUCCESS;
    int target_node_id = -1;
    *is_shared = 0;

    /* If target is in the same node, use shared window instead */
    mpi_errno = MTCORE_Get_node_ids(group, 1, &target_rank, &target_node_id);
    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;

    if (target_node_id == MTCORE_ALL_NODE_IDS[MTCORE_MY_RANK_IN_WORLD]) {
        *is_shared = 1;
    }

    *node_id = target_node_id;

    return mpi_errno;
}

static inline int MTCORE_Win_grant_local_lock(int lock_type, int assert, MTCORE_Win * uh_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank, local_user_rank, node_id;

    PMPI_Comm_rank(uh_win->user_comm, &user_rank);
    PMPI_Comm_rank(uh_win->local_user_comm, &local_user_rank);
    node_id = MTCORE_ALL_NODE_IDS[MTCORE_MY_RANK_IN_WORLD];

#ifdef MTCORE_ENABLE_GRANT_LOCK_HIDDEN_BYTE
    MTCORE_GRANT_LOCK_DATATYPE buf[1];
    mpi_errno = PMPI_Get(buf, 1, MTCORE_GRANT_LOCK_MPI_DATATYPE, uh_win->h_ranks_in_uh[node_id],
                         uh_win->grant_lock_h_offset, 1, MTCORE_GRANT_LOCK_MPI_DATATYPE,
                         uh_win->uh_wins[local_user_rank]);
#else
    /* Simply get 1 byte from start, it does not affect the result of other updates */
    char buf[1];
    mpi_errno = PMPI_Get(buf, 1, MPI_CHAR, uh_win->h_ranks_in_uh[node_id], 0,
                         1, MPI_CHAR, uh_win->uh_wins[local_user_rank]);
#endif
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    mpi_errno = PMPI_Win_flush(uh_win->h_ranks_in_uh[node_id], uh_win->uh_wins[local_user_rank]);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    MTCORE_DBG_PRINT("[%d]grant local lock(Helper(%d), uh_wins[%d])\n", user_rank,
                     uh_win->h_ranks_in_uh[node_id], local_user_rank);
  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

extern int run_h_main(void);

#endif /* MTCORE_H_ */

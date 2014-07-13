#ifndef MPIASP_H_
#define MPIASP_H_

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "hash_table.h"

#define MTCORE_ENABLE_GRANT_LOCK_HIDDEN_BYTE

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

//#define DEBUG
#ifdef DEBUG
#define MPIASP_DBG_PRINT(str...) do {fprintf(stdout, "[MPIASP]"str);fflush(stdout);} while (0)
#else
#define MPIASP_DBG_PRINT(str...) {}
#endif

#define MPIASP_DBG_PRINT_FCNAME() MPIASP_DBG_PRINT("in %s\n", __FUNCTION__)
#define MPIASP_ERR_PRINT(str...) do {fprintf(stderr, str);fflush(stdout);} while (0)

#define MPIASP_Assert(EXPR) do { if (unlikely(!(EXPR))){ \
            MPIASP_ERR_PRINT("[MPIASP][N-%d, %d]  assert fail in [%s:%d]: \"%s\"\n", \
                    MPIASP_MY_NODE_ID, MPIASP_MY_RANK_IN_WORLD, __FILE__, __LINE__, #EXPR); \
            PMPI_Abort(MPI_COMM_WORLD, -1); \
        }} while (0)

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
    MPI_Group local_ua_group;
    MPI_Win local_ua_win;
    ASP_Win_params *local_ua_win_param;

    // communicator including all the user processes and ASP
    MPI_Comm ua_comm;
    MPI_Group ua_group;
    int *asp_ranks_in_ua;
    MPI_Win *ua_wins;           // every target has separate window for permission control

    // communicator including all the user processes
    MPI_Comm user_comm;
    MPI_Group user_group;

    MPI_Comm local_user_comm;

    void *base;
    MPI_Win win;

    int num_nodes;
    unsigned long asp_win_handle;

#ifdef MTCORE_ENABLE_GRANT_LOCK_HIDDEN_BYTE
    MPI_Aint grant_lock_asp_offset;     /* Hidden byte for granting lock on Helper0 */
#endif

    unsigned short is_self_lock_grant_required;
    unsigned short is_self_locked;
} MPIASP_Win;

typedef struct ASP_Func_info {
    MPIASP_Func FUNC;
    int user_nprocs;
    int user_local_nprocs;
} ASP_Func_info;

extern hashtable_t *ua_win_ht;
#define UA_WIN_HT_SIZE 256

static inline int get_ua_win(int handle, MPIASP_Win ** win)
{
    *win = (MPIASP_Win *) ht_get(ua_win_ht, (ht_key_t) handle);
    return 0;
}

static inline int put_ua_win(int key, MPIASP_Win * win)
{
    return ht_set(ua_win_ht, (ht_key_t) key, win);
}

static inline int init_ua_win_table()
{
    ua_win_ht = ht_create(UA_WIN_HT_SIZE);

    if (ua_win_ht == NULL)
        return -1;

    return 0;
}

static inline void destroy_ua_win_table()
{
    return ht_destroy(ua_win_ht);
}

extern MPI_Comm MPIASP_COMM_USER_WORLD;
extern MPI_Comm MPIASP_COMM_LOCAL;
extern MPI_Comm MPIASP_COMM_USER_LOCAL;
extern MPI_Comm MPIASP_COMM_USER_ROOTS;
extern MPI_Group MPIASP_GROUP_WORLD;
extern MPI_Group MPIASP_GROUP_LOCAL;

extern int MPIASP_NUM_ASP_IN_LOCAL;
extern int MPIASP_RANK_IN_COMM_WORLD;
extern int MPIASP_RANK_IN_COMM_LOCAL;
extern int *MPIASP_ALL_ASP_IN_COMM_WORLD;
extern int MPIASP_NUM_NODES;
extern int MPIASP_MY_NODE_ID;
extern int *MPIASP_ALL_NODE_IDS;
extern int MPIASP_MY_RANK_IN_WORLD;

static inline int MPIASP_Asp_initialized(void)
{
    return MPIASP_RANK_IN_COMM_WORLD > -1;
}

/**
 * The root process in current local user communicator ask ASP to start a new function
 */
static inline int MPIASP_Func_start(MPIASP_Func FUNC, int user_nprocs, int user_local_nprocs,
                                    int ua_tag, MPI_Comm user_local_comm)
{
    ASP_Func_info info;
    int local_user_rank;
    info.FUNC = FUNC;
    info.user_nprocs = user_nprocs;
    info.user_local_nprocs = user_local_nprocs;

    PMPI_Comm_rank(user_local_comm, &local_user_rank);
    if (local_user_rank == 0) {
        return PMPI_Send((char *) &info, sizeof(ASP_Func_info), MPI_CHAR,
                         MPIASP_RANK_IN_COMM_LOCAL, ua_tag, MPIASP_COMM_LOCAL);
    }
    return 0;
}

static inline int MPIASP_Func_set_param(char *func_params, int size, int ua_tag,
                                        MPI_Comm user_local_comm)
{
    int local_user_rank;
    int mpi_errno = MPI_SUCCESS;

    PMPI_Comm_rank(user_local_comm, &local_user_rank);
    if (local_user_rank == 0) {
        mpi_errno = PMPI_Send(func_params, size, MPI_CHAR,
                              MPIASP_RANK_IN_COMM_LOCAL, ua_tag, MPIASP_COMM_LOCAL);
    }

    return mpi_errno;
}

static inline int MPIASP_Tag_format(int org_tag, int *tag)
{
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

    tag_ub = *(int *) v;
    *tag = org_tag & tag_ub;

    MPIASP_DBG_PRINT("tag_ub=%d\n", *tag);

    return mpi_errno;
}


static inline int MPIASP_Get_node_ids(MPI_Group group, int n, const int ranks[], int node_ids[])
{
    int mpi_errno = MPI_SUCCESS;
    int *ranks_in_world = NULL;
    int i;

    if (n == 0)
        return mpi_errno;

    ranks_in_world = calloc(n, sizeof(int));

    mpi_errno = PMPI_Group_translate_ranks(group, n, ranks, MPIASP_GROUP_WORLD, ranks_in_world);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    for (i = 0; i < n; i++) {
        node_ids[i] = MPIASP_ALL_NODE_IDS[ranks_in_world[i]];
    }

  fn_exit:
    if (ranks_in_world)
        free(ranks_in_world);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static inline int MPIASP_Is_in_shrd_mem(int target_rank, MPI_Group group, int *node_id,
                                        int *is_shared)
{
    int mpi_errno = MPI_SUCCESS;
    int target_node_id = -1;
    *is_shared = 0;

    // If target is in the same node, use shared window instead
    mpi_errno = MPIASP_Get_node_ids(group, 1, &target_rank, &target_node_id);
    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;

    if (target_node_id == MPIASP_ALL_NODE_IDS[MPIASP_MY_RANK_IN_WORLD]) {
        *is_shared = 1;
    }

    *node_id = target_node_id;

    return mpi_errno;
}

static inline int MPIASP_Win_grant_local_lock(int lock_type, int assert, MPIASP_Win * ua_win)
{
    int mpi_errno = MPI_SUCCESS;
    int user_rank, node_id;

    PMPI_Comm_rank(ua_win->user_comm, &user_rank);
    node_id = MPIASP_ALL_NODE_IDS[MPIASP_MY_RANK_IN_WORLD];

#ifdef MTCORE_ENABLE_GRANT_LOCK_HIDDEN_BYTE
    MTCORE_GRANT_LOCK_DATATYPE buf[1];
    mpi_errno = PMPI_Get(buf, 1, MTCORE_GRANT_LOCK_MPI_DATATYPE, ua_win->asp_ranks_in_ua[node_id],
                         ua_win->grant_lock_asp_offset, 1, MTCORE_GRANT_LOCK_MPI_DATATYPE,
                         ua_win->ua_wins[user_rank]);
#else
    /* Simply get 1 byte from start, it does not affect the result of other updates */
    char buf[1];
    mpi_errno = PMPI_Get(buf, 1, MPI_CHAR, ua_win->asp_ranks_in_ua[node_id], 0,
                         1, MPI_CHAR, ua_win->ua_wins[user_rank]);
#endif
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    mpi_errno = PMPI_Win_flush(ua_win->asp_ranks_in_ua[node_id], ua_win->ua_wins[user_rank]);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    MPIASP_DBG_PRINT("[%d]grant local lock(Helper(%d), ua_wins[%d])\n", user_rank,
                     ua_win->asp_ranks_in_ua[node_id], user_rank);
  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

extern int run_asp_main(void);

#endif /* MPIASP_H_ */

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

/* Multi-objects lock (MLOCK) component on user processes */

#ifdef CSPU_MLOCK_DEBUG
#define CSPU_MLOCK_DBG_PRINT(str,...) do { \
    fprintf(stdout, "[CSPU-MLOCK][%d]"str, CSP_PROC.wrank, ## __VA_ARGS__); \
    fflush(stdout); \
    } while (0)
static const char *cwp_lock_status_name[CSP_MLOCK_STATUS_MAX] = {
    "unset",
    "suspended_l",
    "suspended_h",
    "acquired"
};

#define CSPU_MLOCK_DBG_GID_TO_STR(gid, str) CSP_mlock_gid_to_str(gid, str)
#else
#define CSPU_MLOCK_DBG_PRINT(str,...) do { } while (0)
#define CSPU_MLOCK_DBG_GID_TO_STR(gid, str) do { } while (0)
#endif

/* ======================================================================
 * Internal lock related functions.
 * ====================================================================== */

#if defined(CSP_ENABLE_THREAD_SAFE)
typedef struct CSPU_mlock_seqno_gen {
    CSP_thread_cs_t cs;         /* per window critical section object,
                                 * used only when this process is multi-threaded. */
    int no;
} CSPU_mlock_seqno_gen_t;
static CSPU_mlock_seqno_gen_t mlock_seqno_gen;
#endif

static inline int mlock_sync_status(CSP_mlock_gid_t group_id, CSP_mlock_status_t * lock_status)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_cwp_pkt_t pkt;
    CSP_cwp_mlock_status_sync_pkt_t *locksync_pkt = &pkt.u.lock_status_sync;
    char gidstr[CSP_MLOCK_GID_MAXLEN] CSP_ATTRIBUTE((unused));

    CSP_CALLMPI(RETURN, PMPI_Recv((char *) &pkt, sizeof(CSP_cwp_pkt_t),
                                  MPI_CHAR, CSP_PROC.user.g_lranks[0],
                                  CSP_CWP_MLOCK_SYNC_TAG(group_id), CSP_PROC.local_comm,
                                  MPI_STATUS_IGNORE));

    CSPU_MLOCK_DBG_GID_TO_STR(group_id, gidstr);
    CSPU_MLOCK_DBG_PRINT(" \t sync LOCK STAT (%d -> %d[%s], gid %s)\n", (*lock_status),
                         locksync_pkt->status, cwp_lock_status_name[locksync_pkt->status], gidstr);

    (*lock_status) = locksync_pkt->status;
    return mpi_errno;
}

static inline int mlock_issue_acquire_req(CSP_mlock_gid_t group_id)
{
    CSP_cwp_pkt_t pkt;
    CSP_cwp_mlock_acquire_pkt_t *lockacq_pkt = &pkt.u.lock_acquire;
    char gidstr[CSP_MLOCK_GID_MAXLEN] CSP_ATTRIBUTE((unused));

    pkt.cmd_type = CSP_MLOCK_ACQUIRE;
    lockacq_pkt->group_id = group_id;

    CSPU_MLOCK_DBG_GID_TO_STR(group_id, gidstr);
    CSPU_MLOCK_DBG_PRINT(" \t send LOCK CMD %d (acquire, gid %s)\n", pkt.cmd_type, gidstr);

    return CSPU_cwp_issue(&pkt);
}

static inline int mlock_issue_discard_req(CSP_mlock_gid_t group_id)
{
    CSP_cwp_pkt_t pkt;
    CSP_cwp_mlock_discard_pkt_t *lockdcd_pkt = &pkt.u.lock_discard;
    char gidstr[CSP_MLOCK_GID_MAXLEN] CSP_ATTRIBUTE((unused));

    pkt.cmd_type = CSP_MLOCK_DISCARD;
    lockdcd_pkt->group_id = group_id;

    CSPU_MLOCK_DBG_GID_TO_STR(group_id, gidstr);
    CSPU_MLOCK_DBG_PRINT(" \t send LOCK CMD %d (discard, gid %s)\n", pkt.cmd_type, gidstr);

    return CSPU_cwp_issue(&pkt);
}

static inline int mlock_issue_release_req(CSP_mlock_gid_t group_id)
{
    CSP_cwp_pkt_t pkt;
    CSP_cwp_mlock_release_pkt_t *lockrls_pkt = &pkt.u.lock_release;
    char gidstr[CSP_MLOCK_GID_MAXLEN] CSP_ATTRIBUTE((unused));

    pkt.cmd_type = CSP_MLOCK_RELEASE;
    lockrls_pkt->group_id = group_id;

    CSPU_MLOCK_DBG_GID_TO_STR(group_id, gidstr);
    CSPU_MLOCK_DBG_PRINT(" \t send LOCK CMD %d (release, gid %s)\n", pkt.cmd_type, gidstr);

    return CSPU_cwp_issue(&pkt);
}

static int mlock_generate_gid(MPI_Comm user_root_comm, CSP_mlock_gid_t * group_id)
{
    int mpi_errno = MPI_SUCCESS;
    int user_root_rank = 0;

    CSP_CALLMPI(RETURN, PMPI_Comm_rank(user_root_comm, &user_root_rank));

    if (user_root_rank == 0) {
        group_id->rank = CSP_PROC.wrank;

#if defined(CSP_ENABLE_THREAD_SAFE)
        CSPU_THREAD_OBJ_CS_LOCAL_DCL();
        CSPU_THREAD_ENTER_OBJ_CS(&mlock_seqno_gen);
        group_id->seqno = mlock_seqno_gen.no++;
        CSPU_THREAD_EXIT_OBJ_CS(&mlock_seqno_gen);
#endif
    }

    CSP_CALLMPI(NOSTMT, PMPI_Bcast(group_id, sizeof(CSP_mlock_gid_t), MPI_CHAR, 0, user_root_comm));
    return mpi_errno;
}

int CSPU_mlock_init(void)
{
    int mpi_errno = MPI_SUCCESS;

#if defined(CSP_ENABLE_THREAD_SAFE)
    mlock_seqno_gen.no = 0;
    CSPU_THREAD_INIT_OBJ_CS(&mlock_seqno_gen);
#endif

  fn_exit:
    return mpi_errno;
  fn_fail:
    CSP_ATTRIBUTE((unused));
    goto fn_exit;
}

int CSPU_mlock_destroy(void)
{
    int mpi_errno = MPI_SUCCESS;

#if defined(CSP_ENABLE_THREAD_SAFE)
    CSPU_THREAD_DESTROY_OBJ_CS(&mlock_seqno_gen);
#endif

  fn_exit:
    return mpi_errno;
  fn_fail:
    CSP_ATTRIBUTE((unused));
    goto fn_exit;
}


/* Lock ghost processes for the next function command (blocking collective call).
 * It is collective call in user root communicator, blocking wait till all user
 * root processes have acquired the lock on their local ghost processes.
 * The lock is released automatically after function remotely finished.
 *
 * Note that this routine must be called after all user root arrived in function,
 * otherwise the group id may not be unique (i.e., two groups [0, 1] and [0, 3],
 * 0 and 3 start acquiring lock concurrently). */
int CSPU_mlock_acquire(MPI_Comm user_root_comm)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_mlock_gid_t group_id;
    int all_lock_acquired = 0;
    int user_root_rank = -1;
    char gidstr[CSP_MLOCK_GID_MAXLEN] CSP_ATTRIBUTE((unused));

    CSP_mlock_status_t lock_status = CSP_MLOCK_STATUS_UNSET;
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(user_root_comm, &user_root_rank));

    /* Get group id (the first root's world rank and seq no (unique when threaded)). */
    mpi_errno = mlock_generate_gid(user_root_comm, &group_id);

    CSPU_MLOCK_DBG_GID_TO_STR(group_id, gidstr);
    CSPU_MLOCK_DBG_PRINT(" \t generated gid %s\n", gidstr);

    /* Wait till all user roots have acquired lock. */
    do {
        int min_lock_stats[2];
        int my_lock_stats[2];

        if (lock_status == CSP_MLOCK_STATUS_UNSET) {
            /* only initial, discarded, released locks need issue lock request. */
            mpi_errno = mlock_issue_acquire_req(group_id);
            CSP_CHKMPIFAIL_JUMP(mpi_errno);
        }

        if (lock_status != CSP_MLOCK_STATUS_ACQUIRED) {
            /* new state can be acquired, suspended_l, suspended_h. */
            mpi_errno = mlock_sync_status(group_id, &lock_status);
            CSP_CHKMPIFAIL_JUMP(mpi_errno);
        }

        my_lock_stats[0] = lock_status; /* suspended_l < suspended_h < acquired */
        my_lock_stats[1] = user_root_rank;
        CSP_CALLMPI(JUMP, PMPI_Allreduce(my_lock_stats, min_lock_stats, 1, MPI_2INT, MPI_MINLOC,
                                         user_root_comm));
        CSPU_MLOCK_DBG_PRINT(" \t all-reduced LOCK status min(stat %d [%s], root %d, gid %s)\n",
                             min_lock_stats[0], cwp_lock_status_name[min_lock_stats[0]],
                             min_lock_stats[1], gidstr);

        /* when the smallest group_id is equal to group_id + np, all other roots got its lock. */
        all_lock_acquired = (min_lock_stats[0] == CSP_MLOCK_STATUS_ACQUIRED) ? 1 : 0;
        if (!all_lock_acquired) {
            char bc_byte;
            int lowest_priority = min_lock_stats[0];
            int lowest_p_root = min_lock_stats[1];

            if (lowest_priority == CSP_MLOCK_STATUS_SUSPENDED_L) {
                /* only one of the lowest priority root wait for a lock */
                if (user_root_rank == lowest_p_root) {
                    /* my lock is suspended with low priority now,
                     * wait till all higher priority groups finished work. */
                    do {
                        mpi_errno = mlock_sync_status(group_id, &lock_status);
                        CSP_CHKMPIFAIL_JUMP(mpi_errno);
                    } while (lock_status != CSP_MLOCK_STATUS_ACQUIRED);
                }
                /* all other roots give up its lock and wait for the first root */
                else {
                    if (lock_status == CSP_MLOCK_STATUS_ACQUIRED) {
                        mpi_errno = mlock_issue_release_req(group_id);
                    }
                    else {
                        mpi_errno = mlock_issue_discard_req(group_id);
                    }
                    CSP_CHKMPIFAIL_JUMP(mpi_errno);

                    /* drop any SYNC received before the discarded ACK (none). */
                    do {
                        mpi_errno = mlock_sync_status(group_id, &lock_status);
                        CSP_CHKMPIFAIL_JUMP(mpi_errno);
                    } while (lock_status != CSP_MLOCK_STATUS_UNSET);
                }

                /* wait till the first root got lock */
                CSP_CALLMPI(JUMP, PMPI_Bcast(&bc_byte, 1, MPI_CHAR, lowest_p_root, user_root_comm));

                CSPU_MLOCK_DBG_PRINT(" \t root %d bcast from %d in user root group, gid %s\n",
                                     user_root_rank, lowest_p_root, gidstr);
            }
        }
    } while (!all_lock_acquired);

    CSPU_MLOCK_DBG_PRINT(" \t gid %s acquired lock, return\n", gidstr);

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

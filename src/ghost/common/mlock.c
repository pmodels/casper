/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cspg.h"

/* Multi-objects lock (MLOCK) component on ghost processes */

#ifdef CSPG_MLOCK_DEBUG
#define CSPG_MLOCK_DBG_PRINT(str,...) do { \
    fprintf(stdout, "[CSPG-MLOCK][%d]"str, CSP_PROC.wrank, ## __VA_ARGS__); \
    fflush(stdout); \
    } while (0)
static const char *mlock_status_name[CSP_MLOCK_STATUS_MAX] =
    { "none", "suspended_l", "suspended_h", "acquired" };
#define CSPG_MLOCK_DBG_GID_TO_STR(gid, str) CSP_mlock_gid_to_str(gid, str)
#else
#define CSPG_MLOCK_DBG_PRINT(str,...) do { } while (0)
#define CSPG_MLOCK_DBG_GID_TO_STR(gid, str) do { } while (0)
#endif

typedef struct CSPG_mlock_req {
    CSP_mlock_gid_t group_id;
    int user_local_rank;
    CSP_mlock_status_t status;
} CSPG_mlock_req_t;

static CSP_slist_t mlock_susped_reqs_list;
static CSPG_mlock_req_t *mlock_granted_req = NULL;
static int mlock_req_cnt = 0;   /* counter to tracking unreleased lock requests. */

/* return positive value if gid1 > gid2.*/
static inline int mlock_gid_compare_fnc(CSP_mlock_gid_t gid1, CSP_mlock_gid_t gid2)
{
    int diff = 0;

    diff = gid1.rank - gid2.rank;

#if defined(CSP_ENABLE_THREAD_SAFE)
    /* if same rank, compare seqno. */
    if (diff == 0)
        diff = gid1.seqno - gid2.seqno;
#endif
    return diff;
}

/* return positive value if ubuf1 > ubuf2.*/
static int mlock_req_compare_fnc(void *ubuf1, void *ubuf2)
{
    CSPG_mlock_req_t *req1 = NULL, *req2 = NULL;
    req1 = (CSPG_mlock_req_t *) ubuf1;
    req2 = (CSPG_mlock_req_t *) ubuf2;

    /* compare group id. */
    return mlock_gid_compare_fnc(req1->group_id, req2->group_id);
}

static inline int mlock_sync_status(CSPG_mlock_req_t * req)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_cwp_pkt_t pkt;
    CSP_cwp_mlock_status_sync_pkt_t *locksync_pkt = &pkt.u.lock_status_sync;
    char gidstr[CSP_MLOCK_GID_MAXLEN] CSP_ATTRIBUTE((unused));

    CSP_cwp_init_pkt(CSP_MLOCK_STATUS_SYNC, &pkt);

    locksync_pkt->status = req->status;

    CSPG_MLOCK_DBG_GID_TO_STR(req->group_id, gidstr);
    CSPG_MLOCK_DBG_PRINT(" \t sync CMD STAT %d [%s] to local user %d, gid %s\n",
                         locksync_pkt->status, mlock_status_name[locksync_pkt->status],
                         req->user_local_rank, gidstr);

    CSP_CALLMPI(NOSTMT, PMPI_Send((char *) &pkt, sizeof(CSP_cwp_pkt_t), MPI_CHAR,
                                  req->user_local_rank, CSP_CWP_MLOCK_SYNC_TAG(req->group_id),
                                  CSP_PROC.local_comm));
    return mpi_errno;
}

static inline int mlock_suspend_req(CSPG_mlock_req_t * req)
{
    char gidstr[CSP_MLOCK_GID_MAXLEN] CSP_ATTRIBUTE((unused));
    CSP_ASSERT(mlock_granted_req != NULL);

    if (mlock_req_compare_fnc((void *) mlock_granted_req, (void *) req) > 0) {
        req->status = CSP_MLOCK_STATUS_SUSPENDED_H;
    }
    else {
        req->status = CSP_MLOCK_STATUS_SUSPENDED_L;
    }

    CSPG_MLOCK_DBG_GID_TO_STR(req->group_id, gidstr);
    CSPG_MLOCK_DBG_PRINT(" \t suspend lock req %p (gid %s, %d, %s)\n", req,
                         gidstr, req->user_local_rank,
                         (req->status == CSP_MLOCK_STATUS_SUSPENDED_H) ? "H" : "L");

    /* Insert the new one in suspended list */
    if ((CSP_slist_insert(req, &mlock_susped_reqs_list)) != 0)
        return MPI_ERR_OTHER;

    return mlock_sync_status(req);
}

static inline int mlock_grant_req(CSPG_mlock_req_t * req)
{
    char gidstr[CSP_MLOCK_GID_MAXLEN] CSP_ATTRIBUTE((unused));
    req->status = CSP_MLOCK_STATUS_ACQUIRED;
    mlock_granted_req = req;

    CSPG_MLOCK_DBG_GID_TO_STR(req->group_id, gidstr);
    CSPG_MLOCK_DBG_PRINT(" \t grant lock req %p (gid %s, %d)\n", req, gidstr, req->user_local_rank);
    return mlock_sync_status(req);
}

static int mlock_grant_next_req(void)
{
    int mpi_errno = MPI_SUCCESS;
    CSPG_mlock_req_t *next_req = NULL;
    CSP_slist_elem_t *e = NULL;

    /* Get next available request */
    next_req = (CSPG_mlock_req_t *) CSP_slist_dequeue(&mlock_susped_reqs_list);
    if (next_req) {
        mpi_errno = mlock_grant_req(next_req);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);

        /* Degrade any suspended requests with high priority. */
        e = mlock_susped_reqs_list.head;
        while (e) {
            CSPG_mlock_req_t *req = (CSPG_mlock_req_t *) e->ubuf;
            CSP_mlock_status_t old_status = req->status;

            /* all high priority requests must be in front of any low priority
             * requests in sorted list. */
            if (old_status == CSP_MLOCK_STATUS_SUSPENDED_L)
                break;

            req->status = CSP_MLOCK_STATUS_SUSPENDED_L;
            mpi_errno = mlock_sync_status(req);
            CSP_CHKMPIFAIL_JUMP(mpi_errno);

            e = e->next;
        }
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static int mlock_acquire_cwp_handler(CSP_cwp_pkt_t * pkt, int user_local_rank)
{
    int mpi_errno = MPI_SUCCESS;
    CSPG_mlock_req_t *req = NULL;
    CSP_cwp_mlock_acquire_pkt_t *lockacq_pkt = &pkt->u.lock_acquire;

    /* Initialize new lock request */
    req = CSP_calloc(1, sizeof(CSPG_mlock_req_t));
    if (req == NULL) {
        mpi_errno = MPI_ERR_NO_MEM;
        goto fn_fail;
    }

    req->group_id = lockacq_pkt->group_id;
    req->user_local_rank = user_local_rank;
    mlock_req_cnt++;

    if (mlock_granted_req != NULL) {
        mpi_errno = mlock_suspend_req(req);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }
    else {
        mpi_errno = mlock_grant_req(req);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    if (req)
        free(req);
    goto fn_exit;
}

static int mlock_release_cwp_handler(CSP_cwp_pkt_t * pkt, int user_local_rank)
{
    int mpi_errno = MPI_SUCCESS;
    CSPG_mlock_req_t *rels_req = NULL;
    CSP_cwp_mlock_release_pkt_t *lockrls_pkt = &pkt->u.lock_release;
    char gidstr[CSP_MLOCK_GID_MAXLEN] CSP_ATTRIBUTE((unused));

    CSP_ASSERT(mlock_granted_req != NULL &&
               mlock_granted_req->user_local_rank == user_local_rank &&
               (mlock_gid_compare_fnc(lockrls_pkt->group_id, mlock_granted_req->group_id) == 0));

    rels_req = mlock_granted_req;
    mlock_granted_req = NULL;

    CSPG_MLOCK_DBG_GID_TO_STR(rels_req->group_id, gidstr);
    CSPG_MLOCK_DBG_PRINT(" \t release lock (gid %s, %d)\n", gidstr, user_local_rank);

    mpi_errno = mlock_grant_next_req();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    rels_req->status = CSP_MLOCK_STATUS_UNSET;
    mpi_errno = mlock_sync_status(rels_req);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    free(rels_req);
    mlock_req_cnt--;

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

static int mlock_discard_cwp_handler(CSP_cwp_pkt_t * pkt, int user_local_rank)
{
    int mpi_errno = MPI_SUCCESS;
    CSPG_mlock_req_t *disc_req = NULL;
    CSPG_mlock_req_t search_req;
    CSP_cwp_mlock_discard_pkt_t *lockdcd_pkt = &pkt->u.lock_discard;
    char gidstr[CSP_MLOCK_GID_MAXLEN] CSP_ATTRIBUTE((unused));

    /* If lock is just granted before receiving discard message, release it. */
    if (mlock_granted_req &&
        (mlock_gid_compare_fnc(lockdcd_pkt->group_id, mlock_granted_req->group_id) == 0))
        return mlock_release_cwp_handler(pkt, user_local_rank);

    CSPG_MLOCK_DBG_GID_TO_STR(lockdcd_pkt->group_id, gidstr);
    CSPG_MLOCK_DBG_PRINT(" \t discard lock req (gid %s, %d)\n", gidstr, user_local_rank);

    /* Remove from suspended list. */
    search_req.user_local_rank = user_local_rank;
    search_req.group_id = lockdcd_pkt->group_id;
    disc_req = CSP_slist_remove(&search_req, &mlock_susped_reqs_list);
    CSP_ASSERT(disc_req != NULL);

    disc_req->status = CSP_MLOCK_STATUS_UNSET;
    mpi_errno = mlock_sync_status(disc_req);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    free(disc_req);
    mlock_req_cnt--;

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

/* Release current granted MLOCK.
 * User function may lock ghosts to get exclusive access, after such function is finished,
 * the ghosts call this routine to release the lock. */
int CSPG_mlock_release(void)
{
    int mpi_errno = MPI_SUCCESS;
    int local_gp_rank = 0;

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_PROC.ghost.g_local_comm, &local_gp_rank));

    /* Only the first local ghost handles lock. */
    if (local_gp_rank == 0) {
        if (mlock_granted_req) {
            char gidstr[CSP_MLOCK_GID_MAXLEN] CSP_ATTRIBUTE((unused));

            CSPG_MLOCK_DBG_GID_TO_STR(mlock_granted_req->group_id, gidstr);
            CSPG_MLOCK_DBG_PRINT("\n");
            CSPG_MLOCK_DBG_PRINT(" ghost 0 release lock req %p (gid %s, %d)\n",
                                 mlock_granted_req, gidstr, mlock_granted_req->user_local_rank);

            /* Release current lock */
            free(mlock_granted_req);
            mlock_granted_req = NULL;
            mlock_req_cnt--;
        }

        mpi_errno = mlock_grant_next_req();
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

/* Initialize MLOCK.
 * This includes CWP handler registration and lock structure initialization. */
void CSPG_mlock_init(void)
{
    CSPG_cwp_register_root_handler(CSP_MLOCK_ACQUIRE, mlock_acquire_cwp_handler);
    CSPG_cwp_register_root_handler(CSP_MLOCK_DISCARD, mlock_discard_cwp_handler);
    CSPG_cwp_register_root_handler(CSP_MLOCK_RELEASE, mlock_release_cwp_handler);

    CSP_slist_init(CSPG_SLIST_ORDER_ASC, mlock_req_compare_fnc, &mlock_susped_reqs_list);
    CSPG_MLOCK_DBG_PRINT(" initialized\n");
}

/* Destroy MLOCK.
 * This routine frees all unexpectedly enqueued requests and destroy lock structure. */
void CSPG_mlock_destory(void)
{
    CSPG_mlock_req_t *req = NULL;

    if (mlock_req_cnt > 0) {
        CSPG_DBG_PRINT("%d requests are not freed !\n", mlock_req_cnt);

        /* Release all existing lock requests */
        while (CSP_slist_count(&mlock_susped_reqs_list) > 0) {
            req = CSP_slist_dequeue(&mlock_susped_reqs_list);
            if (req)
                free(req);
        }

        if (mlock_granted_req) {
            free(mlock_granted_req);
            mlock_granted_req = NULL;
        }
    }

    CSP_slist_destroy(&mlock_susped_reqs_list);
    CSPG_MLOCK_DBG_PRINT(" destroyed\n");
}

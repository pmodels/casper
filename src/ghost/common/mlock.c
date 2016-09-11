/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cspg.h"
#include "cwp.h"
#include "slist.h"

/* Multi-objects lock (MLOCK) component on ghost processes */

#ifdef CSPG_MLOCK_DEBUG
#define CSPG_MLOCK_DBG_PRINT(str,...) do { \
    fprintf(stdout, "[CSPG-MLOCK][%d]"str, CSP_PROC.wrank, ## __VA_ARGS__); \
    fflush(stdout); \
    } while (0)
static const char *mlock_status_name[CSP_MLOCK_STATUS_MAX] =
    { "none", "suspended_l", "suspended_h", "acquired" };
#else
#define CSPG_MLOCK_DBG_PRINT(str,...)
#endif

static CSP_slist_t mlock_susped_reqs_list;
static CSPG_mlock_req_t *mlock_granted_req = NULL;
static int mlock_req_cnt = 0;   /* counter to tracking unreleased lock requests. */

static int mlock_req_compare_fnc(void *ubuf1, void *ubuf2)
{
    CSPG_mlock_req_t *req1 = NULL, *req2 = NULL;
    req1 = (CSPG_mlock_req_t *) ubuf1;
    req2 = (CSPG_mlock_req_t *) ubuf2;
    return req1->group_id - req2->group_id;
}

static inline int mlock_sync_status(CSPG_mlock_req_t * req)
{
    CSP_cwp_pkt_t pkt;
    CSP_cwp_mlock_status_sync_pkt_t *locksync_pkt = &pkt.u.lock_status_sync;

    CSP_cwp_init_pkt(CSP_MLOCK_STATUS_SYNC, &pkt);

    locksync_pkt->status = req->status;
    CSPG_MLOCK_DBG_PRINT(" \t sync CMD ACK %d [%s] to local user %d\n",
                         locksync_pkt->status, mlock_status_name[locksync_pkt->status],
                         req->user_local_rank);

    return PMPI_Send((char *) &pkt, sizeof(CSP_cwp_pkt_t), MPI_CHAR,
                     req->user_local_rank, CSP_CWP_TAG, CSP_PROC.local_comm);
}

static inline int mlock_suspend_req(CSPG_mlock_req_t * req)
{
    CSPG_assert(mlock_granted_req != NULL);
    if (req->group_id < mlock_granted_req->group_id) {
        req->status = CSP_MLOCK_STATUS_SUSPENDED_H;
    }
    else {
        req->status = CSP_MLOCK_STATUS_SUSPENDED_L;
    }

    CSPG_MLOCK_DBG_PRINT(" \t suspend lock req %p (%d, %d, %s)\n", req,
                         req->group_id, req->user_local_rank,
                         (req->status == CSP_MLOCK_STATUS_SUSPENDED_H) ? "H" : "L");

    /* Insert the new one in suspended list */
    if ((CSP_slist_insert(req, &mlock_susped_reqs_list)) != 0)
        return MPI_ERR_OTHER;

    return mlock_sync_status(req);
}

static inline int mlock_grant_req(CSPG_mlock_req_t * req)
{
    req->status = CSP_MLOCK_STATUS_ACQUIRED;
    mlock_granted_req = req;

    CSPG_MLOCK_DBG_PRINT(" \t grant lock req %p (%d, %d)\n", req, req->group_id,
                         req->user_local_rank);
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
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

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
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;

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
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
    else {
        mpi_errno = mlock_grant_req(req);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    if (req)
        free(req);
    goto fn_exit;
}

static int mlock_release_cwp_handler(CSP_cwp_pkt_t * pkt CSP_ATTRIBUTE((unused)),
                                     int user_local_rank)
{
    int mpi_errno = MPI_SUCCESS;
    CSPG_mlock_req_t *rels_req = NULL;

    CSPG_assert(mlock_granted_req != NULL && mlock_granted_req->user_local_rank == user_local_rank);

    rels_req = mlock_granted_req;
    mlock_granted_req = NULL;

    mpi_errno = mlock_grant_next_req();
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    rels_req->status = CSP_MLOCK_STATUS_UNSET;
    mpi_errno = mlock_sync_status(rels_req);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

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
    CSP_cwp_mlock_acquire_pkt_t *lockdcd_pkt = &pkt->u.lock_discard;

    /* If lock is just granted before receiving discard message, release it. */
    if (mlock_granted_req && user_local_rank == mlock_granted_req->user_local_rank)
        return mlock_release_cwp_handler(pkt, user_local_rank);

    /* Remove from suspended list. */
    search_req.user_local_rank = user_local_rank;
    search_req.group_id = lockdcd_pkt->group_id;
    disc_req = CSP_slist_remove(&search_req, &mlock_susped_reqs_list);
    CSPG_assert(disc_req != NULL);

    disc_req->status = CSP_MLOCK_STATUS_UNSET;
    mpi_errno = mlock_sync_status(disc_req);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

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

    PMPI_Comm_rank(CSP_PROC.ghost.g_local_comm, &local_gp_rank);

    /* Only the first local ghost handles lock. */
    if (local_gp_rank == 0) {
        if (mlock_granted_req) {
            CSPG_MLOCK_DBG_PRINT("\n");
            CSPG_MLOCK_DBG_PRINT(" ghost 0 release lock req %p (%d, %d)\n",
                                 mlock_granted_req, mlock_granted_req->group_id,
                                 mlock_granted_req->user_local_rank);

            /* Release current lock */
            free(mlock_granted_req);
            mlock_granted_req = NULL;
            mlock_req_cnt--;
        }

        mpi_errno = mlock_grant_next_req();
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
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
        CSPG_WARN_PRINT("%d requests are not freed !\n", mlock_req_cnt);

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

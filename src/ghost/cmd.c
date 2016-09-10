/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cspg.h"
#include "list.h"

#ifdef CSPG_CMD_DEBUG
#define CSPG_CMD_DBG_PRINT(str,...) do { \
    fprintf(stdout, "[CSPG-CMD][%d]"str, CSP_PROC.wrank, ## __VA_ARGS__); \
    fflush(stdout); \
    } while (0)

static const char *csp_cmd_lock_status_name[CSP_CMD_LOCK_STATUS_MAX] =
    { "none", "suspended_l", "suspended_h", "acquired" };
static const char *csp_cmd_name[CSP_CMD_MAX] = {
    "unset",
    "win_allocate",
    "win_free",
    "finalize",
    "lock_acquire",
    "lock_discard",
    "lock_release",
    "lock_status_sync"
};
#else
#define CSPG_CMD_DBG_PRINT(str,...)
#endif

/* Command handlers on root ghosts.
 * The handler is called when received a command from the user process.*/
static CSPG_cmd_root_handler_t CSPG_cmd_root_handlers[CSP_CMD_MAX] = { NULL };

/* Command handlers on children ghosts.
 * The handler is called when received a command broadcasted from the root ghost. */
static CSPG_cmd_handler_t CSPG_cmd_handlers[CSP_CMD_MAX] = { NULL };

static CSP_slist_t susped_lock_reqs_list;
static CSPG_cmd_lock_req_t *granted_lock_req = NULL;
static int lock_req_cnt = 0;    /* counter to tracking unreleased lock requests. */

static int lock_req_compare_fnc(void *ubuf1, void *ubuf2)
{
    CSPG_cmd_lock_req_t *req1 = NULL, *req2 = NULL;
    req1 = (CSPG_cmd_lock_req_t *) ubuf1;
    req2 = (CSPG_cmd_lock_req_t *) ubuf2;
    return req1->group_id - req2->group_id;
}

static inline int sync_lock_status(CSPG_cmd_lock_req_t * req)
{
    CSP_cmd_pkt_t pkt;
    CSP_cmd_lock_status_sync_pkt_t *locksync_pkt = &pkt.u.lock_status_sync;

    locksync_pkt->status = req->status;
    CSPG_CMD_DBG_PRINT(" \t sync CMD ACK %d [%s] to local user %d\n",
                       locksync_pkt->status, csp_cmd_lock_status_name[locksync_pkt->status],
                       req->user_local_rank);

    return PMPI_Send((char *) &pkt, sizeof(CSP_cmd_pkt_t), MPI_CHAR,
                     req->user_local_rank, CSP_CMD_TAG, CSP_PROC.local_comm);
}

static inline int suspend_lock_req(CSPG_cmd_lock_req_t * req)
{
    CSPG_assert(granted_lock_req != NULL);
    if (req->group_id < granted_lock_req->group_id) {
        req->status = CSP_CMD_LOCK_STATUS_SUSPENDED_H;
    }
    else {
        req->status = CSP_CMD_LOCK_STATUS_SUSPENDED_L;
    }

    CSPG_CMD_DBG_PRINT(" \t suspend lock req %p (%d, %d, %s)\n", req,
                       req->group_id, req->user_local_rank,
                       (req->status == CSP_CMD_LOCK_STATUS_SUSPENDED_H) ? "H" : "L");

    /* Insert the new one in suspended list */
    if ((CSP_slist_insert(req, &susped_lock_reqs_list)) != 0)
        return MPI_ERR_OTHER;

    return sync_lock_status(req);
}

static inline int grant_lock_req(CSPG_cmd_lock_req_t * req)
{
    req->status = CSP_CMD_LOCK_STATUS_ACQUIRED;
    granted_lock_req = req;

    CSPG_CMD_DBG_PRINT(" \t grant lock req %p (%d, %d)\n", req, req->group_id,
                       req->user_local_rank);
    return sync_lock_status(req);
}

static int grant_next_lock_req(void)
{
    int mpi_errno = MPI_SUCCESS;
    CSPG_cmd_lock_req_t *next_req = NULL;
    CSP_slist_elem_t *e = NULL;

    /* Get next available request */
    next_req = (CSPG_cmd_lock_req_t *) CSP_slist_dequeue(&susped_lock_reqs_list);
    if (next_req) {
        mpi_errno = grant_lock_req(next_req);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;

        /* Degrade any suspended requests with high priority. */
        e = susped_lock_reqs_list.head;
        while (e) {
            CSPG_cmd_lock_req_t *req = (CSPG_cmd_lock_req_t *) e->ubuf;
            CSP_cmd_lock_status_t old_status = req->status;

            /* all high priority requests must be in front of any low priority
             * requests in sorted list. */
            if (old_status == CSP_CMD_LOCK_STATUS_SUSPENDED_L)
                break;

            req->status = CSP_CMD_LOCK_STATUS_SUSPENDED_L;
            mpi_errno = sync_lock_status(req);
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

static int CSPG_cmd_acquire_lock_handler(CSP_cmd_pkt_t * pkt, int user_local_rank)
{
    int mpi_errno = MPI_SUCCESS;
    CSPG_cmd_lock_req_t *req = NULL;
    CSP_cmd_lock_acquire_pkt_t *lockacq_pkt = &pkt->u.lock_acquire;

    /* Initialize new lock request */
    req = CSP_calloc(1, sizeof(CSPG_cmd_lock_req_t));
    if (req == NULL) {
        mpi_errno = MPI_ERR_NO_MEM;
        goto fn_fail;
    }

    req->group_id = lockacq_pkt->group_id;
    req->user_local_rank = user_local_rank;
    lock_req_cnt++;

    if (granted_lock_req != NULL) {
        mpi_errno = suspend_lock_req(req);
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }
    else {
        mpi_errno = grant_lock_req(req);
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

static int CSPG_cmd_release_lock_handler(CSP_cmd_pkt_t * pkt CSP_ATTRIBUTE((unused)),
                                         int user_local_rank)
{
    int mpi_errno = MPI_SUCCESS;
    CSPG_cmd_lock_req_t *rels_req = NULL;

    CSPG_assert(granted_lock_req != NULL && granted_lock_req->user_local_rank == user_local_rank);

    rels_req = granted_lock_req;
    granted_lock_req = NULL;

    mpi_errno = grant_next_lock_req();
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    rels_req->status = CSP_CMD_LOCK_STATUS_UNSET;
    mpi_errno = sync_lock_status(rels_req);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    free(rels_req);
    lock_req_cnt--;

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

static int CSPG_cmd_discard_lock_handler(CSP_cmd_pkt_t * pkt, int user_local_rank)
{
    int mpi_errno = MPI_SUCCESS;
    CSPG_cmd_lock_req_t *disc_req = NULL;
    CSPG_cmd_lock_req_t search_req;
    CSP_cmd_lock_acquire_pkt_t *lockdcd_pkt = &pkt->u.lock_discard;

    /* If lock is just granted before receiving discard message, release it. */
    if (granted_lock_req && user_local_rank == granted_lock_req->user_local_rank)
        return CSPG_cmd_release_lock_handler(pkt, user_local_rank);

    /* Remove from suspended list. */
    search_req.user_local_rank = user_local_rank;
    search_req.group_id = lockdcd_pkt->group_id;
    disc_req = CSP_slist_remove(&search_req, &susped_lock_reqs_list);
    CSPG_assert(disc_req != NULL);

    disc_req->status = CSP_CMD_LOCK_STATUS_UNSET;
    mpi_errno = sync_lock_status(disc_req);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    free(disc_req);
    lock_req_cnt--;

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

/* Receive command from any local user process (blocking call),
 * and process it in the corresponding command handler.
 * Only return when finalize command is done on all ghost processes. */
int CSPG_cmd_do_progress(void)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_cmd_pkt_t pkt;
    MPI_Status stat;
    int local_gp_rank = -1;

    PMPI_Comm_rank(CSP_PROC.ghost.g_local_comm, &local_gp_rank);
    memset(&pkt, 0, sizeof(CSP_cmd_pkt_t));

    while (1) {
        /* Only the first local ghost (root) receives commands from any local user process.
         * Otherwise deadlock may happen if multiple user roots send request to
         * ghosts concurrently and some ghosts are locked in different communicator creation. */
        if (local_gp_rank == 0) {
            mpi_errno = PMPI_Recv((char *) &pkt, sizeof(CSP_cmd_pkt_t), MPI_CHAR,
                                  MPI_ANY_SOURCE, CSP_CMD_TAG, CSP_PROC.local_comm, &stat);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;

            /* skip undefined command */
            if (pkt.cmd_type <= CSP_CMD_UNSET || pkt.cmd_type >= CSP_CMD_MAX ||
                !CSPG_cmd_root_handlers[pkt.cmd_type]) {
                CSPG_CMD_DBG_PRINT(" Received undefined CMD %d\n", (int) (pkt.cmd_type));
                continue;
            }

            CSPG_CMD_DBG_PRINT(" ghost 0 received CMD %d [%s] from %d\n",
                               (int) (pkt.cmd_type), csp_cmd_name[pkt.cmd_type], stat.MPI_SOURCE);
            mpi_errno = CSPG_cmd_root_handlers[pkt.cmd_type] (&pkt, stat.MPI_SOURCE);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;

        }
        else {
            /* All other ghosts wait on internal commands broadcasted by the root,
             * which is issued in root's command handler. */
            mpi_errno = CSPG_cmd_bcast(&pkt);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;

            /* skip undefined internal command */
            if (pkt.cmd_type <= CSP_CMD_UNSET || pkt.cmd_type >= CSP_CMD_MAX ||
                !CSPG_cmd_handlers[pkt.cmd_type]) {
                CSPG_CMD_DBG_PRINT(" Received undefined CMD %d\n", (int) (pkt.cmd_type));
                continue;
            }

            CSPG_CMD_DBG_PRINT(" all ghosts received CMD %d [%s]\n", (int) pkt.cmd_type,
                               csp_cmd_name[pkt.cmd_type]);
            mpi_errno = CSPG_cmd_handlers[pkt.cmd_type] (&pkt);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        /* Exit after finalize is called. */
        if (CSP_PROC.ghost.is_finalized) {
            CSPG_CMD_DBG_PRINT(" exit from progress engine\n");
            goto fn_exit;
        }
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

/* Release current granted lock.
 * User function may lock ghosts to get exclusive access, after such function is finished,
 * the ghosts call this routine to release the lock. */
int CSPG_cmd_release_lock(void)
{
    int mpi_errno = MPI_SUCCESS;
    int local_gp_rank = 0;

    PMPI_Comm_rank(CSP_PROC.ghost.g_local_comm, &local_gp_rank);

    /* Only the first local ghost handles lock. */
    if (local_gp_rank == 0) {
        if (granted_lock_req) {
            CSPG_CMD_DBG_PRINT("\n");
            CSPG_CMD_DBG_PRINT(" ghost 0 release lock req %p (%d, %d)\n",
                               granted_lock_req, granted_lock_req->group_id,
                               granted_lock_req->user_local_rank);

            /* Release current lock */
            free(granted_lock_req);
            granted_lock_req = NULL;
            lock_req_cnt--;
        }

        mpi_errno = grant_next_lock_req();
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

/* Initialize ghost side command system.
 * It initializes command handlers and lock structures. */
void CSPG_cmd_init(void)
{
    CSPG_CMD_DBG_PRINT(" ghost start\n");
    CSPG_cmd_root_handlers[CSP_CMD_FNC_WIN_ALLOCATE] = CSPG_win_allocate_root_handler;
    CSPG_cmd_root_handlers[CSP_CMD_FNC_WIN_FREE] = CSPG_win_free_root_handler;
    CSPG_cmd_root_handlers[CSP_CMD_FNC_FINALIZE] = CSPG_finalize_root_handler;

    CSPG_cmd_root_handlers[CSP_CMD_LOCK_ACQUIRE] = CSPG_cmd_acquire_lock_handler;
    CSPG_cmd_root_handlers[CSP_CMD_LOCK_DISCARD] = CSPG_cmd_discard_lock_handler;
    CSPG_cmd_root_handlers[CSP_CMD_LOCK_RELEASE] = CSPG_cmd_release_lock_handler;

    CSPG_cmd_handlers[CSP_CMD_FNC_WIN_ALLOCATE] = CSPG_win_allocate_handler;
    CSPG_cmd_handlers[CSP_CMD_FNC_WIN_FREE] = CSPG_win_free_handler;
    CSPG_cmd_handlers[CSP_CMD_FNC_FINALIZE] = CSPG_finalize_handler;

    CSP_slist_init(CSPG_SLIST_ORDER_ASC, lock_req_compare_fnc, &susped_lock_reqs_list);
}

/* Destroy ghost side command system. */
void CSPG_cmd_destory(void)
{
    CSPG_cmd_lock_req_t *req = NULL;

    if (lock_req_cnt > 0) {
        CSPG_WARN_PRINT("%d lock requests are not freed !\n", lock_req_cnt);

        /* Release all existing lock requests */
        while (CSP_slist_count(&susped_lock_reqs_list) > 0) {
            req = CSP_slist_dequeue(&susped_lock_reqs_list);
            if (req)
                free(req);
        }

        if (granted_lock_req) {
            free(granted_lock_req);
            granted_lock_req = NULL;
        }
    }

    CSP_slist_destroy(&susped_lock_reqs_list);
    CSPG_CMD_DBG_PRINT(" All CMD LOCK requests are freed\n");
}

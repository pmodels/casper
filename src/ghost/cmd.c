/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspg.h"
#include "list.h"

#ifdef CSPG_CMD_DEBUG
#define CSPG_CMD_DBG_PRINT(str,...) do { \
    fprintf(stdout, "[CSPG-CMD][%d]"str, CSP_PROC.wrank, ## __VA_ARGS__); \
    fflush(stdout); \
    } while (0)

static const char *csp_cmd_lock_stat_name[CSP_CMD_LOCK_STAT_MAX] =
    { "none", "suspended_l", "suspended_h", "acquired" };
static const char *csp_cmd_lock_cmd_name[CSP_CMD_LOCK_MAX] =
    { "suspend", "acquire", "discard", "release" };
#else
#define CSPG_CMD_DBG_PRINT(str,...)
#endif

CSPG_cmd_fnc_handler_t fnc_cmd_handlers[CSP_CMD_FNC_MAX] = { NULL };
static CSPG_cmd_lock_handler_t lock_cmd_handlers[CSP_CMD_LOCK_MAX] = { NULL };

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

static inline int sync_lock_stat(CSPG_cmd_lock_req_t * req)
{
    CSP_cmd_lock_stat_pkt_t stat_pkt;

    stat_pkt.stat = req->stat;
    CSPG_CMD_DBG_PRINT(" \t sync CMD ACK %d [%s] to local user %d\n",
                       stat_pkt.stat, csp_cmd_lock_stat_name[stat_pkt.stat], req->user_local_rank);

    return PMPI_Send((char *) &stat_pkt, sizeof(CSP_cmd_lock_stat_pkt_t), MPI_CHAR,
                     req->user_local_rank, CSP_CMD_TAG, CSP_PROC.local_comm);
}

static inline int suspend_lock_req(CSPG_cmd_lock_req_t * req)
{
    CSPG_assert(granted_lock_req != NULL);
    if (req->group_id < granted_lock_req->group_id) {
        req->stat = CSP_CMD_LOCK_STAT_SUSPENDED_H;
    }
    else {
        req->stat = CSP_CMD_LOCK_STAT_SUSPENDED_L;
    }

    CSPG_CMD_DBG_PRINT(" \t suspend lock req %p (%d, %d, %s)\n", req,
                       req->group_id, req->user_local_rank,
                       (req->stat == CSP_CMD_LOCK_STAT_SUSPENDED_H) ? "H" : "L");

    /* Insert the new one in suspended list */
    if ((CSP_slist_insert(req, &susped_lock_reqs_list)) != 0)
        return MPI_ERR_OTHER;

    return sync_lock_stat(req);
}

static inline int grant_lock_req(CSPG_cmd_lock_req_t * req)
{
    req->stat = CSP_CMD_LOCK_STAT_ACQUIRED;
    granted_lock_req = req;

    CSPG_CMD_DBG_PRINT(" \t grant lock req %p (%d, %d)\n", req, req->group_id,
                       req->user_local_rank);
    return sync_lock_stat(req);
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
            CSP_cmd_lock_stat old_stat = req->stat;

            /* all high priority requests must be in front of any low priority
             * requests in sorted list. */
            if (old_stat == CSP_CMD_LOCK_STAT_SUSPENDED_L)
                break;

            req->stat = CSP_CMD_LOCK_STAT_SUSPENDED_L;
            mpi_errno = sync_lock_stat(req);
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

static int CSPG_cmd_acquire_lock_handler(CSP_cmd_lock_pkt_t * pkt, int user_local_rank)
{
    int mpi_errno = MPI_SUCCESS;
    CSPG_cmd_lock_req_t *req = NULL;

    /* Initialize new lock request */
    req = CSP_calloc(1, sizeof(CSPG_cmd_lock_req_t));
    if (req == NULL) {
        mpi_errno = MPI_ERR_NO_MEM;
        goto fn_fail;
    }

    req->group_id = pkt->extend.acquire.group_id;
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

static int CSPG_cmd_release_lock_handler(CSP_cmd_lock_pkt_t * pkt CSP_ATTRIBUTE((unused)),
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

    rels_req->stat = CSP_CMD_LOCK_STAT_NONE;
    mpi_errno = sync_lock_stat(rels_req);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    free(rels_req);
    lock_req_cnt--;

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

static int CSPG_cmd_discard_lock_handler(CSP_cmd_lock_pkt_t * pkt, int user_local_rank)
{
    int mpi_errno = MPI_SUCCESS;
    CSPG_cmd_lock_req_t *disc_req = NULL;
    CSPG_cmd_lock_req_t search_req;

    /* If lock is just granted before receiving discard message, release it. */
    if (granted_lock_req && user_local_rank == granted_lock_req->user_local_rank)
        return CSPG_cmd_release_lock_handler(pkt, user_local_rank);

    /* Remove from suspended list. */
    search_req.user_local_rank = user_local_rank;
    search_req.group_id = pkt->extend.discard.group_id;
    disc_req = CSP_slist_remove(&search_req, &susped_lock_reqs_list);
    CSPG_assert(disc_req != NULL);

    disc_req->stat = CSP_CMD_LOCK_STAT_NONE;
    mpi_errno = sync_lock_stat(disc_req);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    free(disc_req);
    lock_req_cnt--;

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

/* Receive command from any local user process (blocking call).
 * Only return source user process's local rank to the root ghost. */
int CSPG_cmd_recv(CSP_cmd_pkt_t * pkt)
{
    int mpi_errno = MPI_SUCCESS;
    int local_gp_rank = 0;
    MPI_Status stat;
    CSP_cmd_lock_pkt_t *lock_pkt = NULL;

    PMPI_Comm_rank(CSP_PROC.ghost.g_local_comm, &local_gp_rank);
    memset(pkt, 0, sizeof(CSP_cmd_pkt_t));

    /* Only the first local ghost receives command from any local user process.
     * Otherwise deadlock may happen if multiple user roots send request to
     * ghosts concurrently and some ghosts are locked in different communicator creation. */
    if (local_gp_rank == 0) {
        /* Lock command should only be handled by the first ghost,
         * thus continuously receive command until received a function command. */
        while (1) {
            mpi_errno = PMPI_Recv((char *) pkt, sizeof(CSP_cmd_pkt_t), MPI_CHAR,
                                  MPI_ANY_SOURCE, CSP_CMD_TAG, CSP_PROC.local_comm, &stat);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;

            if (pkt->cmd_type == CSP_CMD_FNC) {
                CSPG_CMD_DBG_PRINT(" ghost 0 received FNC CMD %d from %d\n",
                                   (int) (pkt->fnc.fnc_cmd), stat.MPI_SOURCE);
                break;
            }

            lock_pkt = &pkt->lock;
            /* skip unknown lock command */
            if (lock_pkt->lock_cmd >= CSP_CMD_LOCK_MAX || !lock_cmd_handlers[lock_pkt->lock_cmd]) {
                CSPG_CMD_DBG_PRINT(" Received unknown LOCK CMD %d\n", (int) (lock_pkt->lock_cmd));
                continue;
            }

            CSPG_CMD_DBG_PRINT(" ghost 0 received LOCK CMD %d [%s] from %d\n",
                               (int) (lock_pkt->lock_cmd),
                               csp_cmd_lock_cmd_name[lock_pkt->lock_cmd], stat.MPI_SOURCE);
            mpi_errno = lock_cmd_handlers[lock_pkt->lock_cmd] (lock_pkt, stat.MPI_SOURCE);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }
    }

    /* All other ghosts start handling function commands */
    mpi_errno = PMPI_Bcast((char *) pkt, sizeof(CSP_cmd_pkt_t), MPI_CHAR, 0,
                           CSP_PROC.ghost.g_local_comm);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;
    CSPG_CMD_DBG_PRINT(" all ghosts received FNC CMD %d\n", (int) pkt->fnc.fnc_cmd);

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
    fnc_cmd_handlers[CSP_CMD_FNC_WIN_ALLOCATE] = CSPG_win_allocate;
    fnc_cmd_handlers[CSP_CMD_FNC_WIN_FREE] = CSPG_win_free;
    fnc_cmd_handlers[CSP_CMD_FNC_FINALIZE] = CSPG_finalize;

    lock_cmd_handlers[CSP_CMD_LOCK_ACQUIRE] = CSPG_cmd_acquire_lock_handler;
    lock_cmd_handlers[CSP_CMD_LOCK_DISCARD] = CSPG_cmd_discard_lock_handler;
    lock_cmd_handlers[CSP_CMD_LOCK_RELEASE] = CSPG_cmd_release_lock_handler;

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

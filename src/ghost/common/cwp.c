/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cspg.h"
#include "cwp.h"

/* Command wire protocol (CWP) component on ghost processes */

#ifdef CSPG_CWP_DEBUG
#define CSPG_CWP_DBG_PRINT(str,...) do { \
    fprintf(stdout, "[CSPG-CWP][%d]"str, CSP_PROC.wrank, ## __VA_ARGS__); \
    fflush(stdout); \
    } while (0)

static const char *cwp_cmd_name[CSP_CWP_MAX] = {
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
#define CSPG_CWP_DBG_PRINT(str,...)
#endif

/* Command handlers on root ghosts.
 * The handler is called when received a command from the user process.*/
static CSPG_cwp_root_handler_t cwp_root_handlers[CSP_CWP_MAX] = { NULL };

/* Command handlers on children ghosts.
 * The handler is called when received a command broadcasted from the root ghost. */
static CSPG_cwp_handler_t cwp_handlers[CSP_CWP_MAX] = { NULL };


/* Receive command from any local user process (blocking call),
 * and process it in the corresponding command handler.
 * Only return when finalize command is done on all ghost processes. */
int CSPG_cwp_do_progress(void)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_cwp_pkt_t pkt;
    MPI_Status stat;
    int local_gp_rank = -1;

    PMPI_Comm_rank(CSP_PROC.ghost.g_local_comm, &local_gp_rank);
    memset(&pkt, 0, sizeof(CSP_cwp_pkt_t));

    while (1) {
        /* Only the first local ghost (root) receives commands from any local user process.
         * Otherwise deadlock may happen if multiple user roots send request to
         * ghosts concurrently and some ghosts are locked in different communicator creation. */
        if (local_gp_rank == 0) {
            mpi_errno = PMPI_Recv((char *) &pkt, sizeof(CSP_cwp_pkt_t), MPI_CHAR,
                                  MPI_ANY_SOURCE, CSP_CWP_TAG, CSP_PROC.local_comm, &stat);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;

            /* skip undefined command */
            if (pkt.cmd_type <= CSP_CWP_UNSET || pkt.cmd_type >= CSP_CWP_MAX ||
                !cwp_root_handlers[pkt.cmd_type]) {
                CSPG_CWP_DBG_PRINT(" Received undefined CMD %d\n", (int) (pkt.cmd_type));
                continue;
            }

            CSPG_CWP_DBG_PRINT(" ghost 0 received CMD %d [%s] from %d\n",
                               (int) (pkt.cmd_type), cwp_cmd_name[pkt.cmd_type], stat.MPI_SOURCE);
            mpi_errno = cwp_root_handlers[pkt.cmd_type] (&pkt, stat.MPI_SOURCE);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;

        }
        else {
            /* All other ghosts wait on internal commands broadcasted by the root,
             * which is issued in root's command handler. */
            mpi_errno = CSPG_cwp_bcast(&pkt);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;

            /* skip undefined internal command */
            if (pkt.cmd_type <= CSP_CWP_UNSET || pkt.cmd_type >= CSP_CWP_MAX ||
                !cwp_handlers[pkt.cmd_type]) {
                CSPG_CWP_DBG_PRINT(" Received undefined CMD %d\n", (int) (pkt.cmd_type));
                continue;
            }

            CSPG_CWP_DBG_PRINT(" all ghosts received CMD %d [%s]\n", (int) pkt.cmd_type,
                               cwp_cmd_name[pkt.cmd_type]);
            mpi_errno = cwp_handlers[pkt.cmd_type] (&pkt);
            if (mpi_errno != MPI_SUCCESS)
                goto fn_fail;
        }

        /* Exit after finalize is called. */
        if (CSP_PROC.ghost.is_finalized) {
            CSPG_CWP_DBG_PRINT(" exit from progress engine\n");
            goto fn_exit;
        }
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

void CSPG_cwp_register_root_handler(CSP_cwp_t cmd_type, CSPG_cwp_root_handler_t handler_fnc)
{
    if (cmd_type <= CSP_CWP_UNSET || cmd_type >= CSP_CWP_MAX) {
        CSPG_WARN_PRINT("register incorrect root handler, cmd_type=%d\n", cmd_type);
    }
    else {
        cwp_root_handlers[cmd_type] = handler_fnc;
    }
}

void CSPG_cwp_register_handler(CSP_cwp_t cmd_type, CSPG_cwp_handler_t handler_fnc)
{
    if (cmd_type <= CSP_CWP_UNSET || cmd_type >= CSP_CWP_MAX) {
        CSPG_WARN_PRINT("register incorrect handler, cmd_type=%d\n", cmd_type);
    }
    else {
        cwp_handlers[cmd_type] = handler_fnc;
    }
}

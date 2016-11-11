/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cspg.h"

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
#define CSPG_CWP_DBG_PRINT(str,...) do { } while (0)
#endif

/* Command handlers on root ghosts.
 * The handler is called when received a command from the user process.*/
static CSPG_cwp_root_handler_t cwp_root_handlers[CSP_CWP_MAX] = { NULL };

/* Command handlers on other ghosts.
 * The handler is called when received a command broadcasted from the root ghost. */
static CSPG_cwp_handler_t cwp_handlers[CSP_CWP_MAX] = { NULL };

/* Internal flag to notify the ghost process to exit from CWP progress engine.
 * The finalize handler calls `CSPG_cwp_terminate` to set it to 1 after all local
 * users have sent finalize command. The progress engine checks this flag to exit
 * from loop.*/
static int cwp_terminate_flag = 0;

/* Register root handler functions for user issued commands. */
void CSPG_cwp_register_root_handler(CSP_cwp_t cmd_type, CSPG_cwp_root_handler_t handler_fnc)
{
    if (cmd_type <= CSP_CWP_UNSET || cmd_type >= CSP_CWP_MAX) {
        CSPG_DBG_PRINT("register incorrect root handler, cmd_type=%d\n", cmd_type);
    }
    else {
        cwp_root_handlers[cmd_type] = handler_fnc;
    }
}

/* Register handler functions for root broadcasted commands. */
void CSPG_cwp_register_handler(CSP_cwp_t cmd_type, CSPG_cwp_handler_t handler_fnc)
{
    if (cmd_type <= CSP_CWP_UNSET || cmd_type >= CSP_CWP_MAX) {
        CSPG_DBG_PRINT("register incorrect handler, cmd_type=%d\n", cmd_type);
    }
    else {
        cwp_handlers[cmd_type] = handler_fnc;
    }
}

/* Receive command from any local user process (blocking call),
 * and process it in the corresponding command handler.
 * Only return when finalize command is done on all ghost processes. */
int CSPG_cwp_do_progress(void)
{
    int mpi_errno = MPI_SUCCESS;
    CSP_cwp_pkt_t pkt;
    MPI_Status stat;
    int local_gp_rank = -1;

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_PROC.ghost.g_local_comm, &local_gp_rank));
    memset(&pkt, 0, sizeof(CSP_cwp_pkt_t));

    while (1) {
        /* Only the first local ghost (root) receives commands from any local user process.
         * Otherwise deadlock may happen if multiple user roots send request to
         * ghosts concurrently and some ghosts are locked in different communicator creation. */
        if (local_gp_rank == 0) {
            CSP_CALLMPI(JUMP, PMPI_Recv((char *) &pkt, sizeof(CSP_cwp_pkt_t), MPI_CHAR,
                                        MPI_ANY_SOURCE, CSP_CWP_TAG, CSP_PROC.local_comm, &stat));

            /* skip undefined command */
            if (pkt.cmd_type <= CSP_CWP_UNSET || pkt.cmd_type >= CSP_CWP_MAX ||
                !cwp_root_handlers[pkt.cmd_type]) {
                CSPG_CWP_DBG_PRINT(" Received undefined CMD %d\n", (int) (pkt.cmd_type));
                continue;
            }

            CSPG_CWP_DBG_PRINT(" ghost 0 received CMD %d [%s] from %d\n",
                               (int) (pkt.cmd_type), cwp_cmd_name[pkt.cmd_type], stat.MPI_SOURCE);
            mpi_errno = cwp_root_handlers[pkt.cmd_type] (&pkt, stat.MPI_SOURCE);
            CSP_CHKMPIFAIL_JUMP(mpi_errno);

        }
        else {
            /* All other ghosts wait on internal commands broadcasted by the root,
             * which is issued in root's command handler. */
            mpi_errno = CSPG_cwp_bcast(&pkt);
            CSP_CHKMPIFAIL_JUMP(mpi_errno);

            /* skip undefined internal command */
            if (pkt.cmd_type <= CSP_CWP_UNSET || pkt.cmd_type >= CSP_CWP_MAX ||
                !cwp_handlers[pkt.cmd_type]) {
                CSPG_CWP_DBG_PRINT(" Received undefined CMD %d\n", (int) (pkt.cmd_type));
                continue;
            }

            CSPG_CWP_DBG_PRINT(" all ghosts received CMD %d [%s]\n", (int) pkt.cmd_type,
                               cwp_cmd_name[pkt.cmd_type]);
            mpi_errno = cwp_handlers[pkt.cmd_type] (&pkt);
            CSP_CHKMPIFAIL_JUMP(mpi_errno);
        }

        /* Terminate after received notification from finalize handler. */
        if (cwp_terminate_flag) {
            CSPG_CWP_DBG_PRINT(" exit from progress engine\n");
            goto fn_exit;
        }
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

/* Notify the CWP progress engine to terminate. */
void CSPG_cwp_terminate(void)
{
    cwp_terminate_flag = 1;
}

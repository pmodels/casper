/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2015 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cspu.h"

int MPI_Win_get_attr(MPI_Win win, int win_keyval, void *attribute_val, int *flag)
{
    CSPU_win_t *ug_win;
    int mpi_errno = MPI_SUCCESS;

    CSPU_fetch_ug_win_from_cache(win, &ug_win);

    if (ug_win == NULL) {
        /* normal window */
        return PMPI_Win_get_attr(win, win_keyval, attribute_val, flag);
    }

    /* Override win_create flavor for casper-allocated window.
     * Only win_create flavor is changed within Casper because the window is
     * internally created by WIN_CREATE. For all the other attributes, just return
     * its real value.*/
    if (win_keyval == MPI_WIN_CREATE_FLAVOR) {
        CSPU_THREAD_OBJ_CS_LOCAL_DCL();
        CSPU_THREAD_ENTER_OBJ_CS(ug_win);
        *((int **) attribute_val) = &ug_win->create_flavor;
        CSPU_THREAD_EXIT_OBJ_CS(ug_win);

        *flag = 1;
    }
    else {
        CSP_CALLMPI(NOSTMT, PMPI_Win_get_attr(win, win_keyval, attribute_val, flag));
    }

    return mpi_errno;
}

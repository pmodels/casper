#include <stdio.h>
#include <stdlib.h>
#include "csp.h"
#include "cspg.h"

int MPI_Init(int *argc, char ***argv)
{
    CSP_DBG_PRINT_FCNAME();

    return MPI_Init_thread(argc, argv, 0, NULL);
}

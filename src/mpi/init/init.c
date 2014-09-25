#include <stdio.h>
#include <stdlib.h>
#include "mtcore.h"
#include "mtcore_helper.h"

int MPI_Init(int *argc, char ***argv)
{
    MTCORE_DBG_PRINT_FCNAME();

    return MPI_Init_thread(argc, argv, 0, NULL);
}

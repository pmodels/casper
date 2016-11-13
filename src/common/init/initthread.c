/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csp.h"

/* Global environment setting */
CSP_env_param_t CSP_ENV;

/* Global process information object */
CSP_proc_t CSP_PROC;

/* User world communicator, including all users in the world */
MPI_Comm CSP_COMM_USER_WORLD = MPI_COMM_NULL;

#define CSP_SET_GLOBAL_COMM(gcomm, comm)   do {     \
    gcomm = comm;                                   \
    comm = MPI_COMM_NULL;   /* avoid local free */  \
} while (0)

static inline int check_valid_ghosts(void)
{
    int mpi_errno = MPI_SUCCESS;
    int local_nprocs;
    int err_flag = 0;

    CSP_CALLMPI(NOSTMT, PMPI_Comm_size(CSP_PROC.local_comm, &local_nprocs));
    if (mpi_errno != MPI_SUCCESS) {
        err_flag++;
        return err_flag;
    }

    if (local_nprocs < 2) {
        CSP_msg_print(CSP_MSG_ERROR, "Can not create shared memory region, %d process in "
                      "MPI_COMM_TYPE_SHARED subcommunicator.\n", local_nprocs);
        err_flag++;
    }

    if (CSP_ENV.num_g < 1 || CSP_ENV.num_g >= local_nprocs) {
        CSP_msg_print(CSP_MSG_ERROR, "Wrong value of number of ghosts, %d. lt 1 or ge %d.\n",
                      CSP_ENV.num_g, local_nprocs);
        err_flag++;
    }

    return err_flag;
}

static inline int setup_common_info(void)
{
    int mpi_errno = MPI_SUCCESS;
    int local_rank;
    MPI_Comm node_comm = MPI_COMM_WORLD;
    int tmp_bcast_buf[2] = { 0, 0 };

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_PROC.local_comm, &local_rank));
    CSP_CALLMPI(JUMP, PMPI_Comm_split(MPI_COMM_WORLD, local_rank == 0, 1, &node_comm));

    if (local_rank == 0) {
        CSP_CALLMPI(JUMP, PMPI_Comm_rank(node_comm, &tmp_bcast_buf[0]));        /* node_id */
        CSP_CALLMPI(JUMP, PMPI_Comm_size(node_comm, &tmp_bcast_buf[1]));        /* num_nodes */
    }

    CSP_CALLMPI(JUMP, PMPI_Bcast(tmp_bcast_buf, 2, MPI_INT, 0, CSP_PROC.local_comm));
    CSP_PROC.node_id = tmp_bcast_buf[0];
    CSP_PROC.num_nodes = tmp_bcast_buf[1];

  fn_exit:
    if (node_comm != MPI_COMM_NULL)
        CSP_CALLMPI_EXIT(PMPI_Comm_free(&node_comm));
    return mpi_errno;

  fn_fail:
    /* Free global objects in MPI_Init_thread. */
    goto fn_exit;
}

/* Initialize environment setting */
static int initialize_env(void)
{
    char *val;
    int mpi_errno = MPI_SUCCESS;

    memset(&CSP_ENV, 0, sizeof(CSP_ENV));

    /* VERBOSE level */
    CSP_ENV.verbose = (int) CSP_MSG_OFF;
    val = getenv("CSP_VERBOSE");
    if (val && strlen(val)) {
        char *vbs = NULL;

        vbs = strtok(val, ",|;");
        while (vbs != NULL) {
            if (!strncmp(vbs, "err", strlen("err"))) {
                CSP_ENV.verbose |= (int) CSP_MSG_ERROR;
            }
            else if (!strncmp(vbs, "warn", strlen("warn"))) {
                CSP_ENV.verbose |= (int) CSP_MSG_WARN;
            }
            else if (!strncmp(vbs, "conf_g", strlen("conf_g"))) {
                CSP_ENV.verbose |= (int) CSP_MSG_CONFIG_GLOBAL;
            }
            else if (!strncmp(vbs, "conf_win", strlen("conf_win"))) {
                CSP_ENV.verbose |= (int) CSP_MSG_CONFIG_WIN;
            }
            else if (!strncmp(vbs, "info", strlen("info"))) {
                CSP_ENV.verbose |= (int) CSP_MSG_INFO;
            }
            vbs = strtok(NULL, ",|;");
        }

        /* Also check shortcut for most useful verbosity */
        if (CSP_ENV.verbose == (int) CSP_MSG_OFF) {
            if (!strncmp(val, "1", strlen("1")) ||
                !strncmp(val, "y", strlen("y")) || !strncmp(val, "Y", strlen("Y"))) {
                CSP_ENV.verbose = (int) (CSP_MSG_ERROR | CSP_MSG_CONFIG_GLOBAL);
            }
        }
    }

    CSP_msg_init(CSP_ENV.verbose);

    CSP_ENV.num_g = CSP_DEFAULT_NG;
    val = getenv("CSP_NG");
    if (val && strlen(val)) {
        CSP_ENV.num_g = atoi(val);
    }
    if (CSP_ENV.num_g <= 0) {
        CSP_msg_print(CSP_MSG_ERROR, "Wrong CSP_NG %d\n", CSP_ENV.num_g);
        return CSP_get_error_code(CSP_ERR_NG);
    }

    CSP_ENV.async_config = CSP_ASYNC_CONFIG_ON;
    val = getenv("CSP_ASYNC_CONFIG");
    if (val && strlen(val)) {
        if (!strncmp(val, "on", strlen("on"))) {
            CSP_ENV.async_config = CSP_ASYNC_CONFIG_ON;
        }
        else if (!strncmp(val, "off", strlen("off"))) {
            CSP_ENV.async_config = CSP_ASYNC_CONFIG_OFF;
        }
        else {
            CSP_msg_print(CSP_MSG_ERROR, "Unknown CSP_ASYNC_CONFIG %s\n", val);
            return CSP_get_error_code(CSP_ERR_ENV);
        }
    }

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
    CSP_ENV.load_opt = CSP_LOAD_OPT_RANDOM;

    val = getenv("CSP_RUMTIME_LOAD_OPT");
    if (val && strlen(val)) {
        if (!strncmp(val, "random", strlen("random"))) {
            CSP_ENV.load_opt = CSP_LOAD_OPT_RANDOM;
        }
        else if (!strncmp(val, "op", strlen("op"))) {
            CSP_ENV.load_opt = CSP_LOAD_OPT_COUNTING;
        }
        else if (!strncmp(val, "byte", strlen("byte"))) {
            CSP_ENV.load_opt = CSP_LOAD_BYTE_COUNTING;
        }
        else {
            CSP_msg_print(CSP_MSG_ERROR, "Unknown CSP_RUMTIME_LOAD_OPT %s\n", val);
            return CSP_get_error_code(CSP_ERR_ENV);
        }
    }

    CSP_ENV.load_lock = CSP_LOAD_LOCK_NATURE;
    val = getenv("CSP_RUNTIME_LOAD_LOCK");
    if (val && strlen(val)) {
        if (!strncmp(val, "nature", strlen("nature"))) {
            CSP_ENV.load_lock = CSP_LOAD_LOCK_NATURE;
        }
        else if (!strncmp(val, "force", strlen("force"))) {
            CSP_ENV.load_lock = CSP_LOAD_LOCK_FORCE;
        }
        else {
            CSP_msg_print(CSP_MSG_ERROR, "Unknown CSP_RUNTIME_LOAD_LOCK %s\n", val);
            return CSP_get_error_code(CSP_ERR_ENV);
        }
    }
#else
    CSP_ENV.load_opt = CSP_LOAD_OPT_STATIC;
    CSP_ENV.load_lock = CSP_LOAD_LOCK_NATURE;
#endif

    if (CSP_PROC.wrank == 0) {
        CSP_msg_print(CSP_MSG_CONFIG_GLOBAL, "CASPER Configuration:  \n"
#ifdef CSP_ENABLE_RMA_ERR_CHECK
                      "    RMA_ERR_CHECK    (enabled) \n"
#endif
#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
                      "    RUMTIME_LOAD_OPT (enabled) \n"
#endif
                      "    CSP_VERBOSE      = %s|%s|%s|%s|%s\n"
                      "    CSP_NG           = %d\n"
                      "    CSP_ASYNC_CONFIG = %s\n",
                      (CSP_ENV.verbose & CSP_MSG_ERROR) ? "err" : "",
                      (CSP_ENV.verbose & CSP_MSG_WARN) ? "warn" : "",
                      (CSP_ENV.verbose & CSP_MSG_CONFIG_GLOBAL) ? "conf_g" : "",
                      (CSP_ENV.verbose & CSP_MSG_CONFIG_WIN) ? "conf_win" : "",
                      (CSP_ENV.verbose & CSP_MSG_INFO) ? "info" : "",
                      CSP_ENV.num_g, (CSP_ENV.async_config == CSP_ASYNC_CONFIG_ON) ? "on" : "off");

#if defined(CSP_ENABLE_RUNTIME_LOAD_OPT)
        CSP_msg_print(CSP_MSG_CONFIG_GLOBAL, "Runtime Load Balancing Options:  \n"
                      "    CSP_RUMTIME_LOAD_OPT = %s \n"
                      "    CSP_RUNTIME_LOAD_LOCK = %s \n",
                      (CSP_ENV.load_opt == CSP_LOAD_OPT_RANDOM) ? "random" :
                      ((CSP_ENV.load_opt == CSP_LOAD_OPT_COUNTING) ? "op" : "byte"),
                      (CSP_ENV.load_lock == CSP_LOAD_LOCK_NATURE) ? "nature" : "force");
#endif
        CSP_msg_print(CSP_MSG_CONFIG_GLOBAL, "\n");
    }
    return mpi_errno;
}

/* Initialize global communicator objects. */
static int initialize_proc(void)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Comm tmp_local_comm = MPI_COMM_NULL, tmp_ur_comm = MPI_COMM_NULL;
    MPI_Comm tmp_comm = MPI_COMM_NULL;
    int local_rank;

    /* Get a communicator only containing processes with shared memory */
    CSP_CALLMPI(JUMP, PMPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0,
                                           MPI_INFO_NULL, &CSP_PROC.local_comm));

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_PROC.local_comm, &local_rank));

    /* Statically set the lowest ranks on every node as ghosts */
    CSP_PROC.proc_type = (local_rank < CSP_ENV.num_g) ? CSP_PROC_GHOST : CSP_PROC_USER;

    /* Check if user specifies valid number of ghosts */
    if (check_valid_ghosts()) {
        mpi_errno = CSP_get_error_code(CSP_ERR_NG);
        goto fn_fail;
    }

    /* Reset user/ghost global object */
    CSP_reset_typed_proc();

    CSP_CALLMPI(JUMP, PMPI_Comm_group(MPI_COMM_WORLD, &CSP_PROC.wgroup));

    /* Create a user comm_world including all the users,
     * user will access it instead of comm_world */
    CSP_CALLMPI(JUMP, PMPI_Comm_split(MPI_COMM_WORLD, CSP_IS_USER, 1, &tmp_comm));

    if (CSP_IS_USER)
        CSP_SET_GLOBAL_COMM(CSP_COMM_USER_WORLD, tmp_comm);

    /* Create a user/ghost comm_local */
    CSP_CALLMPI(JUMP, PMPI_Comm_split(CSP_PROC.local_comm, CSP_IS_USER, 1, &tmp_local_comm));
    if (CSP_IS_USER) {
        CSP_SET_GLOBAL_COMM(CSP_PROC.user.u_local_comm, tmp_local_comm);
    }
    else {
        CSP_SET_GLOBAL_COMM(CSP_PROC.ghost.g_local_comm, tmp_local_comm);
    }

    if (CSP_IS_USER) {
        int local_user_rank = -1;
        CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_PROC.user.u_local_comm, &local_user_rank));

        /* Create a user root communicator including the first user on every node */
        CSP_CALLMPI(JUMP, PMPI_Comm_split(CSP_COMM_USER_WORLD, local_user_rank == 0,
                                          1, &tmp_ur_comm));

        if (local_user_rank == 0)
            CSP_SET_GLOBAL_COMM(CSP_PROC.user.ur_comm, tmp_ur_comm);

        /* Set name for user comm_world.  */
        CSP_CALLMPI(JUMP, PMPI_Comm_set_name(CSP_COMM_USER_WORLD, "MPI_COMM_WORLD"));
    }

    mpi_errno = setup_common_info();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

  fn_exit:
    /* Free unused communicators */
    if (tmp_comm != MPI_COMM_NULL)
        CSP_CALLMPI_EXIT(PMPI_Comm_free(&tmp_comm));
    if (tmp_ur_comm != MPI_COMM_NULL)
        CSP_CALLMPI_EXIT(PMPI_Comm_free(&tmp_ur_comm));
    return mpi_errno;

  fn_fail:
    /* Free global objects in MPI_Init_thread. */
    goto fn_exit;
}

int MPI_Init_thread(int *argc, char ***argv, int required, int *provided)
{
    int mpi_errno = MPI_SUCCESS;
    int is_threaded = 0;

    if (required == 0 && provided == NULL) {
        /* default init */
        CSP_CALLMPI(JUMP, PMPI_Init(argc, argv));
    }
    else {
        /* user init thread */
        CSP_CALLMPI(JUMP, PMPI_Init_thread(argc, argv, required, provided));

        if (required == MPI_THREAD_MULTIPLE && *provided == MPI_THREAD_MULTIPLE)
            is_threaded = 1;
    }

    /* Initialize global variables */
    memset(&CSP_PROC, 0, sizeof(CSP_proc_t));

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(MPI_COMM_WORLD, &CSP_PROC.wrank));

    mpi_errno = CSP_error_init();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* Initialize environment setting */
    mpi_errno = initialize_env();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    /* Initialize global process object */
    mpi_errno = initialize_proc();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    if (CSP_IS_USER) {
        /* Other user-specific initialization */
        mpi_errno = CSPU_global_init(is_threaded);
        CSP_CHKMPIFAIL_JUMP(mpi_errno);
    }
    else {
        /* Other ghost-specific initialization */
        mpi_errno = CSPG_global_init();
        CSP_CHKMPIFAIL_JUMP(mpi_errno);

        /* Start ghost main routine */
        CSPG_main();
        exit(0);
    }

  fn_exit:
    return mpi_errno;

  fn_fail:
    /* --BEGIN ERROR HANDLING-- */

    if (CSP_IS_USER) {
        CSPU_global_finalize();
    }
    else {
        CSPG_global_finalize();
    }

    CSP_ERROR_ABORT(mpi_errno);

    goto fn_exit;
    /* --END ERROR HANDLING-- */
}

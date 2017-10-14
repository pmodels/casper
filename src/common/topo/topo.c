/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <mpi.h>
#include "csp.h"

#ifdef TOPO_DEBUG
static int dbg_wrank = 0;
#define TOPO_DBG_PRINT(str,...) do { \
    fprintf(stdout, "[CSP] %s rank %d: "str, __FUNCTION__, dbg_wrank, ## __VA_ARGS__); \
    fflush(stdout); \
    } while (0)
#else
#define TOPO_DBG_PRINT(str,...) do { } while (0)
#endif

typedef struct CSP_topo_info {
    hwloc_topology_t topo;
    int npus;
} CSP_topo_info_t;

typedef struct CSP_topo_bind_info {
    int domain_idx;

    /* Debug use only */
    char mask[512];
    int npus;
} CSP_topo_bind_info_t;

typedef struct CSP_topo_map {
    int ndomains;
    int bind_ndomains;
    hwloc_obj_type_t domain_obj_type;
    MPI_Comm comm;
    int *domain_sizes;          /* Number of processes in each domain */
    CSP_topo_bind_info_t *bind_infos;
} CSP_topo_map_t;

static CSP_topo_info_t CSP_topo_info;

static inline int topo_init(void)
{
    int mpi_errno = MPI_SUCCESS;
    int num_machine = 1, num_core = 1;
    int hwloc_err = 0;

    hwloc_err = hwloc_topology_init(&CSP_topo_info.topo);
    if (hwloc_err < 0) {
        fprintf(stderr, "failed to init the topology\n");
        goto fn_fail;
    }

    hwloc_err = hwloc_topology_load(CSP_topo_info.topo);
    if (hwloc_err < 0) {
        fprintf(stderr, "failed to load the topology\n");
        hwloc_topology_destroy(CSP_topo_info.topo);
        goto fn_fail;
    }

    CSP_topo_info.npus = hwloc_get_nbobjs_by_type(CSP_topo_info.topo, HWLOC_OBJ_PU);

  fn_exit:
    return mpi_errno;
  fn_fail:
    mpi_errno = CSP_get_error_code(CSP_ERR_INTERN);
    goto fn_exit;
}

static inline void topo_destroy(void)
{
    hwloc_topology_destroy(CSP_topo_info.topo);
}

static inline int topo_get_cpubind(CSP_topo_bind_info_t * bind_info,
                                   hwloc_obj_type_t domain_obj_type)
{
    hwloc_const_bitmap_t cset_all;
    hwloc_bitmap_t myset = NULL;
    hwloc_obj_t bind_obj = NULL, domain_obj = NULL;
    int hwloc_err = 0;

    /* Reset indexes */
    bind_info->domain_idx = -1;

    cset_all = hwloc_topology_get_topology_cpuset(CSP_topo_info.topo);

    /* Get CPU binding of the current process */
    myset = hwloc_bitmap_alloc();
    if (!myset) {
        fprintf(stderr, "Failed to allocate a bitmap\n");
        goto fn_fail;
    }

    hwloc_err = hwloc_get_cpubind(CSP_topo_info.topo, myset, HWLOC_CPUBIND_PROCESS);
    if (hwloc_err < 0) {
        fprintf(stderr, "Failed to get cpu binding\n");
        goto fn_fail;
    }

    if (hwloc_bitmap_isequal(myset, cset_all)) {
        TOPO_DBG_PRINT("No binding found \n");
    }
    else {
        /* Find domain index */
        domain_obj = hwloc_get_next_obj_by_type(CSP_topo_info.topo, domain_obj_type, NULL);
        while (domain_obj != NULL) {
            if (hwloc_bitmap_isincluded(myset, domain_obj->cpuset)) {
                bind_info->domain_idx = domain_obj->logical_index;
                break;
            }
            domain_obj = hwloc_get_next_obj_by_type(CSP_topo_info.topo,
                                                    domain_obj_type, domain_obj);
        }
        /* FIXME: throw more user friendly error. */
        CSP_ASSERT(bind_info->domain_idx >= 0);

        /* Info printing only. Encode bound PU indexes into a string. */
        int puprev = -1, puidx = -1, strpos = 0;
        memset(bind_info->mask, sizeof(bind_info->mask), 0);
        bind_info->npus = hwloc_bitmap_weight(myset);
        do {
            puidx = hwloc_bitmap_next(myset, puprev);
            puprev = puidx;

            if (puidx >= 0 && sizeof(bind_info->mask) - strpos > sizeof(int)) {
                int off = 0;
                off = sprintf(bind_info->mask + strpos, "%d,", puidx);
                strpos += off;  /* terminate char is not counted */
            }
        } while (puidx != -1);
        /* Remove last comma. */
        sprintf(bind_info->mask + strpos - 1, "%c", '\0');
    }

  fn_exit:
    if (!myset)
        hwloc_bitmap_free(myset);
    return hwloc_err;
  fn_fail:
    goto fn_exit;
}

static inline void topo_free_map(CSP_topo_map_t * topo_map)
{
    if (topo_map->bind_infos != NULL)
        free(topo_map->bind_infos);
    if (topo_map->domain_sizes != NULL)
        free(topo_map->domain_sizes);
    topo_map->bind_infos = NULL;
    topo_map->domain_sizes = NULL;
}

static inline int topo_load_comm_map(CSP_topo_domain_type_t domain_type, MPI_Comm comm,
                                     CSP_topo_map_t * topo_map)
{
    hwloc_const_bitmap_t cset_all;
    hwloc_bitmap_t myset = NULL;        /* FIXME: not portable. */
    hwloc_obj_t bind_obj;
    hwloc_obj_type_t domain_obj_type;
    int hwloc_err = 0;
    int mpi_errno = MPI_SUCCESS;
    int i, myrank, size;

    switch (domain_type) {
    case CSP_TOPO_DOMAIN_MACHINE:
        domain_obj_type = HWLOC_OBJ_MACHINE;
        break;
    case CSP_TOPO_DOMAIN_NUMA:
        domain_obj_type = HWLOC_OBJ_NUMANODE;
        break;
    case CSP_TOPO_DOMAIN_SOCK:
        domain_obj_type = HWLOC_OBJ_PACKAGE;
        break;
    default:
        CSP_msg_print(CSP_MSG_ERROR, " Unknown topology domain type %d\n", domain_type);
        goto fn_fail;
    }

    topo_map->comm = comm;
    MPI_Comm_rank(topo_map->comm, &myrank);
    MPI_Comm_size(topo_map->comm, &size);

    topo_map->ndomains = hwloc_get_nbobjs_by_type(CSP_topo_info.topo, domain_obj_type);
    topo_map->domain_sizes = (int *) CSP_calloc(topo_map->ndomains, sizeof(int));
    topo_map->bind_infos = (CSP_topo_bind_info_t *) CSP_calloc(size, sizeof(CSP_topo_bind_info_t));
    topo_map->domain_obj_type = domain_obj_type;

    /* get my bind info */
    hwloc_err = topo_get_cpubind(&topo_map->bind_infos[myrank], domain_obj_type);
    if (hwloc_err < 0)
        goto fn_fail;

    /* exchange */
    CSP_CALLMPI(JUMP, PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                                     topo_map->bind_infos, sizeof(CSP_topo_bind_info_t), MPI_BYTE,
                                     comm));
    for (i = 0; i < size; i++) {
        CSP_ASSERT(topo_map->bind_infos[i].domain_idx < topo_map->ndomains);
        topo_map->domain_sizes[topo_map->bind_infos[i].domain_idx]++;
    }

    /* Get number of bound domains */
    topo_map->bind_ndomains = 0;
    for (i = 0; i < topo_map->ndomains; i++) {
        if (topo_map->domain_sizes[i] > 0)
            topo_map->bind_ndomains++;
    }

  fn_exit:
    return mpi_errno;
  fn_fail:
    mpi_errno = CSP_get_error_code(CSP_ERR_INTERN);
    goto fn_exit;
}

static inline void topo_print_comm_map(CSP_topo_map_t topo_map)
{
    int i, comm_myrank, comm_size;

    MPI_Comm_size(topo_map.comm, &comm_size);
    MPI_Comm_rank(topo_map.comm, &comm_myrank);

    if (comm_myrank == 0 && (CSP_ENV.verbose & (int) CSP_MSG_INFO)) {
        for (i = 0; i < comm_size; i++) {
            CSP_msg_print(CSP_MSG_INFO, "TOPO: rank %d, domain_idx=%d "
                          "(type %s x %d/%d) bound %d PUs(%s)\n",
                          i, topo_map.bind_infos[i].domain_idx,
                          hwloc_obj_type_string(topo_map.domain_obj_type),
                          topo_map.ndomains, topo_map.bind_ndomains,
                          topo_map.bind_infos[i].npus, topo_map.bind_infos[i].mask);
        }
    }
}

static inline int topo_check_remap(CSP_topo_map_t topo_map, int *local_remap_flag,
                                   int *global_remap_flag)
{
    int mpi_errno = MPI_SUCCESS;
    int local_rank, local_nproc;
    int domain_num_g, domain_np;
    int didx, i, map_uneven = 0;
    int ordered = 1;

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_PROC.local_comm, &local_rank));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(CSP_PROC.local_comm, &local_nproc));

    domain_np = local_nproc / topo_map.bind_ndomains;
    domain_num_g = CSP_ENV.num_g / topo_map.bind_ndomains;

    *local_remap_flag = 1;

    /* Do remapping only when more than one domain is set and ghosts is sufficient. */
    if (topo_map.bind_ndomains < 2 || domain_num_g < 1) {
        *local_remap_flag = 0;
        if (local_rank == 0)
            CSP_msg_print(CSP_MSG_INFO, "TOPO: insufficient domains or ghosts, no remap\n");
        goto no_local_remap;
    }

    /* Do not remap uneven or mismatch domain distribution. */
    for (didx = 0; didx < topo_map.ndomains; didx++) {
        if (topo_map.domain_sizes[didx] > 0)    /* ignore zero domain */
            map_uneven |= (domain_np != topo_map.domain_sizes[didx]);
    }
    if (map_uneven) {
        *local_remap_flag = 0;
        if (local_rank == 0)
            CSP_msg_print(CSP_MSG_INFO, "TOPO: uneven, no remap\n");
        goto no_local_remap;
    }

    /* Do not remap if optimal binding is already set. */
    for (i = 0; i < CSP_ENV.num_g; i++)
        ordered &= (topo_map.bind_infos[i].domain_idx == i / domain_num_g);
    for (i = CSP_ENV.num_g; i < local_nproc; i++)
        ordered &= (topo_map.bind_infos[i].domain_idx ==
                    (i - CSP_ENV.num_g) / (domain_np - domain_num_g));
    if (ordered) {
        if (local_rank == 0)
            CSP_msg_print(CSP_MSG_INFO, "TOPO: ordered, no remap\n");
        *local_remap_flag = 0;
    }

  no_local_remap:
    /* Check if any node reordered */
    CSP_CALLMPI(JUMP, PMPI_Allreduce(local_remap_flag, global_remap_flag, 1, MPI_INT, MPI_BOR,
                                     MPI_COMM_WORLD));

    TOPO_DBG_PRINT("Check topology: local remap %d, global remap %d\n",
                   *local_remap_flag, *global_remap_flag);

  fn_exit:
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

int CSP_topo_remap(void)
{
    int mpi_errno = MPI_SUCCESS;
    int local_rank, local_nproc;
    CSP_topo_map_t topo_map;
    int *remap_ranks = NULL, *domains_num_g_set = NULL, *world_remap_ranks = NULL;
    MPI_Comm old_local_comm = MPI_COMM_NULL;
    MPI_Group old_lgroup = MPI_GROUP_NULL, old_wgroup = MPI_GROUP_NULL;
    int local_remap_flag = 0, global_remap_flag = 0;

    mpi_errno = topo_init();
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

#ifdef TOPO_DEBUG
    CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_PROC.wcomm, &dbg_wrank));
#endif

    CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_PROC.local_comm, &local_rank));
    CSP_CALLMPI(JUMP, PMPI_Comm_size(CSP_PROC.local_comm, &local_nproc));
    CSP_CALLMPI(JUMP, PMPI_Comm_group(CSP_PROC.local_comm, &old_lgroup));

    CSP_DBG_PRINT("before remap,I am %d in world, %d in local\n", CSP_PROC.wrank, local_rank);

    mpi_errno = topo_load_comm_map(CSP_ENV.topo.domain, CSP_PROC.local_comm, &topo_map);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    mpi_errno = topo_check_remap(topo_map, &local_remap_flag, &global_remap_flag);
    CSP_CHKMPIFAIL_JUMP(mpi_errno);

    if (local_remap_flag) {
        int g_idx, u_idx, didx, i;
        int domain_np, domain_num_g;

        domain_np = local_nproc / topo_map.bind_ndomains;
        domain_num_g = CSP_ENV.num_g / topo_map.bind_ndomains;

        remap_ranks = (int *) CSP_calloc(local_nproc, sizeof(int));
        domains_num_g_set = (int *) CSP_calloc(topo_map.ndomains, sizeof(int));

        /* Reorder ranks: pick up the first domain_num_g units from each domain
         * as ghosts, move them to the lowest ranks. */
        g_idx = 0;
        u_idx = domain_num_g * topo_map.bind_ndomains;
        for (i = 0; i < local_nproc; i++) {
            didx = topo_map.bind_infos[i].domain_idx;
            if (domains_num_g_set[didx] < domain_num_g) {
                remap_ranks[g_idx++] = i;
                domains_num_g_set[didx]++;
            }
            else {
                remap_ranks[u_idx++] = i;
            }
        }

        CSP_ASSERT(g_idx == domain_num_g * topo_map.bind_ndomains);
        CSP_ASSERT(u_idx == local_nproc);
#ifdef TOPO_DEBUG
        if (local_rank == 0) {
            for (i = 0; i < local_nproc; i++)
                printf("local remap_ranks[%d] %d\n", i, remap_ranks[i]);
            fflush(stdout);
        }
#endif
    }

    /* Local remap on any node will cause a global remap. */
    if (global_remap_flag) {
        int wrank, wnproc;

        CSP_CALLMPI(JUMP, PMPI_Comm_rank(MPI_COMM_WORLD, &wrank));
        CSP_CALLMPI(JUMP, PMPI_Comm_size(MPI_COMM_WORLD, &wnproc));

        world_remap_ranks = (int *) CSP_calloc(wnproc, sizeof(int));

        if (local_remap_flag) {
            /* Get the old world rank with my local offset if local remapped. */
            CSP_CALLMPI(JUMP, PMPI_Group_translate_ranks(old_lgroup, 1,
                                                         &remap_ranks[local_rank],
                                                         CSP_PROC.wgroup,
                                                         &world_remap_ranks[wrank]));
        }
        else {
            world_remap_ranks[wrank] = wrank;
        }

        CSP_CALLMPI(JUMP, PMPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                                         world_remap_ranks, 1, MPI_INT, MPI_COMM_WORLD));

        /* Create remapped comm_world and overwrite global group/comm. */
        old_wgroup = CSP_PROC.wgroup;
        CSP_CALLMPI(JUMP, PMPI_Group_incl(old_wgroup, wnproc, world_remap_ranks, &CSP_PROC.wgroup));
        CSP_CALLMPI(JUMP, PMPI_Comm_create_group(MPI_COMM_WORLD, CSP_PROC.wgroup, 0,
                                                 &CSP_PROC.wcomm));

        CSP_CALLMPI(JUMP, PMPI_Comm_rank(CSP_PROC.wcomm, &CSP_PROC.wrank));

        /* Create remapped local comm and overwrite global group/comm. */
        old_local_comm = CSP_PROC.local_comm;
        CSP_CALLMPI(JUMP, PMPI_Comm_split_type(CSP_PROC.wcomm, MPI_COMM_TYPE_SHARED, 0,
                                               MPI_INFO_NULL, &CSP_PROC.local_comm));

        if (CSP_ENV.verbose & (int) CSP_MSG_INFO) {
            topo_free_map(&topo_map);
            topo_load_comm_map(CSP_ENV.topo.domain, CSP_PROC.local_comm, &topo_map);
            if (wrank == 0)
                CSP_msg_print(CSP_MSG_INFO, "TOPO: reordered\n");
            CSP_CALLMPI(JUMP, PMPI_Barrier(CSP_PROC.wcomm));
            topo_print_comm_map(topo_map);
        }
    }

  fn_exit:
    if (remap_ranks)
        free(remap_ranks);
    if (domains_num_g_set)
        free(domains_num_g_set);
    if (world_remap_ranks)
        free(world_remap_ranks);
    if (old_lgroup != MPI_GROUP_NULL)
        CSP_CALLMPI_EXIT(PMPI_Group_free(&old_lgroup));
    if (old_wgroup != MPI_GROUP_NULL)
        CSP_CALLMPI_EXIT(PMPI_Group_free(&old_wgroup));
    if (old_local_comm != MPI_COMM_NULL)
        CSP_CALLMPI_EXIT(PMPI_Comm_free(&old_local_comm));

    topo_free_map(&topo_map);

    /* No topology work afterward */
    topo_destroy();
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

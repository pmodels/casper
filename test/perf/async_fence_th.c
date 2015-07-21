/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>

#include <assert.h>
#include <sched.h>
#include <pthread.h>
#include <sched.h>
#include "ctest.h"

/* This benchmark evaluates manual thread-based asynchronous progress in fence
 * epoch. Every process performs fence-compute-accumulate-fence, and each of
 * them creates an asynchronous thread to poll MPI progress .*/

/* OS-dependent implementations */

#ifdef HAVE_SYS_SYSINFO_H
#include <sys/sysinfo.h>
#endif

#ifdef HAVE_SYS_SYSCTL_H
#include <sys/types.h>
#include <sys/sysctl.h>
/* We don't need to maintain u_* types separately, since configure will report
 * sys/sysctl.h is not compilable if u_* is not defined.*/
#endif

static inline int get_num_cores()
{
    int n = 1;

#if defined(HAVE_GET_NPROCS)
    n = get_nprocs();
#elif defined(HAVE_GET_NPROCS_CONF)
    n = get_nprocs_conf();
#elif defined(_SC_NPROCESSORS_ONLN)
    n = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(_SC_NPROC_ONLN)
    n = sysconf(_SC_NPROC_ONLN);
#elif defined(_SC_NPROCESSORS_CONF)
    n = sysconf(_SC_NPROCESSORS_CONF);
#elif defined(_SC_NPROC_CONF)
    n = sysconf(_SC_NPROC_CONF);
#elif defined(HAVE_SYSCTL) && defined(CTL_HW) && defined(HW_NCPU)
    static int name[2] = { CTL_HW, HW_NCPU };
    size_t size = sizeof(n);
    if (sysctl(name, 2, &n, &size, NULL, 0) || size != sizeof(n))
        n = -1;
#else
    fprintf(stderr, "Warning: cannot detect number of processes, return 1 by default\n");
#endif
    return n;
}

#if defined(HAVE_CPU_SET_T) && defined(HAVE_PTHREAD_GETAFFINITY_NP)
#if defined(HAVE_PTHREAD_NP_H)
#include <pthread_np.h>
#endif

static int get_pthread_cpu_bind(int *cpuid_ptr, int num_cores)
{
    cpu_set_t cpuset;
    int i, ret = 0;

    *cpuid_ptr = 0;
    CPU_ZERO(&cpuset);
    ret = pthread_getaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    if (ret < 0) {
        fprintf(stderr, "pthread_getaffinity_np failed\n");
        return ret;
    }

    for (i = 0; i < num_cores; i++) {
        if (CPU_ISSET(i, &cpuset))
            *cpuid_ptr = i;
    }
    return 0;
}
#else
static int get_pthread_cpu_bind(int *cpuid_ptr, int num_cores)
{
    *cpuid_ptr = 0;
    fprintf(stderr, "Warning: cannot detect affinity, return 0 by default\n");
    return 0;
}
#endif

#define D_SLEEP_TIME 100        // 100us

//#define DEBUG
//#define CHECK
#define ITER_S 10000
#define ITER_M 5000
#define ITER_L 1000
#define NPROCS_M 6

#ifdef DEBUG
#define debug_printf(str,...) {fprintf(stdout, str, ## __VA_ARGS__);fflush(stdout);}
#else
#define debug_printf(str,...) {}
#endif

#define TH_DEBUG
#ifdef TH_DEBUG
#define th_debug_print(str,...) {fprintf(stdout, str, ## __VA_ARGS__);fflush(stdout);}
#else
#define th_debug_print(str,...) {}
#endif

double *winbuf = NULL;
double *locbuf = NULL;
int rank, nprocs, nprocs_local, cpuid, th_cpuid, ncores;
MPI_Win win = MPI_WIN_NULL;
int ITER = ITER_S;
int NOP = 100;

static int usleep_by_count(unsigned long us)
{
    double start = MPI_Wtime() * 1000 * 1000;
    while (MPI_Wtime() * 1000 * 1000 - start < (double) us);
    return 0;
}

static int run_test(int comp_time)
{
    int i, x, errs = 0, errs_total = 0;
    int dst;
    double t0, avg_total_time = 0.0, t_total = 0.0;

    if (nprocs < NPROCS_M) {
        ITER = ITER_S;
    }
    else if (nprocs >= NPROCS_M && nprocs < NPROCS_M * 2) {
        ITER = ITER_M;
    }
    else {
        ITER = ITER_L;
    }

    t0 = MPI_Wtime();
    for (x = 0; x < ITER; x++) {
        MPI_Win_fence(MPI_MODE_NOPRECEDE, win);

        usleep_by_count(comp_time);

        for (dst = 0; dst < nprocs; dst++) {
            for (i = 1; i < NOP; i++) {
                MPI_Accumulate(&locbuf[i], 1, MPI_DOUBLE, dst, rank, 1, MPI_DOUBLE, MPI_SUM, win);
            }
        }
        MPI_Win_fence(MPI_MODE_NOSUCCEED, win);
    }
    t_total = MPI_Wtime() - t0;
    t_total /= ITER;

    MPI_Reduce(&t_total, &avg_total_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Allreduce(&errs, &errs_total, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    if (rank == 0) {
        avg_total_time = avg_total_time / nprocs * 1000 * 1000;
        fprintf(stdout,
                "thread: iter %d comp_size %d num_op %d nprocs %d total_time %.2lf\n",
                ITER, comp_time, NOP, nprocs, avg_total_time);
    }

    return errs_total;
}

static MPI_Comm progress_comm;
static pthread_t progress_thread_id;
static pthread_mutex_t progress_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t progress_cond = PTHREAD_COND_INITIALIZER;
static volatile int progress_thread_done = 0;
#define WAKE_TAG 9898
static volatile int thread_inited = 0;

static void *progress_fn(void *arg CTEST_ATTRIBUTE((unused)))
{
    int err;
    MPI_Request request;
    MPI_Status status;

    get_pthread_cpu_bind(&th_cpuid, ncores);

    MPI_Irecv(NULL, 0, MPI_CHAR, rank, WAKE_TAG, progress_comm, &request);
    MPI_Wait(&request, &status);

    err = pthread_mutex_lock(&progress_mutex);
    assert(!err);

    progress_thread_done = 1;
    err = pthread_mutex_unlock(&progress_mutex);
    assert(!err);

    pthread_cond_signal(&progress_cond);
    assert(!err);

    return (void *) (0);
}

static int init_async_thread(void)
{
    int err = 0;
    pthread_attr_t attr;

    /* Dup comm world for the progress thread */
    MPI_Comm_dup(MPI_COMM_WORLD, &progress_comm);

    err = pthread_attr_init(&attr);
    assert(!err);
    err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    assert(!err);

    err = pthread_create(&progress_thread_id, &attr, &progress_fn, NULL);
    assert(!err);

    pthread_attr_destroy(&attr);
    thread_inited = 1;

    th_debug_print("[%d]Thread created\n", rank);

    return err;
}

static void finalize_async_thread(void)
{
    int err = 0;
    MPI_Request request;
    MPI_Status status;

    if (!thread_inited)
        return;

    th_debug_print("[%d]Start finishing thread\n", rank);

    MPI_Isend(NULL, 0, MPI_CHAR, rank, WAKE_TAG, progress_comm, &request);
    MPI_Wait(&request, &status);

    err = pthread_mutex_lock(&progress_mutex);
    assert(!err);

    while (!progress_thread_done) {
        err = pthread_cond_wait(&progress_cond, &progress_mutex);
        assert(!err);
    }

    err = pthread_mutex_unlock(&progress_mutex);
    assert(!err);

    th_debug_print("[%d]Received thread done\n", rank);

    MPI_Comm_free(&progress_comm);

    err = pthread_cond_destroy(&progress_cond);
    assert(!err);

    err = pthread_mutex_destroy(&progress_mutex);
    assert(!err);

    return;
}

static void check_cpu_binding()
{
    int i;
    int cpuids[2];
    int *all_cpuids = calloc(2 * nprocs, sizeof(int));
    int *cpuid_bitmap = calloc(ncores, sizeof(int));

    cpuids[0] = cpuid;
    cpuids[1] = th_cpuid;

    MPI_Gather(cpuids, 2, MPI_INT, all_cpuids, 2, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        for (i = 0; i < ncores; i++) {
            cpuid_bitmap[i] = -1;
        }

        for (i = 0; i < nprocs * 2; i++) {
            if (cpuid_bitmap[all_cpuids[i]] == -1) {
                cpuid_bitmap[all_cpuids[i]] = i;
            }
            else {
                fprintf(stderr, "%d and %d are binded to the same cpu id %d\n",
                        cpuid_bitmap[all_cpuids[i]], i, all_cpuids[i]);
            }
        }
    }

    free(all_cpuids);
    free(cpuid_bitmap);
}

int main(int argc, char *argv[])
{
    int i, errs;
    int min_time = D_SLEEP_TIME, max_time = D_SLEEP_TIME, iter_time = 2, comp_time = 0;
    int provided;

    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

    if (provided != MPI_THREAD_MULTIPLE) {
        printf("This test requires MPI_THREAD_MULTIPLE, but %d\n", provided);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    ncores = get_num_cores();
    get_pthread_cpu_bind(&cpuid, ncores);

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (nprocs < 2) {
        fprintf(stderr, "Please run using at least 2 processes\n");
        goto exit;
    }

    if (argc >= 4) {
        min_time = atoi(argv[1]);
        max_time = atoi(argv[2]);
        iter_time = atoi(argv[3]);
    }
    if (argc >= 5) {
        NOP = atoi(argv[4]);
    }

    locbuf = malloc(sizeof(double) * NOP);
    for (i = 0; i < NOP; i++) {
        locbuf[i] = 1.0;
    }

    // size in byte
    MPI_Win_allocate(sizeof(double) * nprocs, sizeof(double), MPI_INFO_NULL,
                     MPI_COMM_WORLD, &winbuf, &win);
    debug_printf("[%d]win_allocate done\n", rank);

    init_async_thread();
    MPI_Barrier(MPI_COMM_WORLD);

    for (comp_time = min_time; comp_time <= max_time; comp_time *= iter_time) {
        /* reset window */
        MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win);
        for (i = 0; i < nprocs; i++) {
            winbuf[i] = 0.0;
        }
        MPI_Win_unlock(rank, win);

        MPI_Barrier(MPI_COMM_WORLD);

        errs = run_test(comp_time);
        if (errs > 0)
            break;
    }

    check_cpu_binding();

  exit:

    if (win != MPI_WIN_NULL)
        MPI_Win_free(&win);
    if (locbuf)
        free(locbuf);

    finalize_async_thread();
    MPI_Finalize();

    return 0;
}

/* Title: LU factorization with 1D row partition by MPI RMA
 *
 * Author: Xin Zhao <xinzhao3@illinois.edu>
 * Date  : Sept, 2013
 *
 * Description:
 *
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <mpi.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#ifdef MTCORE
extern int MTCORE_NUM_H;
#endif

#define RAND_RANGE  (32767)

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

void printSubMatrix(double *sub_matrix, int block_num, int block_size, int matrix_size);
void initializeSubMatrix();
void computeLU();
void checkSubMatrix();
void finalizeSubMatrix();
void initWindow();
void freeWindow();

int myrank, myrank_lu, nprocs, work_nprocs;
int matrix_size = 4, block_size = 1, block_num = 0;     /* default values for user-defined parameters */
int validate_flag = 0;

double *local_rows = NULL, *result_local_rows = NULL;
double t0 = 0.0, t1 = 0.0, elapse_t = 0.0;
int curr_iter = 0, iter_cnt = 1, omit_cnt = 0;
MPI_Comm lu_comm;

double *comm_buf, *compute_buf, *temp_buf;
MPI_Win comm_win, compute_win, temp_win;

int main(int argc, char *argv[])
{
    int i;
    /* get user-defined parameters */
#ifdef MTCORE
    if (argc > 2) {
        /* skip first parameter in manticore */
        for (i = 2; i < argc; i++) {
#else
    if (argc > 1) {
        for (i = 0; i < argc; i++) {
#endif
            if (!strcmp(argv[i], "-m"))
                matrix_size = atoi(argv[++i]);
            else if (!strcmp(argv[i], "-b"))
                block_size = atoi(argv[++i]);
            else if (!strcmp(argv[i], "-c"))
                iter_cnt = atoi(argv[++i]);
            else if (!strcmp(argv[i], "-o"))
                omit_cnt = atoi(argv[++i]);
            else if (!strcmp(argv[i], "-v"))
                validate_flag = 1;
        }

        if (matrix_size <= 0 || block_size <= 0 || matrix_size % block_size != 0) {
            printf("Arguments do not satisfy program assumptions:\n"
                   "\t matrix_size > 0, block_size > 0, and\n"
                   "\t block_size perfectly divides matrix_size.\n"
                   "\t Currently matrix_size=%d, block_size=%d.\n", matrix_size, block_size);
            exit(-1);
        }
    }

    /* initialize MPI environment */
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    /* set actual number of working processes */
    work_nprocs = MIN(nprocs, (matrix_size / block_size));

    /* create LU communicator for working processes */
    MPI_Comm_split(MPI_COMM_WORLD, (myrank < work_nprocs), myrank, &lu_comm);

    if (myrank < work_nprocs) {

        /* calculate my rank in LU comm */
        MPI_Comm_rank(lu_comm, &myrank_lu);

        /* calculate number of blocks on each process */
        block_num = (matrix_size / block_size) / work_nprocs;
        int remainder = (matrix_size / block_size) % work_nprocs;
        if (remainder != 0 && myrank_lu < remainder)
            block_num++;

        initWindow();

        /* allocate buffer for local rows */
        MPI_Alloc_mem(matrix_size * block_size * block_num * sizeof(double), MPI_INFO_NULL,
                      &local_rows);
        MPI_Alloc_mem(matrix_size * block_size * block_num * sizeof(double), MPI_INFO_NULL,
                      &result_local_rows);

        /* do the work */
        for (curr_iter = 0; curr_iter < iter_cnt; curr_iter++) {
            initializeSubMatrix();
            computeLU();
            if (validate_flag)
                checkSubMatrix();
        }
        finalizeSubMatrix();

        /* calculate average spent time */
        {
            double sum_elapse_t = 0.0;
            MPI_Reduce(&elapse_t, &sum_elapse_t, 1, MPI_DOUBLE, MPI_SUM, 0 /*root */ , lu_comm);
            if (myrank_lu == 0) {

#ifdef MTCORE
                printf("mtcore: matrix %d block %d nprocs %d nh %d %.4f\n",
                       matrix_size, block_size, work_nprocs, MTCORE_NUM_H,
                       ((sum_elapse_t / work_nprocs) * 1000000) / (iter_cnt - omit_cnt));
#else
                printf("orig: matrix %d block %d nprocs %d %.4f\n",
                       matrix_size, block_size, work_nprocs,
                       ((sum_elapse_t / work_nprocs) * 1000000) / (iter_cnt - omit_cnt));
#endif
            }
        }

        freeWindow();
    }

    /* free LU communicator */
    MPI_Comm_free(&lu_comm);

    MPI_Finalize();

    return 0;
}

void initWindow()
{
    MPI_Info win_info;
    /* create info */
//    MPI_Info_create(&win_info);
//    MPI_Info_set(win_info, "alloc_shm", "false");       /* NOTE: a bug in MPICH for PSCW with SHM window */
//    MPI_Info_set(win_info, "same_size", "true");
    win_info = MPI_INFO_NULL;

    /* allocate RMA windows */
    MPI_Win_allocate(matrix_size * sizeof(double), sizeof(double),
                     win_info, lu_comm, &comm_buf, &comm_win);
    MPI_Win_allocate(matrix_size * sizeof(double), sizeof(double),
                     win_info, lu_comm, &compute_buf, &compute_win);

    /* free info */
    if (win_info != MPI_INFO_NULL)
        MPI_Info_free(&win_info);
}

void freeWindow()
{
    /* free RMA windows */
    MPI_Win_free(&comm_win);
    MPI_Win_free(&compute_win);
}

void initializeSubMatrix()
{
    srand(time(NULL) * myrank_lu);      /* same seed will lead to same random number on different process within the node... */

    /* initialize local rows */
    int i;
    for (i = 0; i < block_num * block_size * matrix_size; i++) {
        do {
            local_rows[i] = result_local_rows[i] = (double) rand() / (RAND_MAX / RAND_RANGE + 1);
        } while (local_rows[i] == 0.0);
    }
}

void computeLU()
{
    int i, j, k, m;
    int bbuf[1];

    /* initialize buffers */
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, myrank_lu, 0, comm_win);
    memset(comm_buf, 0, matrix_size * sizeof(double));
    MPI_Win_unlock(myrank_lu, comm_win);
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, myrank_lu, 0, compute_win);
    memset(compute_buf, 0, matrix_size * sizeof(double));
    MPI_Win_unlock(myrank_lu, compute_win);

    if (curr_iter >= omit_cnt)
        t0 = MPI_Wtime();

    MPI_Barrier(lu_comm);       /* start LU */

    for (k = -1; k < matrix_size - 1; k++) {
        int n_req = 0;
        MPI_Request request[2];
        int origin_rank = ((k + 1) / block_size) % work_nprocs;

        if (myrank_lu == origin_rank) {
            MPI_Win_lock_all(0, comm_win);
        }

        if (k == -1) {
            /* broadcast the first row */
            if (myrank_lu == origin_rank) {
                for (i = 0; i < work_nprocs; i++) {
                    /* find out if we really needed to send message to rank m... */
                    int p, q, comm_flag = 0;
                    int block_num_i = (matrix_size / block_size) / work_nprocs;
                    int remainder = (matrix_size / block_size) % work_nprocs;
                    if (remainder != 0 && i < remainder)
                        block_num_i++;
                    for (p = 0; p < block_num_i; p++) {
                        for (q = 0; q < block_size; q++) {
                            int orig_row = (p * work_nprocs + i) * block_size + q;
                            if (orig_row > k + 1) {
                                comm_flag = 1;
                                break;
                            }
                        }
                    }

                    if (comm_flag)
                        MPI_Put(&result_local_rows[0 + (k + 1)], matrix_size - (k + 1), MPI_DOUBLE,
                                i, (k + 1), matrix_size - (k + 1), MPI_DOUBLE, comm_win);
                }
            }
        }
        else {
            for (i = k + 1; i < matrix_size; i++) {
                int current_row_rank = (i / block_size) % work_nprocs;
                if (current_row_rank == myrank_lu) {
                    /* compute new value for this row */
                    int myrow = ((i / block_size) / work_nprocs) * block_size + i % block_size;
                    assert(compute_buf[k] != 0.0);

                    result_local_rows[k + myrow * matrix_size] /= compute_buf[k];
                    for (j = k + 1; j < matrix_size; j++)
                        result_local_rows[j + myrow * matrix_size] -=
                            result_local_rows[k + myrow * matrix_size] * compute_buf[j];

                    if (i == k + 1) {
                        assert(origin_rank == myrank_lu);

                        for (m = 0; m < work_nprocs; m++) {
                            /* find out if we really needed to send message to rank m... */
                            int p, q, comm_flag = 0;
                            int block_num_m = (matrix_size / block_size) / work_nprocs;
                            int remainder = (matrix_size / block_size) % work_nprocs;
                            if (remainder != 0 && m < remainder)
                                block_num_m++;
                            for (p = 0; p < block_num_m; p++) {
                                for (q = 0; q < block_size; q++) {
                                    int orig_row = (p * work_nprocs + m) * block_size + q;
                                    if (orig_row > k + 1) {
                                        comm_flag = 1;
                                        break;
                                    }
                                }
                            }

                            if (comm_flag)
                                MPI_Put(&result_local_rows[myrow * matrix_size + (k + 1)],
                                        matrix_size - (k + 1), MPI_DOUBLE, m, (k + 1),
                                        matrix_size - (k + 1), MPI_DOUBLE, comm_win);
                        }
                    }
                }
            }
        }

        if (myrank_lu == origin_rank) {
            MPI_Win_unlock_all(comm_win);
        }
        /* notify target processes */
//        MPI_Bcast(bbuf, 1, MPI_INT, origin_rank, lu_comm);
        MPI_Barrier(lu_comm);

        temp_win = comm_win;
        comm_win = compute_win;
        compute_win = temp_win;
        temp_buf = comm_buf;
        comm_buf = compute_buf;
        compute_buf = temp_buf;
    }

    MPI_Barrier(lu_comm);       /* end LU */

    if (curr_iter >= omit_cnt) {
        t1 = MPI_Wtime();
        elapse_t += t1 - t0;
    }
}

void checkSubMatrix()
{
    int i, j, k, m, n, p, q, bogus = 0, tile = 4;
    double *temp_cols;
    double max_diff = 0.0, diff = 0.0;

    assert(matrix_size % tile == 0);

    MPI_Alloc_mem(matrix_size * tile * sizeof(double), MPI_INFO_NULL, &temp_cols);

    for (i = 0; i < matrix_size; i += tile) {
        /* exchange current column for matrix multiplication */
        memset(temp_cols, 0, matrix_size * tile * sizeof(double));
        for (j = 0; j < tile; j++) {
            for (p = 0; p <= i + j; p++) {
                int target_rank = (p / block_size) % work_nprocs;
                if (target_rank == myrank_lu) {
                    int target_row = ((p / block_size) / work_nprocs) * block_size + p % block_size;
                    temp_cols[p + j * matrix_size] =
                        result_local_rows[i + j + target_row * matrix_size];
                }
            }
        }

        MPI_Allreduce(MPI_IN_PLACE, temp_cols, matrix_size * tile, MPI_DOUBLE, MPI_SUM, lu_comm);

        /* compute matrix multiplication */
        for (m = 0; m < block_num; m++) {
            for (n = 0; n < block_size; n++) {
                for (j = 0; j < tile; j++) {
                    /* check if LU == A */
                    double current_result = 0.0;
                    for (k = 0; k < matrix_size; k++) {
                        double tmp_L = 0.0;
                        int orig_row = (m * work_nprocs + myrank_lu) * block_size + n;  /* row id in the whole matrix */
                        if (k <= orig_row) {
                            if (k == orig_row)
                                tmp_L = 1.0;
                            else
                                tmp_L =
                                    result_local_rows[k + n * matrix_size +
                                                      m * matrix_size * block_size];
                        }
                        current_result += tmp_L * temp_cols[k + j * matrix_size];
                    }

                    diff =
                        local_rows[i + j + n * matrix_size + m * block_size * matrix_size] -
                        current_result;
                    if (fabs(diff) > 0.00001) {
                        bogus = 1;
                        if (fabs(diff) > max_diff)
                            max_diff = fabs(diff);
                    }
                }
            }
        }
    }

    if (bogus)
        printf("[rank=%d]TEST FAILED: (%.5f diff).\n", myrank_lu, max_diff);

    MPI_Free_mem(temp_cols);
}

void finalizeSubMatrix()
{
    /* free local sub matrix */
    MPI_Free_mem(local_rows);
    MPI_Free_mem(result_local_rows);
}

void printSubMatrix(double *sub_matrix, int block_num, int block_size, int matrix_size)
{
    int i, j, k;
    for (i = 0; i < block_num; i++) {
        printf("[block_num=%d]\n", i);
        for (j = 0; j < block_size; j++) {
            for (k = 0; k < matrix_size; k++) {
                printf("[%.5f]", sub_matrix[k + j * matrix_size + i * block_size * matrix_size]);
            }
            printf("\n");
        }
        printf("\n");
    }
}

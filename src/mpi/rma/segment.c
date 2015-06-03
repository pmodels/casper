/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2014 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "csp.h"

void CSP_Op_segments_destroy(CSP_OP_Segment ** decoded_ops_ptr)
{
    if (*decoded_ops_ptr) {
        free(*decoded_ops_ptr);
    }
}

int CSP_Op_segments_decode_basic_datatype(const void *origin_addr,
                                          int origin_count ATTRIBUTE((unused)),
                                          MPI_Datatype origin_datatype, int target_rank,
                                          MPI_Aint target_disp, int target_count,
                                          MPI_Datatype target_datatype, CSP_Win * ug_win,
                                          CSP_OP_Segment ** decoded_ops_ptr, int *num_segs)
{
    int mpi_errno = MPI_SUCCESS;
    int o_type_size, t_type_size;
    MPI_Aint target_base_off, target_data_size;
    CSP_OP_Segment *decoded_ops = NULL;
    MPI_Aint dt_size = 0, op_sg_size = 0, op_sg_base = 0, sg_base = 0, sg_size = 0;
    int sg_off = 0, op_sg_off = 0;

    PMPI_Type_size(origin_datatype, &o_type_size);
    PMPI_Type_size(target_datatype, &t_type_size);

    *num_segs = -1;

    target_base_off = target_disp * t_type_size;
    target_data_size = target_count * t_type_size;

    if (target_base_off + target_data_size > ug_win->targets[target_rank].size) {
        fprintf(stderr, "Wrong operation target_disp 0x%lx, target_count %d "
                "(base 0x%lx + size 0x%lx > 0x%lx)\n",
                target_disp, target_count, target_base_off, target_data_size,
                ug_win->targets[target_rank].size);
        return -1;
    }

    decoded_ops = calloc(ug_win->targets[target_rank].num_segs, sizeof(CSP_OP_Segment));
    if (decoded_ops == NULL)
        goto fn_fail;

    op_sg_base = target_base_off;
    while (dt_size < target_data_size) {
        CSP_Assert(op_sg_off < ug_win->targets[target_rank].num_segs);
        CSP_Assert(sg_off < ug_win->targets[target_rank].num_segs);

        sg_base = ug_win->targets[target_rank].segs[sg_off].base_offset;
        sg_size = ug_win->targets[target_rank].segs[sg_off].size;

        if (sg_base <= op_sg_base && sg_base + sg_size > op_sg_base) {
            op_sg_size = min(sg_size - op_sg_base + sg_base, target_data_size - dt_size);

            decoded_ops[op_sg_off].origin_addr = (void *) ((MPI_Aint) origin_addr + dt_size);   /* byte unit */
            decoded_ops[op_sg_off].origin_datatype = origin_datatype;
            decoded_ops[op_sg_off].origin_count = op_sg_size / o_type_size;
            decoded_ops[op_sg_off].target_seg_off = sg_off;
            decoded_ops[op_sg_off].target_disp = op_sg_base / t_type_size;      /*dt unit */
            decoded_ops[op_sg_off].target_datatype = target_datatype;
            decoded_ops[op_sg_off].target_count = op_sg_size / t_type_size;
            decoded_ops[op_sg_off].target_dtsize = op_sg_size;

            /* next operation segment */
            dt_size += op_sg_size;
            op_sg_base += op_sg_size;
            op_sg_off++;
        }

        /* next target segment */
        sg_off++;
    }

    *num_segs = op_sg_off;
    *decoded_ops_ptr = decoded_ops;

  fn_exit:
    return mpi_errno;

  fn_fail:
    if (decoded_ops)
        free(decoded_ops);
    goto fn_exit;
}

int CSP_Op_segments_decode(const void *origin_addr, int origin_count,
                           MPI_Datatype origin_datatype,
                           int target_rank, MPI_Aint target_disp,
                           int target_count, MPI_Datatype target_datatype,
                           CSP_Win * ug_win, CSP_OP_Segment ** decoded_ops_ptr, int *num_segs)
{
    int mpi_errno = MPI_SUCCESS;
    int o_combiner = 0, o_num_integers = 0, o_num_datatypes = 0, o_num_addresses = 0;
    int t_combiner = 0, t_num_integers = 0, t_num_datatypes = 0, t_num_addresses = 0;

    mpi_errno = MPI_Type_get_envelope(target_datatype, &o_num_integers,
                                      &o_num_addresses, &o_num_datatypes, &o_combiner);
    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;

    mpi_errno = MPI_Type_get_envelope(target_datatype, &t_num_integers,
                                      &t_num_addresses, &t_num_datatypes, &t_combiner);
    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;

    /* Both sides are basic datatype */
    if (o_combiner == MPI_COMBINER_NAMED && t_combiner == MPI_COMBINER_NAMED) {
        return CSP_Op_segments_decode_basic_datatype(origin_addr, origin_count,
                                                     origin_datatype, target_rank, target_disp,
                                                     target_count, target_datatype, ug_win,
                                                     decoded_ops_ptr, num_segs);
    }
    /* Derived origin datatype and basic target datatype */
    else if (t_combiner == MPI_COMBINER_NAMED) {
        fprintf(stderr, "Origin derived datatype is not supported for now.\n");
        mpi_errno = -1;
    }
    /* Basic origin datatype and derived target datatype */
    else if (o_combiner == MPI_COMBINER_NAMED) {
        fprintf(stderr, "Target derived datatype is not supported for now.\n");
        mpi_errno = -1;
    }
    else {
        fprintf(stderr,
                "Both origin and target are derived datatype, it is not supported for now.\n");
        mpi_errno = -1;
    }

    return mpi_errno;
}

#include <stdio.h>
#include <stdlib.h>
#include "asp.h"

#ifdef HAVE_ROMIO 


/* -- Begin Profiling Symbol Block for routine MPI_Init */
#if defined(HAVE_PRAGMA_WEAK) 
#pragma weak MPI_File_call_errhandler = MPIASP_File_call_errhandler
#pragma weak MPI_File_create_errhandler = MPIASP_File_create_errhandler
#pragma weak MPI_File_get_errhandler = MPIASP_File_get_errhandler
#pragma weak MPI_File_set_errhandler = MPIASP_File_set_errhandler
#pragma weak MPI_File_close = MPIASP_File_close
#pragma weak MPI_File_delete = MPIASP_File_delete
#pragma weak MPI_File_c2f = MPIASP_File_c2f
#pragma weak MPI_File_f2c = MPIASP_File_f2c
#pragma weak MPI_File_sync = MPIASP_File_sync
#pragma weak MPI_File_get_amode = MPIASP_File_get_amode
#pragma weak MPI_File_get_atomicity = MPIASP_File_get_atomicity
#pragma weak MPI_File_get_byte_offset = MPIASP_File_get_byte_offset
#pragma weak MPI_File_get_type_extent = MPIASP_File_get_type_extent
#pragma weak MPI_File_get_group = MPIASP_File_get_group
#pragma weak MPI_File_get_info = MPIASP_File_get_info
#pragma weak MPI_File_get_position = MPIASP_File_get_position
#pragma weak MPI_File_get_position_shared = MPIASP_File_get_position_shared
#pragma weak MPI_File_get_size = MPIASP_File_get_size
#pragma weak MPI_File_get_view = MPIASP_File_get_view
#pragma weak MPI_File_iread = MPIASP_File_iread
#pragma weak MPI_File_iread_at = MPIASP_File_iread_at
#pragma weak MPI_File_iread_shared = MPIASP_File_iread_shared
#pragma weak MPI_File_iwrite = MPIASP_File_iwrite
#pragma weak MPI_File_iwrite_at = MPIASP_File_iwrite_at
#pragma weak MPI_File_iwrite_shared = MPIASP_File_iwrite_shared
#pragma weak MPI_File_open = MPIASP_File_open
#pragma weak MPI_File_preallocate = MPIASP_File_preallocate
#pragma weak MPI_File_read_at_all_begin = MPIASP_File_read_at_all_begin
#pragma weak MPI_File_read_at_all_end = MPIASP_File_read_at_all_end
#pragma weak MPI_File_read = MPIASP_File_read
#pragma weak MPI_File_read_all = MPIASP_File_read_all
#pragma weak MPI_File_read_all_begin = MPIASP_File_read_all_begin
#pragma weak MPI_File_read_all_end = MPIASP_File_read_all_end
#pragma weak MPI_File_read_at = MPIASP_File_read_at
#pragma weak MPI_File_read_at_all = MPIASP_File_read_at_all
#pragma weak MPI_File_read_ordered = MPIASP_File_read_ordered
#pragma weak MPI_File_read_ordered_begin = MPIASP_File_read_ordered_begin
#pragma weak MPI_File_read_ordered_end = MPIASP_File_read_ordered_end
#pragma weak MPI_File_read_shared = MPIASP_File_read_shared
#pragma weak MPI_File_seek = MPIASP_File_seek
#pragma weak MPI_File_seek_shared = MPIASP_File_seek_shared
#pragma weak MPI_File_set_atomicity = MPIASP_File_set_atomicity
#pragma weak MPI_File_set_info = MPIASP_File_set_info
#pragma weak MPI_File_set_size = MPIASP_File_set_size
#pragma weak MPI_File_set_view = MPIASP_File_set_view
#pragma weak MPI_File_write = MPIASP_File_write
#pragma weak MPI_File_write_all = MPIASP_File_write_all
#pragma weak MPI_File_write_all_begin = MPIASP_File_write_all_begin
#pragma weak MPI_File_write_all_end = MPIASP_File_write_all_end
#pragma weak MPI_File_write_at = MPIASP_File_write_at
#pragma weak MPI_File_write_at_all = MPIASP_File_write_at_all
#pragma weak MPI_File_write_ordered = MPIASP_File_write_ordered
#pragma weak MPI_File_write_ordered_begin = MPIASP_File_write_ordered_begin
#pragma weak MPI_File_write_ordered_end = MPIASP_File_write_ordered_end
#pragma weak MPI_File_write_shared = MPIASP_File_write_shared
#pragma weak MPI_File_write_at_all_begin = MPIASP_File_write_at_all_begin
#pragma weak MPI_File_write_at_all_end = MPIASP_File_write_at_all_end

#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF MPIASP_File_call_errhandler MPI_File_call_errhandler
#pragma _HP_SECONDARY_DEF MPIASP_File_create_errhandler MPI_File_create_errhandler
#pragma _HP_SECONDARY_DEF MPIASP_File_get_errhandler MPI_File_get_errhandler
#pragma _HP_SECONDARY_DEF MPIASP_File_set_errhandler MPI_File_set_errhandler
#pragma _HP_SECONDARY_DEF MPIASP_File_close MPI_File_close
#pragma _HP_SECONDARY_DEF MPIASP_File_delete MPI_File_delete
#pragma _HP_SECONDARY_DEF MPIASP_File_c2f MPI_File_c2f
#pragma _HP_SECONDARY_DEF MPIASP_File_f2c MPI_File_f2c
#pragma _HP_SECONDARY_DEF MPIASP_File_sync MPI_File_sync
#pragma _HP_SECONDARY_DEF MPIASP_File_get_amode MPI_File_get_amode
#pragma _HP_SECONDARY_DEF MPIASP_File_get_atomicity MPI_File_get_atomicity
#pragma _HP_SECONDARY_DEF MPIASP_File_get_byte_offset MPI_File_get_byte_offset
#pragma _HP_SECONDARY_DEF MPIASP_File_get_type_extent MPI_File_get_type_extent
#pragma _HP_SECONDARY_DEF MPIASP_File_get_group MPI_File_get_group
#pragma _HP_SECONDARY_DEF MPIASP_File_get_info MPI_File_get_info
#pragma _HP_SECONDARY_DEF MPIASP_File_get_position MPI_File_get_position
#pragma _HP_SECONDARY_DEF MPIASP_File_get_position_shared MPI_File_get_position_shared
#pragma _HP_SECONDARY_DEF MPIASP_File_get_size MPI_File_get_size
#pragma _HP_SECONDARY_DEF MPIASP_File_get_view MPI_File_get_view
#pragma _HP_SECONDARY_DEF MPIASP_File_iread MPI_File_iread
#pragma _HP_SECONDARY_DEF MPIASP_File_iread_at MPI_File_iread_at
#pragma _HP_SECONDARY_DEF MPIASP_File_iread_shared MPI_File_iread_shared
#pragma _HP_SECONDARY_DEF MPIASP_File_iwrite MPI_File_iwrite
#pragma _HP_SECONDARY_DEF MPIASP_File_iwrite_at MPI_File_iwrite_at
#pragma _HP_SECONDARY_DEF MPIASP_File_iwrite_shared MPI_File_iwrite_shared
#pragma _HP_SECONDARY_DEF MPIASP_File_open MPI_File_open
#pragma _HP_SECONDARY_DEF MPIASP_File_preallocate MPI_File_preallocate
#pragma _HP_SECONDARY_DEF MPIASP_File_read_at_all_begin MPI_File_read_at_all_begin
#pragma _HP_SECONDARY_DEF MPIASP_File_read_at_all_end MPI_File_read_at_all_end
#pragma _HP_SECONDARY_DEF MPIASP_File_read MPI_File_read
#pragma _HP_SECONDARY_DEF MPIASP_File_read_all MPI_File_read_all
#pragma _HP_SECONDARY_DEF MPIASP_File_read_all_begin MPI_File_read_all_begin
#pragma _HP_SECONDARY_DEF MPIASP_File_read_all_end MPI_File_read_all_end
#pragma _HP_SECONDARY_DEF MPIASP_File_read_at MPI_File_read_at
#pragma _HP_SECONDARY_DEF MPIASP_File_read_at_all MPI_File_read_at_all
#pragma _HP_SECONDARY_DEF MPIASP_File_read_ordered MPI_File_read_ordered
#pragma _HP_SECONDARY_DEF MPIASP_File_read_ordered_begin MPI_File_read_ordered_begin
#pragma _HP_SECONDARY_DEF MPIASP_File_read_ordered_end MPI_File_read_ordered_end
#pragma _HP_SECONDARY_DEF MPIASP_File_read_shared MPI_File_read_shared
#pragma _HP_SECONDARY_DEF MPIASP_File_seek MPI_File_seek
#pragma _HP_SECONDARY_DEF MPIASP_File_seek_shared MPI_File_seek_shared
#pragma _HP_SECONDARY_DEF MPIASP_File_set_atomicity MPI_File_set_atomicity
#pragma _HP_SECONDARY_DEF MPIASP_File_set_info MPI_File_set_info
#pragma _HP_SECONDARY_DEF MPIASP_File_set_size MPI_File_set_size
#pragma _HP_SECONDARY_DEF MPIASP_File_set_view MPI_File_set_view
#pragma _HP_SECONDARY_DEF MPIASP_File_write MPI_File_write
#pragma _HP_SECONDARY_DEF MPIASP_File_write_all MPI_File_write_all
#pragma _HP_SECONDARY_DEF MPIASP_File_write_all_begin MPI_File_write_all_begin
#pragma _HP_SECONDARY_DEF MPIASP_File_write_all_end MPI_File_write_all_end
#pragma _HP_SECONDARY_DEF MPIASP_File_write_at MPI_File_write_at
#pragma _HP_SECONDARY_DEF MPIASP_File_write_at_all MPI_File_write_at_all
#pragma _HP_SECONDARY_DEF MPIASP_File_write_ordered MPI_File_write_ordered
#pragma _HP_SECONDARY_DEF MPIASP_File_write_ordered_begin MPI_File_write_ordered_begin
#pragma _HP_SECONDARY_DEF MPIASP_File_write_ordered_end MPI_File_write_ordered_end
#pragma _HP_SECONDARY_DEF MPIASP_File_write_shared MPI_File_write_shared
#pragma _HP_SECONDARY_DEF MPIASP_File_write_at_all_begin MPI_File_write_at_all_begin
#pragma _HP_SECONDARY_DEF MPIASP_File_write_at_all_end MPI_File_write_at_all_end

#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPI_File_call_errhandler as MPIASP_File_call_errhandler
#pragma _CRI duplicate MPI_File_create_errhandler as MPIASP_File_create_errhandler
#pragma _CRI duplicate MPI_File_get_errhandler as MPIASP_File_get_errhandler
#pragma _CRI duplicate MPI_File_set_errhandler as MPIASP_File_set_errhandler
#pragma _CRI duplicate MPI_File_close as MPIASP_File_close
#pragma _CRI duplicate MPI_File_delete as MPIASP_File_delete
#pragma _CRI duplicate MPI_File_c2f as MPIASP_File_c2f
#pragma _CRI duplicate MPI_File_f2c as MPIASP_File_f2c
#pragma _CRI duplicate MPI_File_sync as MPIASP_File_sync
#pragma _CRI duplicate MPI_File_get_amode as MPIASP_File_get_amode
#pragma _CRI duplicate MPI_File_get_atomicity as MPIASP_File_get_atomicity
#pragma _CRI duplicate MPI_File_get_byte_offset as MPIASP_File_get_byte_offset
#pragma _CRI duplicate MPI_File_get_type_extent as MPIASP_File_get_type_extent
#pragma _CRI duplicate MPI_File_get_group as MPIASP_File_get_group
#pragma _CRI duplicate MPI_File_get_info as MPIASP_File_get_info
#pragma _CRI duplicate MPI_File_get_position as MPIASP_File_get_position
#pragma _CRI duplicate MPI_File_get_position_shared as MPIASP_File_get_position_shared
#pragma _CRI duplicate MPI_File_get_size as MPIASP_File_get_size
#pragma _CRI duplicate MPI_File_get_view as MPIASP_File_get_view
#pragma _CRI duplicate MPI_File_iread as MPIASP_File_iread
#pragma _CRI duplicate MPI_File_iread_at as MPIASP_File_iread_at
#pragma _CRI duplicate MPI_File_iread_shared as MPIASP_File_iread_shared
#pragma _CRI duplicate MPI_File_iwrite as MPIASP_File_iwrite
#pragma _CRI duplicate MPI_File_iwrite_at as MPIASP_File_iwrite_at
#pragma _CRI duplicate MPI_File_iwrite_shared as MPIASP_File_iwrite_shared
#pragma _CRI duplicate MPI_File_open as MPIASP_File_open
#pragma _CRI duplicate MPI_File_preallocate as MPIASP_File_preallocate
#pragma _CRI duplicate MPI_File_read_at_all_begin as MPIASP_File_read_at_all_begin
#pragma _CRI duplicate MPI_File_read_at_all_end as MPIASP_File_read_at_all_end
#pragma _CRI duplicate MPI_File_read as MPIASP_File_read
#pragma _CRI duplicate MPI_File_read_all as MPIASP_File_read_all
#pragma _CRI duplicate MPI_File_read_all_begin as MPIASP_File_read_all_begin
#pragma _CRI duplicate MPI_File_read_all_end as MPIASP_File_read_all_end
#pragma _CRI duplicate MPI_File_read_at as MPIASP_File_read_at
#pragma _CRI duplicate MPI_File_read_at_all as MPIASP_File_read_at_all
#pragma _CRI duplicate MPI_File_read_ordered as MPIASP_File_read_ordered
#pragma _CRI duplicate MPI_File_read_ordered_begin as MPIASP_File_read_ordered_begin
#pragma _CRI duplicate MPI_File_read_ordered_end as MPIASP_File_read_ordered_end
#pragma _CRI duplicate MPI_File_read_shared as MPIASP_File_read_shared
#pragma _CRI duplicate MPI_File_seek as MPIASP_File_seek
#pragma _CRI duplicate MPI_File_seek_shared as MPIASP_File_seek_shared
#pragma _CRI duplicate MPI_File_set_atomicity as MPIASP_File_set_atomicity
#pragma _CRI duplicate MPI_File_set_info as MPIASP_File_set_info
#pragma _CRI duplicate MPI_File_set_size as MPIASP_File_set_size
#pragma _CRI duplicate MPI_File_set_view as MPIASP_File_set_view
#pragma _CRI duplicate MPI_File_write as MPIASP_File_write
#pragma _CRI duplicate MPI_File_write_all as MPIASP_File_write_all
#pragma _CRI duplicate MPI_File_write_all_begin as MPIASP_File_write_all_begin
#pragma _CRI duplicate MPI_File_write_all_end as MPIASP_File_write_all_end
#pragma _CRI duplicate MPI_File_write_at as MPIASP_File_write_at
#pragma _CRI duplicate MPI_File_write_at_all as MPIASP_File_write_at_all
#pragma _CRI duplicate MPI_File_write_ordered as MPIASP_File_write_ordered
#pragma _CRI duplicate MPI_File_write_ordered_begin as MPIASP_File_write_ordered_begin
#pragma _CRI duplicate MPI_File_write_ordered_end as MPIASP_File_write_ordered_end
#pragma _CRI duplicate MPI_File_write_shared as MPIASP_File_write_shared
#pragma _CRI duplicate MPI_File_write_at_all_begin as MPIASP_File_write_at_all_begin
#pragma _CRI duplicate MPI_File_write_at_all_end as MPIASP_File_write_at_all_end
#endif
/* -- End Profiling Symbol Block */

#undef FCNAME
#define FCNAME MPIASP_File_call_errhandler
int MPIASP_File_call_errhandler(MPI_File fh, int errorcode) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_call_errhandler(fh, errorcode);
}

#undef FCNAME
#define FCNAME MPIASP_File_create_errhandler
int MPIASP_File_create_errhandler(MPI_File_errhandler_function *function,
        MPI_Errhandler *errhandler) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_create_errhandler(function, errhandler);
}

#undef FCNAME
#define FCNAME MPIASP_File_get_errhandler
int MPIASP_File_get_errhandler(MPI_File file, MPI_Errhandler *errhandler) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_get_errhandler(file, errhandler);
}

#undef FCNAME
#define FCNAME MPIASP_File_set_errhandler
int MPIASP_File_set_errhandler(MPI_File file, MPI_Errhandler errhandler) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_set_errhandler(file, errhandler);
}

#undef FCNAME
#define FCNAME MPIASP_File_close
int MPIASP_File_close(MPI_File *mpi_fh) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_close(mpi_fh);
}

#undef FCNAME
#define FCNAME MPIASP_File_delete
int MPIASP_File_delete(char *filename, MPI_Info info) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_delete(filename, info);
}

#undef FCNAME
#define FCNAME MPIASP_File_c2f
MPI_Fint MPIASP_File_c2f(MPI_File mpi_fh) {
    MPIASP_DBG_PRINT_FCNAME_VOID(FCNAME);
    return PMPI_File_c2f(mpi_fh);
}

#undef FCNAME
#define FCNAME MPIASP_File_f2c
MPI_File MPI_File_f2c(MPI_Fint i) {
    MPIASP_DBG_PRINT_FCNAME_VOID(FCNAME);
    return PMPI_File_f2c(i);
}

#undef FCNAME
#define FCNAME MPIASP_File_sync
int MPIASP_File_sync(MPI_File mpi_fh) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_sync(mpi_fh);
}

#undef FCNAME
#define FCNAME MPIASP_File_get_amode
int MPIASP_File_get_amode(MPI_File mpi_fh, int *amode) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_get_amode(mpi_fh, amode);
}

#undef FCNAME
#define FCNAME MPIASP_File_get_atomicity
int MPIASP_File_get_atomicity(MPI_File mpi_fh, int *flag) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_get_atomicity(mpi_fh, flag);
}

#undef FCNAME
#define FCNAME MPIASP_File_get_byte_offset
int MPIASP_File_get_byte_offset(MPI_File mpi_fh, MPI_Offset offset,
        MPI_Offset *disp) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_get_byte_offset(mpi_fh, offset, disp);
}

#undef FCNAME
#define FCNAME MPIASP_File_get_type_extent
int MPIASP_File_get_type_extent(MPI_File mpi_fh, MPI_Datatype datatype,
        MPI_Aint *extent) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_get_type_extent(mpi_fh, datatype, extent);
}

#undef FCNAME
#define FCNAME MPIASP_File_get_group
int MPIASP_File_get_group(MPI_File mpi_fh, MPI_Group *group) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_get_group(mpi_fh, group);
}

#undef FCNAME
#define FCNAME MPIASP_File_get_info
int MPIASP_File_get_info(MPI_File mpi_fh, MPI_Info *info_used) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_get_info(mpi_fh, info_used);
}

#undef FCNAME
#define FCNAME MPIASP_File_get_position
int MPIASP_File_get_position(MPI_File mpi_fh, MPI_Offset *offset) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_get_position(mpi_fh, offset);
}

#undef FCNAME
#define FCNAME MPIASP_File_get_position_shared
int MPIASP_File_get_position_shared(MPI_File mpi_fh, MPI_Offset *offset) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_get_position_shared(mpi_fh, offset);
}

#undef FCNAME
#define FCNAME MPIASP_File_get_size
int MPIASP_File_get_size(MPI_File mpi_fh, MPI_Offset *size) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_get_size(mpi_fh, size);
}

#undef FCNAME
#define FCNAME MPIASP_File_get_view
int MPIASP_File_get_view(MPI_File mpi_fh, MPI_Offset *disp, MPI_Datatype *etype,
        MPI_Datatype *filetype, char *datarep) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_get_view(mpi_fh, disp, etype, filetype, datarep);
}

#undef FCNAME
#define FCNAME MPIASP_File_iread
int MPIASP_File_iread(MPI_File mpi_fh, void *buf, int count,
        MPI_Datatype datatype, MPIO_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_iread(mpi_fh, buf, count, datatype, request);
}

#undef FCNAME
#define FCNAME MPIASP_File_iread_at
int MPIASP_File_iread_at(MPI_File mpi_fh, MPI_Offset offset, void *buf,
        int count, MPI_Datatype datatype, MPIO_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_iread_at(mpi_fh, offset, buf, count, datatype, request);
}

#undef FCNAME
#define FCNAME MPIASP_File_iread_shared
int MPIASP_File_iread_shared(MPI_File mpi_fh, void *buf, int count,
        MPI_Datatype datatype, MPIO_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_iread_shared(mpi_fh, buf, count, datatype, request);
}

#undef FCNAME
#define FCNAME MPIASP_File_iwrite
int MPIASP_File_iwrite(MPI_File mpi_fh, void *buf, int count,
        MPI_Datatype datatype, MPIO_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_iwrite(mpi_fh, buf, count, datatype, request);
}

#undef FCNAME
#define FCNAME MPIASP_File_iwrite_at
int MPIASP_File_iwrite_at(MPI_File mpi_fh, MPI_Offset offset, void *buf,
        int count, MPI_Datatype datatype, MPIO_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_iwrite_at(mpi_fh, offset, buf, count, datatype, request);
}

#undef FCNAME
#define FCNAME MPIASP_File_iwrite_shared
int MPIASP_File_iwrite_shared(MPI_File mpi_fh, void *buf, int count,
        MPI_Datatype datatype, MPIO_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_iwrite_shared(mpi_fh, buf, count, datatype, request);
}

#undef FCNAME
#define FCNAME MPIASP_File_open
int MPIASP_File_open(MPI_Comm comm, char *filename, int amode, MPI_Info info,
        MPI_File *fh) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_open(comm, filename, amode, info, fh);
}

#undef FCNAME
#define FCNAME MPIASP_File_preallocate
int MPIASP_File_preallocate(MPI_File mpi_fh, MPI_Offset size) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_preallocate(mpi_fh, size);
}

#undef FCNAME
#define FCNAME MPIASP_File_read_at_all_begin
int MPIASP_File_read_at_all_begin(MPI_File mpi_fh, MPI_Offset offset, void *buf,
        int count, MPI_Datatype datatype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_read_at_all_begin(mpi_fh, offset, buf, count, datatype);
}

#undef FCNAME
#define FCNAME MPIASP_File_read_at_all_end
int MPIASP_File_read_at_all_end(MPI_File mpi_fh, void *buf, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_read_at_all_end(mpi_fh, buf, status);
}

#undef FCNAME
#define FCNAME MPIASP_File_read
int MPIASP_File_read(MPI_File mpi_fh, void *buf, int count,
        MPI_Datatype datatype, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_read(mpi_fh, buf, count, datatype, status);
}

#undef FCNAME
#define FCNAME MPIASP_File_read_all
int MPIASP_File_read_all(MPI_File mpi_fh, void *buf, int count,
        MPI_Datatype datatype, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_read_all(mpi_fh, buf, count, datatype, status);
}

#undef FCNAME
#define FCNAME MPIASP_File_read_all_begin
int MPIASP_File_read_all_begin(MPI_File mpi_fh, void *buf, int count,
        MPI_Datatype datatype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_read_all_begin(mpi_fh, buf, count, datatype);
}

#undef FCNAME
#define FCNAME MPIASP_File_read_all_end
int MPIASP_File_read_all_end(MPI_File mpi_fh, void *buf, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_read_all_end(mpi_fh, buf, status);
}

#undef FCNAME
#define FCNAME MPIASP_File_read_at
int MPIASP_File_read_at(MPI_File mpi_fh, MPI_Offset offset, void *buf,
        int count, MPI_Datatype datatype, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_read_at(mpi_fh, offset, buf, count, datatype, status);
}

#undef FCNAME
#define FCNAME MPIASP_File_read_at_all
int MPIASP_File_read_at_all(MPI_File mpi_fh, MPI_Offset offset, void *buf,
        int count, MPI_Datatype datatype, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_read_at_all(mpi_fh, offset, buf, count, datatype, status);
}

#undef FCNAME
#define FCNAME MPIASP_File_read_ordered
int MPIASP_File_read_ordered(MPI_File mpi_fh, void *buf, int count,
        MPI_Datatype datatype, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_read_ordered(mpi_fh, buf, count, datatype, status);
}

#undef FCNAME
#define FCNAME MPIASP_File_read_ordered_begin
int MPIASP_File_read_ordered_begin(MPI_File mpi_fh, void *buf, int count,
        MPI_Datatype datatype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_read_ordered_begin(mpi_fh, buf, count, datatype);
}

#undef FCNAME
#define FCNAME MPIASP_File_read_ordered_end
int MPIASP_File_read_ordered_end(MPI_File mpi_fh, void *buf, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_read_ordered_end(mpi_fh, buf, status);
}

#undef FCNAME
#define FCNAME MPIASP_File_read_shared
int MPIASP_File_read_shared(MPI_File mpi_fh, void *buf, int count,
        MPI_Datatype datatype, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_read_shared(mpi_fh, buf, count, datatype, status);
}

#undef FCNAME
#define FCNAME MPIASP_File_seek
int MPIASP_File_seek(MPI_File mpi_fh, MPI_Offset offset, int whence) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_seek(mpi_fh, offset, whence);
}

#undef FCNAME
#define FCNAME MPIASP_File_seek_shared
int MPIASP_File_seek_shared(MPI_File mpi_fh, MPI_Offset offset, int whence) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_seek_shared(mpi_fh, offset, whence);
}

#undef FCNAME
#define FCNAME MPIASP_File_set_atomicity
int MPIASP_File_set_atomicity(MPI_File mpi_fh, int flag) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_set_atomicity(mpi_fh, flag);
}

#undef FCNAME
#define FCNAME MPIASP_File_set_info
int MPIASP_File_set_info(MPI_File mpi_fh, MPI_Info info) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_set_info(mpi_fh, info);
}

#undef FCNAME
#define FCNAME MPIASP_File_set_size
int MPIASP_File_set_size(MPI_File mpi_fh, MPI_Offset size) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_set_size(mpi_fh, size);
}

#undef FCNAME
#define FCNAME MPIASP_File_set_view
int MPIASP_File_set_view(MPI_File mpi_fh, MPI_Offset disp, MPI_Datatype etype,
        MPI_Datatype filetype, char *datarep, MPI_Info info) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_set_view(mpi_fh, disp, etype, filetype, datarep, info);
}

#undef FCNAME
#define FCNAME MPIASP_File_write
int MPIASP_File_write(MPI_File mpi_fh, void *buf, int count,
        MPI_Datatype datatype, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_write(mpi_fh, buf, count, datatype, status);
}

#undef FCNAME
#define FCNAME MPIASP_File_write_all
int MPIASP_File_write_all(MPI_File mpi_fh, void *buf, int count,
        MPI_Datatype datatype, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_write_all(mpi_fh, buf, count, datatype, status);
}

#undef FCNAME
#define FCNAME MPIASP_File_write_all_begin
int MPIASP_File_write_all_begin(MPI_File mpi_fh, void *buf, int count,
        MPI_Datatype datatype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_write_all_begin(mpi_fh, buf, count, datatype);
}

#undef FCNAME
#define FCNAME MPIASP_File_write_all_end
int MPIASP_File_write_all_end(MPI_File mpi_fh, void *buf, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_write_all_end(mpi_fh, buf, status);
}

#undef FCNAME
#define FCNAME MPIASP_File_write_at
int MPIASP_File_write_at(MPI_File mpi_fh, MPI_Offset offset, void *buf,
        int count, MPI_Datatype datatype, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_write_at(mpi_fh, offset, buf, count, datatype, status);
}

#undef FCNAME
#define FCNAME MPIASP_File_write_at_all
int MPIASP_File_write_at_all(MPI_File mpi_fh, MPI_Offset offset, void *buf,
        int count, MPI_Datatype datatype, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_write_at_all(mpi_fh, offset, buf, count, datatype, status);
}

#undef FCNAME
#define FCNAME MPIASP_File_write_ordered
int MPIASP_File_write_ordered(MPI_File mpi_fh, void *buf, int count,
        MPI_Datatype datatype, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_write_ordered(mpi_fh, buf, count, datatype, status);
}

#undef FCNAME
#define FCNAME MPIASP_File_write_ordered_begin
int MPIASP_File_write_ordered_begin(MPI_File mpi_fh, void *buf, int count,
        MPI_Datatype datatype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_write_ordered_begin(mpi_fh, buf, count, datatype);
}

#undef FCNAME
#define FCNAME MPIASP_File_write_ordered_end
int MPIASP_File_write_ordered_end(MPI_File mpi_fh, void *buf,
        MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_write_ordered_end(mpi_fh, buf, status);
}

#undef FCNAME
#define FCNAME MPIASP_File_write_shared
int MPIASP_File_write_shared(MPI_File mpi_fh, void *buf, int count,
        MPI_Datatype datatype, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_write_shared(mpi_fh, buf, count, datatype, status);
}

#undef FCNAME
#define FCNAME MPIASP_File_write_at_all_begin
int MPIASP_File_write_at_all_begin(MPI_File mpi_fh, MPI_Offset offset,
        void *buf, int count, MPI_Datatype datatype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_write_at_all_begin(mpi_fh, offset, buf, count, datatype);
}

#undef FCNAME
#define FCNAME MPIASP_File_write_at_all_end
int MPIASP_File_write_at_all_end(MPI_File mpi_fh, void *buf, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_File_write_at_all_end(mpi_fh, buf, status);
}
#endif /* endof HAVE_ROMIO */

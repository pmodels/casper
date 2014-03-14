#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "asp.h"


/* -- Begin Profiling Symbol Block for routine MPI_Init */
#if defined(HAVE_PRAGMA_WEAK) 
#pragma weak MPI_Init_thread = MPIASP_Init_thread
#pragma weak MPI_Attr_delete = MPIASP_Attr_delete
#pragma weak MPI_Attr_get = MPIASP_Attr_get
#pragma weak MPI_Attr_put = MPIASP_Attr_put
#pragma weak MPI_Comm_create_keyval = MPIASP_Comm_create_keyval
#pragma weak MPI_Comm_delete_attr = MPIASP_Comm_delete_attr
#pragma weak MPI_Comm_free_keyval = MPIASP_Comm_free_keyval
#pragma weak MPI_Comm_get_attr = MPIASP_Comm_get_attr
#pragma weak MPI_Comm_set_attr = MPIASP_Comm_set_attr
#pragma weak MPI_Keyval_create = MPIASP_Keyval_create
#pragma weak MPI_Keyval_free = MPIASP_Keyval_free
#pragma weak MPI_Type_create_keyval = MPIASP_Type_create_keyval
#pragma weak MPI_Type_delete_attr = MPIASP_Type_delete_attr
#pragma weak MPI_Type_free_keyval = MPIASP_Type_free_keyval
#pragma weak MPI_Type_get_attr = MPIASP_Type_get_attr
#pragma weak MPI_Type_set_attr = MPIASP_Type_set_attr
#pragma weak MPI_Win_create_keyval = MPIASP_Win_create_keyval
#pragma weak MPI_Win_delete_attr = MPIASP_Win_delete_attr
#pragma weak MPI_Win_free_keyval = MPIASP_Win_free_keyval
#pragma weak MPI_Win_get_attr = MPIASP_Win_get_attr
#pragma weak MPI_Win_set_attr = MPIASP_Win_set_attr
#pragma weak MPI_Allgather = MPIASP_Allgather
#pragma weak MPI_Allgatherv = MPIASP_Allgatherv
#pragma weak MPI_Allreduce = MPIASP_Allreduce
#pragma weak MPI_Alltoall = MPIASP_Alltoall
#pragma weak MPI_Alltoallv = MPIASP_Alltoallv
#pragma weak MPI_Alltoallw = MPIASP_Alltoallw
#pragma weak MPI_Barrier = MPIASP_Barrier
#pragma weak MPI_Bcast = MPIASP_Bcast
#pragma weak MPI_Exscan = MPIASP_Exscan
#pragma weak MPI_Gather = MPIASP_Gather
#pragma weak MPI_Gatherv = MPIASP_Gatherv
#pragma weak MPI_Op_create = MPIASP_Op_create
#pragma weak MPI_Op_free = MPIASP_Op_free
#pragma weak MPI_Op_commutative = MPIASP_Op_commutative
#pragma weak MPI_Reduce = MPIASP_Reduce
#pragma weak MPI_Reduce_local = MPIASP_Reduce_local
#pragma weak MPI_Reduce_scatter = MPIASP_Reduce_scatter
#pragma weak MPI_Reduce_scatter_block = MPIASP_Reduce_scatter_block
#pragma weak MPI_Scan = MPIASP_Scan
#pragma weak MPI_Scatter = MPIASP_Scatter
#pragma weak MPI_Scatterv = MPIASP_Scatterv
#pragma weak MPI_Comm_compare = MPIASP_Comm_compare
#pragma weak MPI_Comm_create = MPIASP_Comm_create
#pragma weak MPI_Comm_free = MPIASP_Comm_free
#pragma weak MPI_Comm_get_name = MPIASP_Comm_get_name
#pragma weak MPI_Comm_group = MPIASP_Comm_group
#pragma weak MPI_Comm_rank = MPIASP_Comm_rank
#pragma weak MPI_Comm_remote_group = MPIASP_Comm_remote_group
#pragma weak MPI_Comm_remote_size = MPIASP_Comm_remote_size
#pragma weak MPI_Comm_set_name = MPIASP_Comm_set_name
#pragma weak MPI_Comm_size = MPIASP_Comm_size
#pragma weak MPI_Comm_split = MPIASP_Comm_split
#pragma weak MPI_Comm_test_inter = MPIASP_Comm_test_inter
#pragma weak MPI_Intercomm_create = MPIASP_Intercomm_create
#pragma weak MPI_Intercomm_merge = MPIASP_Intercomm_merge
#pragma weak MPI_Address = MPIASP_Address
#pragma weak MPI_Get_address = MPIASP_Get_address
#pragma weak MPI_Get_count = MPIASP_Get_count
#pragma weak MPI_Get_elements = MPIASP_Get_elements
#pragma weak MPI_Pack = MPIASP_Pack
#pragma weak MPI_Pack_external = MPIASP_Pack_external
#pragma weak MPI_Pack_external_size = MPIASP_Pack_external_size
#pragma weak MPI_Pack_size = MPIASP_Pack_size
#pragma weak MPI_Status_set_elements = MPIASP_Status_set_elements
#pragma weak MPI_Type_commit = MPIASP_Type_commit
#pragma weak MPI_Type_contiguous = MPIASP_Type_contiguous
#pragma weak MPI_Type_create_darray = MPIASP_Type_create_darray
#pragma weak MPI_Type_create_hindexed = MPIASP_Type_create_hindexed
#pragma weak MPI_Type_create_hvector = MPIASP_Type_create_hvector
#pragma weak MPI_Type_create_indexed_block = MPIASP_Type_create_indexed_block
#pragma weak MPI_Type_create_hindexed_block = MPIASP_Type_create_hindexed_block
#pragma weak MPI_Type_create_resized = MPIASP_Type_create_resized
#pragma weak MPI_Type_create_struct = MPIASP_Type_create_struct
#pragma weak MPI_Type_create_subarray = MPIASP_Type_create_subarray
#pragma weak MPI_Type_dup = MPIASP_Type_dup
#pragma weak MPI_Type_extent = MPIASP_Type_extent
#pragma weak MPI_Type_free = MPIASP_Type_free
#pragma weak MPI_Type_get_contents = MPIASP_Type_get_contents
#pragma weak MPI_Type_get_envelope = MPIASP_Type_get_envelope
#pragma weak MPI_Type_get_extent = MPIASP_Type_get_extent
#pragma weak MPI_Type_get_name = MPIASP_Type_get_name
#pragma weak MPI_Type_get_true_extent = MPIASP_Type_get_true_extent
#pragma weak MPI_Type_hindexed = MPIASP_Type_hindexed
#pragma weak MPI_Type_hvector = MPIASP_Type_hvector
#pragma weak MPI_Type_indexed = MPIASP_Type_indexed
#pragma weak MPI_Type_lb = MPIASP_Type_lb
#pragma weak MPI_Type_match_size = MPIASP_Type_match_size
#pragma weak MPI_Type_set_name = MPIASP_Type_set_name
#pragma weak MPI_Type_size = MPIASP_Type_size
#pragma weak MPI_Type_struct = MPIASP_Type_struct
#pragma weak MPI_Type_ub = MPIASP_Type_ub
#pragma weak MPI_Type_vector = MPIASP_Type_vector
#pragma weak MPI_Unpack = MPIASP_Unpack
#pragma weak MPI_Unpack_external = MPIASP_Unpack_external
#pragma weak MPI_Add_error_class = MPIASP_Add_error_class
#pragma weak MPI_Add_error_code = MPIASP_Add_error_code
#pragma weak MPI_Add_error_string = MPIASP_Add_error_string
#pragma weak MPI_Comm_call_errhandler = MPIASP_Comm_call_errhandler
#pragma weak MPI_Comm_create_errhandler = MPIASP_Comm_create_errhandler
#pragma weak MPI_Comm_get_errhandler = MPIASP_Comm_get_errhandler
#pragma weak MPI_Comm_set_errhandler = MPIASP_Comm_set_errhandler
#pragma weak MPI_Errhandler_create = MPIASP_Errhandler_create
#pragma weak MPI_Errhandler_free = MPIASP_Errhandler_free
#pragma weak MPI_Errhandler_get = MPIASP_Errhandler_get
#pragma weak MPI_Errhandler_set = MPIASP_Errhandler_set
#pragma weak MPI_Error_class = MPIASP_Error_class
#pragma weak MPI_Error_string = MPIASP_Error_string
#pragma weak MPI_Win_call_errhandler = MPIASP_Win_call_errhandler
#pragma weak MPI_Win_create_errhandler = MPIASP_Win_create_errhandler
#pragma weak MPI_Win_get_errhandler = MPIASP_Win_get_errhandler
#pragma weak MPI_Win_set_errhandler = MPIASP_Win_set_errhandler
#pragma weak MPI_Group_compare = MPIASP_Group_compare
#pragma weak MPI_Group_difference = MPIASP_Group_difference
#pragma weak MPI_Group_excl = MPIASP_Group_excl
#pragma weak MPI_Group_free = MPIASP_Group_free
#pragma weak MPI_Group_incl = MPIASP_Group_incl
#pragma weak MPI_Group_intersection = MPIASP_Group_intersection
#pragma weak MPI_Group_range_excl = MPIASP_Group_range_excl
#pragma weak MPI_Group_range_incl = MPIASP_Group_range_incl
#pragma weak MPI_Group_rank = MPIASP_Group_rank
#pragma weak MPI_Group_size = MPIASP_Group_size
#pragma weak MPI_Group_translate_ranks = MPIASP_Group_translate_ranks
#pragma weak MPI_Group_union = MPIASP_Group_union
#pragma weak MPI_Finalized = MPIASP_Finalized
#pragma weak MPI_Initialized = MPIASP_Initialized
#pragma weak MPI_Is_thread_main = MPIASP_Is_thread_main
#pragma weak MPI_Query_thread = MPIASP_Query_thread
#pragma weak MPI_Get_processor_name = MPIASP_Get_processor_name
#pragma weak MPI_Pcontrol = MPIASP_Pcontrol
#pragma weak MPI_Get_version = MPIASP_Get_version
#pragma weak MPI_Bsend = MPIASP_Bsend
#pragma weak MPI_Bsend_init = MPIASP_Bsend_init
#pragma weak MPI_Buffer_attach = MPIASP_Buffer_attach
#pragma weak MPI_Buffer_detach = MPIASP_Buffer_detach
#pragma weak MPI_Cancel = MPIASP_Cancel
#pragma weak MPI_Grequest_complete = MPIASP_Grequest_complete
#pragma weak MPI_Grequest_start = MPIASP_Grequest_start
#pragma weak MPI_Ibsend = MPIASP_Ibsend
#pragma weak MPI_Iprobe = MPIASP_Iprobe
#pragma weak MPI_Irecv = MPIASP_Irecv
#pragma weak MPI_Irsend = MPIASP_Irsend
#pragma weak MPI_Isend = MPIASP_Isend
#pragma weak MPI_Issend = MPIASP_Issend
#pragma weak MPI_Probe = MPIASP_Probe
#pragma weak MPI_Recv = MPIASP_Recv
#pragma weak MPI_Recv_init = MPIASP_Recv_init
#pragma weak MPI_Request_free = MPIASP_Request_free
#pragma weak MPI_Request_get_status = MPIASP_Request_get_status
#pragma weak MPI_Rsend = MPIASP_Rsend
#pragma weak MPI_Rsend_init = MPIASP_Rsend_init
#pragma weak MPI_Send = MPIASP_Send
#pragma weak MPI_Sendrecv = MPIASP_Sendrecv
#pragma weak MPI_Sendrecv_replace = MPIASP_Sendrecv_replace
#pragma weak MPI_Send_init = MPIASP_Send_init
#pragma weak MPI_Ssend = MPIASP_Ssend
#pragma weak MPI_Ssend_init = MPIASP_Ssend_init
#pragma weak MPI_Start = MPIASP_Start
#pragma weak MPI_Startall = MPIASP_Startall
#pragma weak MPI_Status_set_cancelled = MPIASP_Status_set_cancelled
#pragma weak MPI_Test = MPIASP_Test
#pragma weak MPI_Testall = MPIASP_Testall
#pragma weak MPI_Testany = MPIASP_Testany
#pragma weak MPI_Testsome = MPIASP_Testsome
#pragma weak MPI_Test_cancelled = MPIASP_Test_cancelled
#pragma weak MPI_Wait = MPIASP_Wait
#pragma weak MPI_Waitall = MPIASP_Waitall
#pragma weak MPI_Waitany = MPIASP_Waitany
#pragma weak MPI_Waitsome = MPIASP_Waitsome
#pragma weak MPI_Accumulate = MPIASP_Accumulate
#pragma weak MPI_Alloc_mem = MPIASP_Alloc_mem
#pragma weak MPI_Free_mem = MPIASP_Free_mem
#pragma weak MPI_Get = MPIASP_Get
#pragma weak MPI_Put = MPIASP_Put
#pragma weak MPI_Win_complete = MPIASP_Win_complete
#pragma weak MPI_Win_fence = MPIASP_Win_fence
#pragma weak MPI_Win_free = MPIASP_Win_free
#pragma weak MPI_Win_get_group = MPIASP_Win_get_group
#pragma weak MPI_Win_get_name = MPIASP_Win_get_name
#pragma weak MPI_Win_lock = MPIASP_Win_lock
#pragma weak MPI_Win_post = MPIASP_Win_post
#pragma weak MPI_Win_set_name = MPIASP_Win_set_name
#pragma weak MPI_Win_start = MPIASP_Win_start
#pragma weak MPI_Win_test = MPIASP_Win_test
#pragma weak MPI_Win_unlock = MPIASP_Win_unlock
#pragma weak MPI_Win_wait = MPIASP_Win_wait
#pragma weak MPI_Info_create = MPIASP_Info_create
#pragma weak MPI_Info_delete = MPIASP_Info_delete
#pragma weak MPI_Info_dup = MPIASP_Info_dup
#pragma weak MPI_Info_free = MPIASP_Info_free
#pragma weak MPI_Info_get = MPIASP_Info_get
#pragma weak MPI_Info_get_nkeys = MPIASP_Info_get_nkeys
#pragma weak MPI_Info_get_nthkey = MPIASP_Info_get_nthkey
#pragma weak MPI_Info_get_valuelen = MPIASP_Info_get_valuelen
#pragma weak MPI_Info_set = MPIASP_Info_set
#pragma weak MPI_Close_port = MPIASP_Close_port
#pragma weak MPI_Comm_accept = MPIASP_Comm_accept
#pragma weak MPI_Comm_connect = MPIASP_Comm_connect
#pragma weak MPI_Comm_disconnect = MPIASP_Comm_disconnect
#pragma weak MPI_Comm_get_parent = MPIASP_Comm_get_parent
#pragma weak MPI_Comm_join = MPIASP_Comm_join
#pragma weak MPI_Comm_spawn = MPIASP_Comm_spawn
#pragma weak MPI_Comm_spawn_multiple = MPIASP_Comm_spawn_multiple
#pragma weak MPI_Lookup_name = MPIASP_Lookup_name
#pragma weak MPI_Open_port = MPIASP_Open_port
#pragma weak MPI_Publish_name = MPIASP_Publish_name
#pragma weak MPI_Unpublish_name = MPIASP_Unpublish_name
#pragma weak MPI_Cartdim_get = MPIASP_Cartdim_get
#pragma weak MPI_Cart_coords = MPIASP_Cart_coords
#pragma weak MPI_Cart_create = MPIASP_Cart_create
#pragma weak MPI_Cart_get = MPIASP_Cart_get
#pragma weak MPI_Cart_map = MPIASP_Cart_map
#pragma weak MPI_Cart_rank = MPIASP_Cart_rank
#pragma weak MPI_Cart_shift = MPIASP_Cart_shift
#pragma weak MPI_Cart_sub = MPIASP_Cart_sub
#pragma weak MPI_Dims_create = MPIASP_Dims_create
#pragma weak MPI_Graph_create = MPIASP_Graph_create
#pragma weak MPI_Dist_graph_create = MPIASP_Dist_graph_create
#pragma weak MPI_Dist_graph_create_adjacent = MPIASP_Dist_graph_create_adjacent
#pragma weak MPI_Graphdims_get = MPIASP_Graphdims_get
#pragma weak MPI_Graph_neighbors_count = MPIASP_Graph_neighbors_count
#pragma weak MPI_Graph_get = MPIASP_Graph_get
#pragma weak MPI_Graph_map = MPIASP_Graph_map
#pragma weak MPI_Graph_neighbors = MPIASP_Graph_neighbors
#pragma weak MPI_Dist_graph_neighbors = MPIASP_Dist_graph_neighbors
#pragma weak MPI_Dist_graph_neighbors_count = MPIASP_Dist_graph_neighbors_count
#pragma weak MPI_Topo_test = MPIASP_Topo_test
#pragma weak MPI_Wtime = MPIASP_Wtime
#pragma weak MPI_Wtick = MPIASP_Wtick

#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF MPIASP_Init_thread MPI_Init_thread
#pragma _HP_SECONDARY_DEF MPIASP_Attr_delete MPI_Attr_delete
#pragma _HP_SECONDARY_DEF MPIASP_Attr_get MPI_Attr_get
#pragma _HP_SECONDARY_DEF MPIASP_Attr_put MPI_Attr_put
#pragma _HP_SECONDARY_DEF MPIASP_Comm_create_keyval MPI_Comm_create_keyval
#pragma _HP_SECONDARY_DEF MPIASP_Comm_delete_attr MPI_Comm_delete_attr
#pragma _HP_SECONDARY_DEF MPIASP_Comm_free_keyval MPI_Comm_free_keyval
#pragma _HP_SECONDARY_DEF MPIASP_Comm_get_attr MPI_Comm_get_attr
#pragma _HP_SECONDARY_DEF MPIASP_Comm_set_attr MPI_Comm_set_attr
#pragma _HP_SECONDARY_DEF MPIASP_Keyval_create MPI_Keyval_create
#pragma _HP_SECONDARY_DEF MPIASP_Keyval_free MPI_Keyval_free
#pragma _HP_SECONDARY_DEF MPIASP_Type_create_keyval MPI_Type_create_keyval
#pragma _HP_SECONDARY_DEF MPIASP_Type_delete_attr MPI_Type_delete_attr
#pragma _HP_SECONDARY_DEF MPIASP_Type_free_keyval MPI_Type_free_keyval
#pragma _HP_SECONDARY_DEF MPIASP_Type_get_attr MPI_Type_get_attr
#pragma _HP_SECONDARY_DEF MPIASP_Type_set_attr MPI_Type_set_attr
#pragma _HP_SECONDARY_DEF MPIASP_Win_create_keyval MPI_Win_create_keyval
#pragma _HP_SECONDARY_DEF MPIASP_Win_delete_attr MPI_Win_delete_attr
#pragma _HP_SECONDARY_DEF MPIASP_Win_free_keyval MPI_Win_free_keyval
#pragma _HP_SECONDARY_DEF MPIASP_Win_get_attr MPI_Win_get_attr
#pragma _HP_SECONDARY_DEF MPIASP_Win_set_attr MPI_Win_set_attr
#pragma _HP_SECONDARY_DEF MPIASP_Allgather MPI_Allgather
#pragma _HP_SECONDARY_DEF MPIASP_Allgatherv MPI_Allgatherv
#pragma _HP_SECONDARY_DEF MPIASP_Allreduce MPI_Allreduce
#pragma _HP_SECONDARY_DEF MPIASP_Alltoall MPI_Alltoall
#pragma _HP_SECONDARY_DEF MPIASP_Alltoallv MPI_Alltoallv
#pragma _HP_SECONDARY_DEF MPIASP_Alltoallw MPI_Alltoallw
#pragma _HP_SECONDARY_DEF MPIASP_Barrier MPI_Barrier
#pragma _HP_SECONDARY_DEF MPIASP_Bcast MPI_Bcast
#pragma _HP_SECONDARY_DEF MPIASP_Exscan MPI_Exscan
#pragma _HP_SECONDARY_DEF MPIASP_Gather MPI_Gather
#pragma _HP_SECONDARY_DEF MPIASP_Gatherv MPI_Gatherv
#pragma _HP_SECONDARY_DEF MPIASP_Op_create MPI_Op_create
#pragma _HP_SECONDARY_DEF MPIASP_Op_free MPI_Op_free
#pragma _HP_SECONDARY_DEF MPIASP_Op_commutative MPI_Op_commutative
#pragma _HP_SECONDARY_DEF MPIASP_Reduce MPI_Reduce
#pragma _HP_SECONDARY_DEF MPIASP_Reduce_local MPI_Reduce_local
#pragma _HP_SECONDARY_DEF MPIASP_Reduce_scatter MPI_Reduce_scatter
#pragma _HP_SECONDARY_DEF MPIASP_Reduce_scatter_block MPI_Reduce_scatter_block
#pragma _HP_SECONDARY_DEF MPIASP_Scan MPI_Scan
#pragma _HP_SECONDARY_DEF MPIASP_Scatter MPI_Scatter
#pragma _HP_SECONDARY_DEF MPIASP_Scatterv MPI_Scatterv
#pragma _HP_SECONDARY_DEF MPIASP_Comm_compare MPI_Comm_compare
#pragma _HP_SECONDARY_DEF MPIASP_Comm_create MPI_Comm_create
#pragma _HP_SECONDARY_DEF MPIASP_Comm_free MPI_Comm_free
#pragma _HP_SECONDARY_DEF MPIASP_Comm_get_name MPI_Comm_get_name
#pragma _HP_SECONDARY_DEF MPIASP_Comm_group MPI_Comm_group
#pragma _HP_SECONDARY_DEF MPIASP_Comm_rank MPI_Comm_rank
#pragma _HP_SECONDARY_DEF MPIASP_Comm_remote_group MPI_Comm_remote_group
#pragma _HP_SECONDARY_DEF MPIASP_Comm_remote_size MPI_Comm_remote_size
#pragma _HP_SECONDARY_DEF MPIASP_Comm_set_name MPI_Comm_set_name
#pragma _HP_SECONDARY_DEF MPIASP_Comm_size MPI_Comm_size
#pragma _HP_SECONDARY_DEF MPIASP_Comm_split MPI_Comm_split
#pragma _HP_SECONDARY_DEF MPIASP_Comm_test_inter MPI_Comm_test_inter
#pragma _HP_SECONDARY_DEF MPIASP_Intercomm_create MPI_Intercomm_create
#pragma _HP_SECONDARY_DEF MPIASP_Intercomm_merge MPI_Intercomm_merge
#pragma _HP_SECONDARY_DEF MPIASP_Address MPI_Address
#pragma _HP_SECONDARY_DEF MPIASP_Get_address MPI_Get_address
#pragma _HP_SECONDARY_DEF MPIASP_Get_count MPI_Get_count
#pragma _HP_SECONDARY_DEF MPIASP_Get_elements MPI_Get_elements
#pragma _HP_SECONDARY_DEF MPIASP_Pack MPI_Pack
#pragma _HP_SECONDARY_DEF MPIASP_Pack_external MPI_Pack_external
#pragma _HP_SECONDARY_DEF MPIASP_Pack_external_size MPI_Pack_external_size
#pragma _HP_SECONDARY_DEF MPIASP_Pack_size MPI_Pack_size
#pragma _HP_SECONDARY_DEF MPIASP_Status_set_elements MPI_Status_set_elements
#pragma _HP_SECONDARY_DEF MPIASP_Type_commit MPI_Type_commit
#pragma _HP_SECONDARY_DEF MPIASP_Type_contiguous MPI_Type_contiguous
#pragma _HP_SECONDARY_DEF MPIASP_Type_create_darray MPI_Type_create_darray
#pragma _HP_SECONDARY_DEF MPIASP_Type_create_hindexed MPI_Type_create_hindexed
#pragma _HP_SECONDARY_DEF MPIASP_Type_create_hvector MPI_Type_create_hvector
#pragma _HP_SECONDARY_DEF MPIASP_Type_create_indexed_block MPI_Type_create_indexed_block
#pragma _HP_SECONDARY_DEF MPIASP_Type_create_hindexed_block MPI_Type_create_hindexed_block
#pragma _HP_SECONDARY_DEF MPIASP_Type_create_resized MPI_Type_create_resized
#pragma _HP_SECONDARY_DEF MPIASP_Type_create_struct MPI_Type_create_struct
#pragma _HP_SECONDARY_DEF MPIASP_Type_create_subarray MPI_Type_create_subarray
#pragma _HP_SECONDARY_DEF MPIASP_Type_dup MPI_Type_dup
#pragma _HP_SECONDARY_DEF MPIASP_Type_extent MPI_Type_extent
#pragma _HP_SECONDARY_DEF MPIASP_Type_free MPI_Type_free
#pragma _HP_SECONDARY_DEF MPIASP_Type_get_contents MPI_Type_get_contents
#pragma _HP_SECONDARY_DEF MPIASP_Type_get_envelope MPI_Type_get_envelope
#pragma _HP_SECONDARY_DEF MPIASP_Type_get_extent MPI_Type_get_extent
#pragma _HP_SECONDARY_DEF MPIASP_Type_get_name MPI_Type_get_name
#pragma _HP_SECONDARY_DEF MPIASP_Type_get_true_extent MPI_Type_get_true_extent
#pragma _HP_SECONDARY_DEF MPIASP_Type_hindexed MPI_Type_hindexed
#pragma _HP_SECONDARY_DEF MPIASP_Type_hvector MPI_Type_hvector
#pragma _HP_SECONDARY_DEF MPIASP_Type_indexed MPI_Type_indexed
#pragma _HP_SECONDARY_DEF MPIASP_Type_lb MPI_Type_lb
#pragma _HP_SECONDARY_DEF MPIASP_Type_match_size MPI_Type_match_size
#pragma _HP_SECONDARY_DEF MPIASP_Type_set_name MPI_Type_set_name
#pragma _HP_SECONDARY_DEF MPIASP_Type_size MPI_Type_size
#pragma _HP_SECONDARY_DEF MPIASP_Type_struct MPI_Type_struct
#pragma _HP_SECONDARY_DEF MPIASP_Type_ub MPI_Type_ub
#pragma _HP_SECONDARY_DEF MPIASP_Type_vector MPI_Type_vector
#pragma _HP_SECONDARY_DEF MPIASP_Unpack MPI_Unpack
#pragma _HP_SECONDARY_DEF MPIASP_Unpack_external MPI_Unpack_external
#pragma _HP_SECONDARY_DEF MPIASP_Add_error_class MPI_Add_error_class
#pragma _HP_SECONDARY_DEF MPIASP_Add_error_code MPI_Add_error_code
#pragma _HP_SECONDARY_DEF MPIASP_Add_error_string MPI_Add_error_string
#pragma _HP_SECONDARY_DEF MPIASP_Comm_call_errhandler MPI_Comm_call_errhandler
#pragma _HP_SECONDARY_DEF MPIASP_Comm_create_errhandler MPI_Comm_create_errhandler
#pragma _HP_SECONDARY_DEF MPIASP_Comm_get_errhandler MPI_Comm_get_errhandler
#pragma _HP_SECONDARY_DEF MPIASP_Comm_set_errhandler MPI_Comm_set_errhandler
#pragma _HP_SECONDARY_DEF MPIASP_Errhandler_create MPI_Errhandler_create
#pragma _HP_SECONDARY_DEF MPIASP_Errhandler_free MPI_Errhandler_free
#pragma _HP_SECONDARY_DEF MPIASP_Errhandler_get MPI_Errhandler_get
#pragma _HP_SECONDARY_DEF MPIASP_Errhandler_set MPI_Errhandler_set
#pragma _HP_SECONDARY_DEF MPIASP_Error_class MPI_Error_class
#pragma _HP_SECONDARY_DEF MPIASP_Error_string MPI_Error_string
#pragma _HP_SECONDARY_DEF MPIASP_Win_call_errhandler MPI_Win_call_errhandler
#pragma _HP_SECONDARY_DEF MPIASP_Win_create_errhandler MPI_Win_create_errhandler
#pragma _HP_SECONDARY_DEF MPIASP_Win_get_errhandler MPI_Win_get_errhandler
#pragma _HP_SECONDARY_DEF MPIASP_Win_set_errhandler MPI_Win_set_errhandler
#pragma _HP_SECONDARY_DEF MPIASP_Group_compare MPI_Group_compare
#pragma _HP_SECONDARY_DEF MPIASP_Group_difference MPI_Group_difference
#pragma _HP_SECONDARY_DEF MPIASP_Group_excl MPI_Group_excl
#pragma _HP_SECONDARY_DEF MPIASP_Group_free MPI_Group_free
#pragma _HP_SECONDARY_DEF MPIASP_Group_incl MPI_Group_incl
#pragma _HP_SECONDARY_DEF MPIASP_Group_intersection MPI_Group_intersection
#pragma _HP_SECONDARY_DEF MPIASP_Group_range_excl MPI_Group_range_excl
#pragma _HP_SECONDARY_DEF MPIASP_Group_range_incl MPI_Group_range_incl
#pragma _HP_SECONDARY_DEF MPIASP_Group_rank MPI_Group_rank
#pragma _HP_SECONDARY_DEF MPIASP_Group_size MPI_Group_size
#pragma _HP_SECONDARY_DEF MPIASP_Group_translate_ranks MPI_Group_translate_ranks
#pragma _HP_SECONDARY_DEF MPIASP_Group_union MPI_Group_union
#pragma _HP_SECONDARY_DEF MPIASP_Finalized MPI_Finalized
#pragma _HP_SECONDARY_DEF MPIASP_Initialized MPI_Initialized
#pragma _HP_SECONDARY_DEF MPIASP_Is_thread_main MPI_Is_thread_main
#pragma _HP_SECONDARY_DEF MPIASP_Query_thread MPI_Query_thread
#pragma _HP_SECONDARY_DEF MPIASP_Get_processor_name MPI_Get_processor_name
#pragma _HP_SECONDARY_DEF MPIASP_Pcontrol MPI_Pcontrol
#pragma _HP_SECONDARY_DEF MPIASP_Get_version MPI_Get_version
#pragma _HP_SECONDARY_DEF MPIASP_Bsend MPI_Bsend
#pragma _HP_SECONDARY_DEF MPIASP_Bsend_init MPI_Bsend_init
#pragma _HP_SECONDARY_DEF MPIASP_Buffer_attach MPI_Buffer_attach
#pragma _HP_SECONDARY_DEF MPIASP_Buffer_detach MPI_Buffer_detach
#pragma _HP_SECONDARY_DEF MPIASP_Cancel MPI_Cancel
#pragma _HP_SECONDARY_DEF MPIASP_Grequest_complete MPI_Grequest_complete
#pragma _HP_SECONDARY_DEF MPIASP_Grequest_start MPI_Grequest_start
#pragma _HP_SECONDARY_DEF MPIASP_Ibsend MPI_Ibsend
#pragma _HP_SECONDARY_DEF MPIASP_Iprobe MPI_Iprobe
#pragma _HP_SECONDARY_DEF MPIASP_Irecv MPI_Irecv
#pragma _HP_SECONDARY_DEF MPIASP_Irsend MPI_Irsend
#pragma _HP_SECONDARY_DEF MPIASP_Isend MPI_Isend
#pragma _HP_SECONDARY_DEF MPIASP_Issend MPI_Issend
#pragma _HP_SECONDARY_DEF MPIASP_Probe MPI_Probe
#pragma _HP_SECONDARY_DEF MPIASP_Recv MPI_Recv
#pragma _HP_SECONDARY_DEF MPIASP_Recv_init MPI_Recv_init
#pragma _HP_SECONDARY_DEF MPIASP_Request_free MPI_Request_free
#pragma _HP_SECONDARY_DEF MPIASP_Request_get_status MPI_Request_get_status
#pragma _HP_SECONDARY_DEF MPIASP_Rsend MPI_Rsend
#pragma _HP_SECONDARY_DEF MPIASP_Rsend_init MPI_Rsend_init
#pragma _HP_SECONDARY_DEF MPIASP_Send MPI_Send
#pragma _HP_SECONDARY_DEF MPIASP_Sendrecv MPI_Sendrecv
#pragma _HP_SECONDARY_DEF MPIASP_Sendrecv_replace MPI_Sendrecv_replace
#pragma _HP_SECONDARY_DEF MPIASP_Send_init MPI_Send_init
#pragma _HP_SECONDARY_DEF MPIASP_Ssend MPI_Ssend
#pragma _HP_SECONDARY_DEF MPIASP_Ssend_init MPI_Ssend_init
#pragma _HP_SECONDARY_DEF MPIASP_Start MPI_Start
#pragma _HP_SECONDARY_DEF MPIASP_Startall MPI_Startall
#pragma _HP_SECONDARY_DEF MPIASP_Status_set_cancelled MPI_Status_set_cancelled
#pragma _HP_SECONDARY_DEF MPIASP_Test MPI_Test
#pragma _HP_SECONDARY_DEF MPIASP_Testall MPI_Testall
#pragma _HP_SECONDARY_DEF MPIASP_Testany MPI_Testany
#pragma _HP_SECONDARY_DEF MPIASP_Testsome MPI_Testsome
#pragma _HP_SECONDARY_DEF MPIASP_Test_cancelled MPI_Test_cancelled
#pragma _HP_SECONDARY_DEF MPIASP_Wait MPI_Wait
#pragma _HP_SECONDARY_DEF MPIASP_Waitall MPI_Waitall
#pragma _HP_SECONDARY_DEF MPIASP_Waitany MPI_Waitany
#pragma _HP_SECONDARY_DEF MPIASP_Waitsome MPI_Waitsome
#pragma _HP_SECONDARY_DEF MPIASP_Accumulate MPI_Accumulate
#pragma _HP_SECONDARY_DEF MPIASP_Alloc_mem MPI_Alloc_mem
#pragma _HP_SECONDARY_DEF MPIASP_Free_mem MPI_Free_mem
#pragma _HP_SECONDARY_DEF MPIASP_Get MPI_Get
#pragma _HP_SECONDARY_DEF MPIASP_Put MPI_Put
#pragma _HP_SECONDARY_DEF MPIASP_Win_complete MPI_Win_complete
#pragma _HP_SECONDARY_DEF MPIASP_Win_fence MPI_Win_fence
#pragma _HP_SECONDARY_DEF MPIASP_Win_free MPI_Win_free
#pragma _HP_SECONDARY_DEF MPIASP_Win_get_group MPI_Win_get_group
#pragma _HP_SECONDARY_DEF MPIASP_Win_get_name MPI_Win_get_name
#pragma _HP_SECONDARY_DEF MPIASP_Win_lock MPI_Win_lock
#pragma _HP_SECONDARY_DEF MPIASP_Win_post MPI_Win_post
#pragma _HP_SECONDARY_DEF MPIASP_Win_set_name MPI_Win_set_name
#pragma _HP_SECONDARY_DEF MPIASP_Win_start MPI_Win_start
#pragma _HP_SECONDARY_DEF MPIASP_Win_test MPI_Win_test
#pragma _HP_SECONDARY_DEF MPIASP_Win_unlock MPI_Win_unlock
#pragma _HP_SECONDARY_DEF MPIASP_Win_wait MPI_Win_wait
#pragma _HP_SECONDARY_DEF MPIASP_Info_create MPI_Info_create
#pragma _HP_SECONDARY_DEF MPIASP_Info_delete MPI_Info_delete
#pragma _HP_SECONDARY_DEF MPIASP_Info_dup MPI_Info_dup
#pragma _HP_SECONDARY_DEF MPIASP_Info_free MPI_Info_free
#pragma _HP_SECONDARY_DEF MPIASP_Info_get MPI_Info_get
#pragma _HP_SECONDARY_DEF MPIASP_Info_get_nkeys MPI_Info_get_nkeys
#pragma _HP_SECONDARY_DEF MPIASP_Info_get_nthkey MPI_Info_get_nthkey
#pragma _HP_SECONDARY_DEF MPIASP_Info_get_valuelen MPI_Info_get_valuelen
#pragma _HP_SECONDARY_DEF MPIASP_Info_set MPI_Info_set
#pragma _HP_SECONDARY_DEF MPIASP_Close_port MPI_Close_port
#pragma _HP_SECONDARY_DEF MPIASP_Comm_accept MPI_Comm_accept
#pragma _HP_SECONDARY_DEF MPIASP_Comm_connect MPI_Comm_connect
#pragma _HP_SECONDARY_DEF MPIASP_Comm_disconnect MPI_Comm_disconnect
#pragma _HP_SECONDARY_DEF MPIASP_Comm_get_parent MPI_Comm_get_parent
#pragma _HP_SECONDARY_DEF MPIASP_Comm_join MPI_Comm_join
#pragma _HP_SECONDARY_DEF MPIASP_Comm_spawn MPI_Comm_spawn
#pragma _HP_SECONDARY_DEF MPIASP_Comm_spawn_multiple MPI_Comm_spawn_multiple
#pragma _HP_SECONDARY_DEF MPIASP_Lookup_name MPI_Lookup_name
#pragma _HP_SECONDARY_DEF MPIASP_Open_port MPI_Open_port
#pragma _HP_SECONDARY_DEF MPIASP_Publish_name MPI_Publish_name
#pragma _HP_SECONDARY_DEF MPIASP_Unpublish_name MPI_Unpublish_name
#pragma _HP_SECONDARY_DEF MPIASP_Cartdim_get MPI_Cartdim_get
#pragma _HP_SECONDARY_DEF MPIASP_Cart_coords MPI_Cart_coords
#pragma _HP_SECONDARY_DEF MPIASP_Cart_create MPI_Cart_create
#pragma _HP_SECONDARY_DEF MPIASP_Cart_get MPI_Cart_get
#pragma _HP_SECONDARY_DEF MPIASP_Cart_map MPI_Cart_map
#pragma _HP_SECONDARY_DEF MPIASP_Cart_rank MPI_Cart_rank
#pragma _HP_SECONDARY_DEF MPIASP_Cart_shift MPI_Cart_shift
#pragma _HP_SECONDARY_DEF MPIASP_Cart_sub MPI_Cart_sub
#pragma _HP_SECONDARY_DEF MPIASP_Dims_create MPI_Dims_create
#pragma _HP_SECONDARY_DEF MPIASP_Graph_create MPI_Graph_create
#pragma _HP_SECONDARY_DEF MPIASP_Dist_graph_create MPI_Dist_graph_create
#pragma _HP_SECONDARY_DEF MPIASP_Dist_graph_create_adjacent MPI_Dist_graph_create_adjacent
#pragma _HP_SECONDARY_DEF MPIASP_Graphdims_get MPI_Graphdims_get
#pragma _HP_SECONDARY_DEF MPIASP_Graph_neighbors_count MPI_Graph_neighbors_count
#pragma _HP_SECONDARY_DEF MPIASP_Graph_get MPI_Graph_get
#pragma _HP_SECONDARY_DEF MPIASP_Graph_map MPI_Graph_map
#pragma _HP_SECONDARY_DEF MPIASP_Graph_neighbors MPI_Graph_neighbors
#pragma _HP_SECONDARY_DEF MPIASP_Dist_graph_neighbors MPI_Dist_graph_neighbors
#pragma _HP_SECONDARY_DEF MPIASP_Dist_graph_neighbors_count MPI_Dist_graph_neighbors_count
#pragma _HP_SECONDARY_DEF MPIASP_Topo_test MPI_Topo_test
#pragma _HP_SECONDARY_DEF MPIASP_Wtime MPI_Wtime
#pragma _HP_SECONDARY_DEF MPIASP_Wtick MPI_Wtick

#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPI_Init_thread as MPIASP_Init_thread
#pragma _CRI duplicate MPI_Attr_delete as MPIASP_Attr_delete
#pragma _CRI duplicate MPI_Attr_get as MPIASP_Attr_get
#pragma _CRI duplicate MPI_Attr_put as MPIASP_Attr_put
#pragma _CRI duplicate MPI_Comm_create_keyval as MPIASP_Comm_create_keyval
#pragma _CRI duplicate MPI_Comm_delete_attr as MPIASP_Comm_delete_attr
#pragma _CRI duplicate MPI_Comm_free_keyval as MPIASP_Comm_free_keyval
#pragma _CRI duplicate MPI_Comm_get_attr as MPIASP_Comm_get_attr
#pragma _CRI duplicate MPI_Comm_set_attr as MPIASP_Comm_set_attr
#pragma _CRI duplicate MPI_Keyval_create as MPIASP_Keyval_create
#pragma _CRI duplicate MPI_Keyval_free as MPIASP_Keyval_free
#pragma _CRI duplicate MPI_Type_create_keyval as MPIASP_Type_create_keyval
#pragma _CRI duplicate MPI_Type_delete_attr as MPIASP_Type_delete_attr
#pragma _CRI duplicate MPI_Type_free_keyval as MPIASP_Type_free_keyval
#pragma _CRI duplicate MPI_Type_get_attr as MPIASP_Type_get_attr
#pragma _CRI duplicate MPI_Type_set_attr as MPIASP_Type_set_attr
#pragma _CRI duplicate MPI_Win_create_keyval as MPIASP_Win_create_keyval
#pragma _CRI duplicate MPI_Win_delete_attr as MPIASP_Win_delete_attr
#pragma _CRI duplicate MPI_Win_free_keyval as MPIASP_Win_free_keyval
#pragma _CRI duplicate MPI_Win_get_attr as MPIASP_Win_get_attr
#pragma _CRI duplicate MPI_Win_set_attr as MPIASP_Win_set_attr
#pragma _CRI duplicate MPI_Allgather as MPIASP_Allgather
#pragma _CRI duplicate MPI_Allgatherv as MPIASP_Allgatherv
#pragma _CRI duplicate MPI_Allreduce as MPIASP_Allreduce
#pragma _CRI duplicate MPI_Alltoall as MPIASP_Alltoall
#pragma _CRI duplicate MPI_Alltoallv as MPIASP_Alltoallv
#pragma _CRI duplicate MPI_Alltoallw as MPIASP_Alltoallw
#pragma _CRI duplicate MPI_Barrier as MPIASP_Barrier
#pragma _CRI duplicate MPI_Bcast as MPIASP_Bcast
#pragma _CRI duplicate MPI_Exscan as MPIASP_Exscan
#pragma _CRI duplicate MPI_Gather as MPIASP_Gather
#pragma _CRI duplicate MPI_Gatherv as MPIASP_Gatherv
#pragma _CRI duplicate MPI_Op_create as MPIASP_Op_create
#pragma _CRI duplicate MPI_Op_free as MPIASP_Op_free
#pragma _CRI duplicate MPI_Op_commutative as MPIASP_Op_commutative
#pragma _CRI duplicate MPI_Reduce as MPIASP_Reduce
#pragma _CRI duplicate MPI_Reduce_local as MPIASP_Reduce_local
#pragma _CRI duplicate MPI_Reduce_scatter as MPIASP_Reduce_scatter
#pragma _CRI duplicate MPI_Reduce_scatter_block as MPIASP_Reduce_scatter_block
#pragma _CRI duplicate MPI_Scan as MPIASP_Scan
#pragma _CRI duplicate MPI_Scatter as MPIASP_Scatter
#pragma _CRI duplicate MPI_Scatterv as MPIASP_Scatterv
#pragma _CRI duplicate MPI_Comm_compare as MPIASP_Comm_compare
#pragma _CRI duplicate MPI_Comm_create as MPIASP_Comm_create
#pragma _CRI duplicate MPI_Comm_free as MPIASP_Comm_free
#pragma _CRI duplicate MPI_Comm_get_name as MPIASP_Comm_get_name
#pragma _CRI duplicate MPI_Comm_group as MPIASP_Comm_group
#pragma _CRI duplicate MPI_Comm_rank as MPIASP_Comm_rank
#pragma _CRI duplicate MPI_Comm_remote_group as MPIASP_Comm_remote_group
#pragma _CRI duplicate MPI_Comm_remote_size as MPIASP_Comm_remote_size
#pragma _CRI duplicate MPI_Comm_set_name as MPIASP_Comm_set_name
#pragma _CRI duplicate MPI_Comm_size as MPIASP_Comm_size
#pragma _CRI duplicate MPI_Comm_split as MPIASP_Comm_split
#pragma _CRI duplicate MPI_Comm_test_inter as MPIASP_Comm_test_inter
#pragma _CRI duplicate MPI_Intercomm_create as MPIASP_Intercomm_create
#pragma _CRI duplicate MPI_Intercomm_merge as MPIASP_Intercomm_merge
#pragma _CRI duplicate MPI_Address as MPIASP_Address
#pragma _CRI duplicate MPI_Get_address as MPIASP_Get_address
#pragma _CRI duplicate MPI_Get_count as MPIASP_Get_count
#pragma _CRI duplicate MPI_Get_elements as MPIASP_Get_elements
#pragma _CRI duplicate MPI_Pack as MPIASP_Pack
#pragma _CRI duplicate MPI_Pack_external as MPIASP_Pack_external
#pragma _CRI duplicate MPI_Pack_external_size as MPIASP_Pack_external_size
#pragma _CRI duplicate MPI_Pack_size as MPIASP_Pack_size
#pragma _CRI duplicate MPI_Status_set_elements as MPIASP_Status_set_elements
#pragma _CRI duplicate MPI_Type_commit as MPIASP_Type_commit
#pragma _CRI duplicate MPI_Type_contiguous as MPIASP_Type_contiguous
#pragma _CRI duplicate MPI_Type_create_darray as MPIASP_Type_create_darray
#pragma _CRI duplicate MPI_Type_create_hindexed as MPIASP_Type_create_hindexed
#pragma _CRI duplicate MPI_Type_create_hvector as MPIASP_Type_create_hvector
#pragma _CRI duplicate MPI_Type_create_indexed_block as MPIASP_Type_create_indexed_block
#pragma _CRI duplicate MPI_Type_create_hindexed_block as MPIASP_Type_create_hindexed_block
#pragma _CRI duplicate MPI_Type_create_resized as MPIASP_Type_create_resized
#pragma _CRI duplicate MPI_Type_create_struct as MPIASP_Type_create_struct
#pragma _CRI duplicate MPI_Type_create_subarray as MPIASP_Type_create_subarray
#pragma _CRI duplicate MPI_Type_dup as MPIASP_Type_dup
#pragma _CRI duplicate MPI_Type_extent as MPIASP_Type_extent
#pragma _CRI duplicate MPI_Type_free as MPIASP_Type_free
#pragma _CRI duplicate MPI_Type_get_contents as MPIASP_Type_get_contents
#pragma _CRI duplicate MPI_Type_get_envelope as MPIASP_Type_get_envelope
#pragma _CRI duplicate MPI_Type_get_extent as MPIASP_Type_get_extent
#pragma _CRI duplicate MPI_Type_get_name as MPIASP_Type_get_name
#pragma _CRI duplicate MPI_Type_get_true_extent as MPIASP_Type_get_true_extent
#pragma _CRI duplicate MPI_Type_hindexed as MPIASP_Type_hindexed
#pragma _CRI duplicate MPI_Type_hvector as MPIASP_Type_hvector
#pragma _CRI duplicate MPI_Type_indexed as MPIASP_Type_indexed
#pragma _CRI duplicate MPI_Type_lb as MPIASP_Type_lb
#pragma _CRI duplicate MPI_Type_match_size as MPIASP_Type_match_size
#pragma _CRI duplicate MPI_Type_set_name as MPIASP_Type_set_name
#pragma _CRI duplicate MPI_Type_size as MPIASP_Type_size
#pragma _CRI duplicate MPI_Type_struct as MPIASP_Type_struct
#pragma _CRI duplicate MPI_Type_ub as MPIASP_Type_ub
#pragma _CRI duplicate MPI_Type_vector as MPIASP_Type_vector
#pragma _CRI duplicate MPI_Unpack as MPIASP_Unpack
#pragma _CRI duplicate MPI_Unpack_external as MPIASP_Unpack_external
#pragma _CRI duplicate MPI_Add_error_class as MPIASP_Add_error_class
#pragma _CRI duplicate MPI_Add_error_code as MPIASP_Add_error_code
#pragma _CRI duplicate MPI_Add_error_string as MPIASP_Add_error_string
#pragma _CRI duplicate MPI_Comm_call_errhandler as MPIASP_Comm_call_errhandler
#pragma _CRI duplicate MPI_Comm_create_errhandler as MPIASP_Comm_create_errhandler
#pragma _CRI duplicate MPI_Comm_get_errhandler as MPIASP_Comm_get_errhandler
#pragma _CRI duplicate MPI_Comm_set_errhandler as MPIASP_Comm_set_errhandler
#pragma _CRI duplicate MPI_Errhandler_create as MPIASP_Errhandler_create
#pragma _CRI duplicate MPI_Errhandler_free as MPIASP_Errhandler_free
#pragma _CRI duplicate MPI_Errhandler_get as MPIASP_Errhandler_get
#pragma _CRI duplicate MPI_Errhandler_set as MPIASP_Errhandler_set
#pragma _CRI duplicate MPI_Error_class as MPIASP_Error_class
#pragma _CRI duplicate MPI_Error_string as MPIASP_Error_string
#pragma _CRI duplicate MPI_Win_call_errhandler as MPIASP_Win_call_errhandler
#pragma _CRI duplicate MPI_Win_create_errhandler as MPIASP_Win_create_errhandler
#pragma _CRI duplicate MPI_Win_get_errhandler as MPIASP_Win_get_errhandler
#pragma _CRI duplicate MPI_Win_set_errhandler as MPIASP_Win_set_errhandler
#pragma _CRI duplicate MPI_Group_compare as MPIASP_Group_compare
#pragma _CRI duplicate MPI_Group_difference as MPIASP_Group_difference
#pragma _CRI duplicate MPI_Group_excl as MPIASP_Group_excl
#pragma _CRI duplicate MPI_Group_free as MPIASP_Group_free
#pragma _CRI duplicate MPI_Group_incl as MPIASP_Group_incl
#pragma _CRI duplicate MPI_Group_intersection as MPIASP_Group_intersection
#pragma _CRI duplicate MPI_Group_range_excl as MPIASP_Group_range_excl
#pragma _CRI duplicate MPI_Group_range_incl as MPIASP_Group_range_incl
#pragma _CRI duplicate MPI_Group_rank as MPIASP_Group_rank
#pragma _CRI duplicate MPI_Group_size as MPIASP_Group_size
#pragma _CRI duplicate MPI_Group_translate_ranks as MPIASP_Group_translate_ranks
#pragma _CRI duplicate MPI_Group_union as MPIASP_Group_union
#pragma _CRI duplicate MPI_Finalized as MPIASP_Finalized
#pragma _CRI duplicate MPI_Initialized as MPIASP_Initialized
#pragma _CRI duplicate MPI_Is_thread_main as MPIASP_Is_thread_main
#pragma _CRI duplicate MPI_Query_thread as MPIASP_Query_thread
#pragma _CRI duplicate MPI_Get_processor_name as MPIASP_Get_processor_name
#pragma _CRI duplicate MPI_Pcontrol as MPIASP_Pcontrol
#pragma _CRI duplicate MPI_Get_version as MPIASP_Get_version
#pragma _CRI duplicate MPI_Bsend as MPIASP_Bsend
#pragma _CRI duplicate MPI_Bsend_init as MPIASP_Bsend_init
#pragma _CRI duplicate MPI_Buffer_attach as MPIASP_Buffer_attach
#pragma _CRI duplicate MPI_Buffer_detach as MPIASP_Buffer_detach
#pragma _CRI duplicate MPI_Cancel as MPIASP_Cancel
#pragma _CRI duplicate MPI_Grequest_complete as MPIASP_Grequest_complete
#pragma _CRI duplicate MPI_Grequest_start as MPIASP_Grequest_start
#pragma _CRI duplicate MPI_Ibsend as MPIASP_Ibsend
#pragma _CRI duplicate MPI_Iprobe as MPIASP_Iprobe
#pragma _CRI duplicate MPI_Irecv as MPIASP_Irecv
#pragma _CRI duplicate MPI_Irsend as MPIASP_Irsend
#pragma _CRI duplicate MPI_Isend as MPIASP_Isend
#pragma _CRI duplicate MPI_Issend as MPIASP_Issend
#pragma _CRI duplicate MPI_Probe as MPIASP_Probe
#pragma _CRI duplicate MPI_Recv as MPIASP_Recv
#pragma _CRI duplicate MPI_Recv_init as MPIASP_Recv_init
#pragma _CRI duplicate MPI_Request_free as MPIASP_Request_free
#pragma _CRI duplicate MPI_Request_get_status as MPIASP_Request_get_status
#pragma _CRI duplicate MPI_Rsend as MPIASP_Rsend
#pragma _CRI duplicate MPI_Rsend_init as MPIASP_Rsend_init
#pragma _CRI duplicate MPI_Send as MPIASP_Send
#pragma _CRI duplicate MPI_Sendrecv as MPIASP_Sendrecv
#pragma _CRI duplicate MPI_Sendrecv_replace as MPIASP_Sendrecv_replace
#pragma _CRI duplicate MPI_Send_init as MPIASP_Send_init
#pragma _CRI duplicate MPI_Ssend as MPIASP_Ssend
#pragma _CRI duplicate MPI_Ssend_init as MPIASP_Ssend_init
#pragma _CRI duplicate MPI_Start as MPIASP_Start
#pragma _CRI duplicate MPI_Startall as MPIASP_Startall
#pragma _CRI duplicate MPI_Status_set_cancelled as MPIASP_Status_set_cancelled
#pragma _CRI duplicate MPI_Test as MPIASP_Test
#pragma _CRI duplicate MPI_Testall as MPIASP_Testall
#pragma _CRI duplicate MPI_Testany as MPIASP_Testany
#pragma _CRI duplicate MPI_Testsome as MPIASP_Testsome
#pragma _CRI duplicate MPI_Test_cancelled as MPIASP_Test_cancelled
#pragma _CRI duplicate MPI_Wait as MPIASP_Wait
#pragma _CRI duplicate MPI_Waitall as MPIASP_Waitall
#pragma _CRI duplicate MPI_Waitany as MPIASP_Waitany
#pragma _CRI duplicate MPI_Waitsome as MPIASP_Waitsome
#pragma _CRI duplicate MPI_Accumulate as MPIASP_Accumulate
#pragma _CRI duplicate MPI_Alloc_mem as MPIASP_Alloc_mem
#pragma _CRI duplicate MPI_Free_mem as MPIASP_Free_mem
#pragma _CRI duplicate MPI_Get as MPIASP_Get
#pragma _CRI duplicate MPI_Put as MPIASP_Put
#pragma _CRI duplicate MPI_Win_complete as MPIASP_Win_complete
#pragma _CRI duplicate MPI_Win_fence as MPIASP_Win_fence
#pragma _CRI duplicate MPI_Win_free as MPIASP_Win_free
#pragma _CRI duplicate MPI_Win_get_group as MPIASP_Win_get_group
#pragma _CRI duplicate MPI_Win_get_name as MPIASP_Win_get_name
#pragma _CRI duplicate MPI_Win_lock as MPIASP_Win_lock
#pragma _CRI duplicate MPI_Win_post as MPIASP_Win_post
#pragma _CRI duplicate MPI_Win_set_name as MPIASP_Win_set_name
#pragma _CRI duplicate MPI_Win_start as MPIASP_Win_start
#pragma _CRI duplicate MPI_Win_test as MPIASP_Win_test
#pragma _CRI duplicate MPI_Win_unlock as MPIASP_Win_unlock
#pragma _CRI duplicate MPI_Win_wait as MPIASP_Win_wait
#pragma _CRI duplicate MPI_Info_create as MPIASP_Info_create
#pragma _CRI duplicate MPI_Info_delete as MPIASP_Info_delete
#pragma _CRI duplicate MPI_Info_dup as MPIASP_Info_dup
#pragma _CRI duplicate MPI_Info_free as MPIASP_Info_free
#pragma _CRI duplicate MPI_Info_get as MPIASP_Info_get
#pragma _CRI duplicate MPI_Info_get_nkeys as MPIASP_Info_get_nkeys
#pragma _CRI duplicate MPI_Info_get_nthkey as MPIASP_Info_get_nthkey
#pragma _CRI duplicate MPI_Info_get_valuelen as MPIASP_Info_get_valuelen
#pragma _CRI duplicate MPI_Info_set as MPIASP_Info_set
#pragma _CRI duplicate MPI_Close_port as MPIASP_Close_port
#pragma _CRI duplicate MPI_Comm_accept as MPIASP_Comm_accept
#pragma _CRI duplicate MPI_Comm_connect as MPIASP_Comm_connect
#pragma _CRI duplicate MPI_Comm_disconnect as MPIASP_Comm_disconnect
#pragma _CRI duplicate MPI_Comm_get_parent as MPIASP_Comm_get_parent
#pragma _CRI duplicate MPI_Comm_join as MPIASP_Comm_join
#pragma _CRI duplicate MPI_Comm_spawn as MPIASP_Comm_spawn
#pragma _CRI duplicate MPI_Comm_spawn_multiple as MPIASP_Comm_spawn_multiple
#pragma _CRI duplicate MPI_Lookup_name as MPIASP_Lookup_name
#pragma _CRI duplicate MPI_Open_port as MPIASP_Open_port
#pragma _CRI duplicate MPI_Publish_name as MPIASP_Publish_name
#pragma _CRI duplicate MPI_Unpublish_name as MPIASP_Unpublish_name
#pragma _CRI duplicate MPI_Cartdim_get as MPIASP_Cartdim_get
#pragma _CRI duplicate MPI_Cart_coords as MPIASP_Cart_coords
#pragma _CRI duplicate MPI_Cart_create as MPIASP_Cart_create
#pragma _CRI duplicate MPI_Cart_get as MPIASP_Cart_get
#pragma _CRI duplicate MPI_Cart_map as MPIASP_Cart_map
#pragma _CRI duplicate MPI_Cart_rank as MPIASP_Cart_rank
#pragma _CRI duplicate MPI_Cart_shift as MPIASP_Cart_shift
#pragma _CRI duplicate MPI_Cart_sub as MPIASP_Cart_sub
#pragma _CRI duplicate MPI_Dims_create as MPIASP_Dims_create
#pragma _CRI duplicate MPI_Graph_create as MPIASP_Graph_create
#pragma _CRI duplicate MPI_Dist_graph_create as MPIASP_Dist_graph_create
#pragma _CRI duplicate MPI_Dist_graph_create_adjacent as MPIASP_Dist_graph_create_adjacent
#pragma _CRI duplicate MPI_Graphdims_get as MPIASP_Graphdims_get
#pragma _CRI duplicate MPI_Graph_neighbors_count as MPIASP_Graph_neighbors_count
#pragma _CRI duplicate MPI_Graph_get as MPIASP_Graph_get
#pragma _CRI duplicate MPI_Graph_map as MPIASP_Graph_map
#pragma _CRI duplicate MPI_Graph_neighbors as MPIASP_Graph_neighbors
#pragma _CRI duplicate MPI_Dist_graph_neighbors as MPIASP_Dist_graph_neighbors
#pragma _CRI duplicate MPI_Dist_graph_neighbors_count as MPIASP_Dist_graph_neighbors_count
#pragma _CRI duplicate MPI_Topo_test as MPIASP_Topo_test
#pragma _CRI duplicate MPI_Wtime as MPIASP_Wtime
#pragma _CRI duplicate MPI_Wtick as MPIASP_Wtick
#endif
/* -- End Profiling Symbol Block */

#undef FCNAME
#define FCNAME MPIASP_Init_thread
int MPIASP_Init_thread(int *argc, char ***argv, int required, int *provided) {
    int result;

    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;

    return PMPI_Init_thread(argc, argv, required, provided);
}

#undef FCNAME
#define FCNAME MPIASP_Attr_delete
int MPIASP_Attr_delete(MPI_Comm comm, int keyval) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Attr_delete(comm, keyval);
}

#undef FCNAME
#define FCNAME MPIASP_Attr_get
int MPIASP_Attr_get(MPI_Comm comm, int keyval, void *attr_value, int *flag) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Attr_get(comm, keyval, attr_value, flag);
}

#undef FCNAME
#define FCNAME MPIASP_Attr_put
int MPIASP_Attr_put(MPI_Comm comm, int keyval, void *attr_value) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Attr_put(comm, keyval, attr_value);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_create_keyval
int MPIASP_Comm_create_keyval(MPI_Comm_copy_attr_function *comm_copy_attr_fn,
        MPI_Comm_delete_attr_function *comm_delete_attr_fn, int *comm_keyval,
        void *extra_state) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_create_keyval(comm_copy_attr_fn, comm_delete_attr_fn,
            comm_keyval, extra_state);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_delete_attr
int MPIASP_Comm_delete_attr(MPI_Comm comm, int comm_keyval) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_delete_attr(comm, comm_keyval);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_free_keyval
int MPIASP_Comm_free_keyval(int *comm_keyval) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_free_keyval(comm_keyval);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_get_attr
int MPIASP_Comm_get_attr(MPI_Comm comm, int comm_keyval, void *attribute_val,
        int *flag) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_get_attr(comm, comm_keyval, attribute_val, flag);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_set_attr
int MPIASP_Comm_set_attr(MPI_Comm comm, int comm_keyval, void *attribute_val) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_set_attr(comm, comm_keyval, attribute_val);
}

#undef FCNAME
#define FCNAME MPIASP_Keyval_create
int MPIASP_Keyval_create(MPI_Copy_function *copy_fn,
        MPI_Delete_function *delete_fn, int *keyval, void *extra_state) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Keyval_create(copy_fn, delete_fn, keyval, extra_state);
}

#undef FCNAME
#define FCNAME MPIASP_Keyval_free
int MPIASP_Keyval_free(int *keyval) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Keyval_free(keyval);
}

#undef FCNAME
#define FCNAME MPIASP_Type_create_keyval
int MPIASP_Type_create_keyval(MPI_Type_copy_attr_function *type_copy_attr_fn,
        MPI_Type_delete_attr_function *type_delete_attr_fn, int *type_keyval,
        void *extra_state) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_create_keyval(type_copy_attr_fn, type_delete_attr_fn,
            type_keyval, extra_state);
}

#undef FCNAME
#define FCNAME MPIASP_Type_delete_attr
int MPIASP_Type_delete_attr(MPI_Datatype type, int type_keyval) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_delete_attr(type, type_keyval);
}

#undef FCNAME
#define FCNAME MPIASP_Type_free_keyval
int MPIASP_Type_free_keyval(int *type_keyval) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_free_keyval(type_keyval);
}

#undef FCNAME
#define FCNAME MPIASP_Type_get_attr
int MPIASP_Type_get_attr(MPI_Datatype type, int type_keyval,
        void *attribute_val, int *flag) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_get_attr(type, type_keyval, attribute_val, flag);
}

#undef FCNAME
#define FCNAME MPIASP_Type_set_attr
int MPIASP_Type_set_attr(MPI_Datatype type, int type_keyval,
        void *attribute_val) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_set_attr(type, type_keyval, attribute_val);
}

#undef FCNAME
#define FCNAME MPIASP_Win_create_keyval
int MPIASP_Win_create_keyval(MPI_Win_copy_attr_function *win_copy_attr_fn,
        MPI_Win_delete_attr_function *win_delete_attr_fn, int *win_keyval,
        void *extra_state) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_create_keyval(win_copy_attr_fn, win_delete_attr_fn,
            win_keyval, extra_state);
}

#undef FCNAME
#define FCNAME MPIASP_Win_delete_attr
int MPIASP_Win_delete_attr(MPI_Win win, int win_keyval) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_delete_attr(win, win_keyval);
}

#undef FCNAME
#define FCNAME MPIASP_Win_free_keyval
int MPIASP_Win_free_keyval(int *win_keyval) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_free_keyval(win_keyval);
}

#undef FCNAME
#define FCNAME MPIASP_Win_get_attr
int MPIASP_Win_get_attr(MPI_Win win, int win_keyval, void *attribute_val,
        int *flag) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_get_attr(win, win_keyval, attribute_val, flag);
}

#undef FCNAME
#define FCNAME MPIASP_Win_set_attr
int MPIASP_Win_set_attr(MPI_Win win, int win_keyval, void *attribute_val) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_set_attr(win, win_keyval, attribute_val);
}

#undef FCNAME
#define FCNAME MPIASP_Allgather
int MPIASP_Allgather(void *sendbuf, int sendcount, MPI_Datatype sendtype,
        void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Allgather(sendbuf, sendcount, sendtype, recvbuf, recvcount,
            recvtype, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Allgatherv
int MPIASP_Allgatherv(void *sendbuf, int sendcount, MPI_Datatype sendtype,
        void *recvbuf, int *recvcounts, int *displs, MPI_Datatype recvtype,
        MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Allgatherv(sendbuf, sendcount, sendtype, recvbuf, recvcounts,
            displs, recvtype, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Allreduce
int MPIASP_Allreduce(void *sendbuf, void *recvbuf, int count,
        MPI_Datatype datatype, MPI_Op op, MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Alltoall
int MPIASP_Alltoall(void *sendbuf, int sendcount, MPI_Datatype sendtype,
        void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Alltoall(sendbuf, sendcount, sendtype, recvbuf, recvcount,
            recvtype, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Alltoallv
int MPIASP_Alltoallv(void *sendbuf, int *sendcnts, int *sdispls,
        MPI_Datatype sendtype, void *recvbuf, int *recvcnts, int *rdispls,
        MPI_Datatype recvtype, MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Alltoallv(sendbuf, sendcnts, sdispls, sendtype, recvbuf,
            recvcnts, rdispls, recvtype, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Alltoallw
int MPIASP_Alltoallw(void *sendbuf, int *sendcnts, int *sdispls,
        MPI_Datatype *sendtypes, void *recvbuf, int *recvcnts, int *rdispls,
        MPI_Datatype *recvtypes, MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Alltoallw(sendbuf, sendcnts, sdispls, sendtypes, recvbuf,
            recvcnts, rdispls, recvtypes, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Barrier
int MPIASP_Barrier(MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Barrier(comm);
}

#undef FCNAME
#define FCNAME MPIASP_Bcast
int MPIASP_Bcast(void *buffer, int count, MPI_Datatype datatype, int root,
        MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Bcast(buffer, count, datatype, root, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Exscan
int MPIASP_Exscan(void *sendbuf, void *recvbuf, int count,
        MPI_Datatype datatype, MPI_Op op, MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Exscan(sendbuf, recvbuf, count, datatype, op, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Gather
int MPIASP_Gather(void *sendbuf, int sendcnt, MPI_Datatype sendtype,
        void *recvbuf, int recvcnt, MPI_Datatype recvtype, int root,
        MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Gather(sendbuf, sendcnt, sendtype, recvbuf, recvcnt, recvtype,
            root, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Gatherv
int MPIASP_Gatherv(void *sendbuf, int sendcnt, MPI_Datatype sendtype,
        void *recvbuf, int *recvcnts, int *displs, MPI_Datatype recvtype,
        int root, MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Gatherv(sendbuf, sendcnt, sendtype, recvbuf, recvcnts, displs,
            recvtype, root, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Op_create
int MPIASP_Op_create(MPI_User_function *function, int commute, MPI_Op *op) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Op_create(function, commute, op);
}

#undef FCNAME
#define FCNAME MPIASP_Op_free
int MPIASP_Op_free(MPI_Op *op) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Op_free(op);
}

#undef FCNAME
#define FCNAME MPIASP_Op_commutative
int MPIASP_Op_commutative(MPI_Op op, int *commute) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Op_commutative(op, commute);
}

#undef FCNAME
#define FCNAME MPIASP_Reduce
int MPIASP_Reduce(void *sendbuf, void *recvbuf, int count,
        MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Reduce(sendbuf, recvbuf, count, datatype, op, root, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Reduce_local
int MPIASP_Reduce_local(void *inbuf, void *inoutbuf, int count,
        MPI_Datatype datatype, MPI_Op op) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Reduce_local(inbuf, inoutbuf, count, datatype, op);
}

#undef FCNAME
#define FCNAME MPIASP_Reduce_scatter
int MPIASP_Reduce_scatter(void *sendbuf, void *recvbuf, int *recvcnts,
        MPI_Datatype datatype, MPI_Op op, MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Reduce_scatter(sendbuf, recvbuf, recvcnts, datatype, op, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Reduce_scatter_block
int MPIASP_Reduce_scatter_block(void *sendbuf, void *recvbuf, int recvcount,
        MPI_Datatype datatype, MPI_Op op, MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Reduce_scatter_block(sendbuf, recvbuf, recvcount, datatype, op,
            comm);
}

#undef FCNAME
#define FCNAME MPIASP_Scan
int MPIASP_Scan(void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
        MPI_Op op, MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Scan(sendbuf, recvbuf, count, datatype, op, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Scatter
int MPIASP_Scatter(void *sendbuf, int sendcnt, MPI_Datatype sendtype,
        void *recvbuf, int recvcnt, MPI_Datatype recvtype, int root,
        MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Scatter(sendbuf, sendcnt, sendtype, recvbuf, recvcnt, recvtype,
            root, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Scatterv
int MPIASP_Scatterv(void *sendbuf, int *sendcnts, int *displs,
        MPI_Datatype sendtype, void *recvbuf, int recvcnt,
        MPI_Datatype recvtype, int root, MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Scatterv(sendbuf, sendcnts, displs, sendtype, recvbuf, recvcnt,
            recvtype, root, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_compare
int MPIASP_Comm_compare(MPI_Comm comm1, MPI_Comm comm2, int *result) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_compare(comm1, comm2, result);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_create
int MPIASP_Comm_create(MPI_Comm comm, MPI_Group group, MPI_Comm *newcomm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_create(comm, group, newcomm);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_free
int MPIASP_Comm_free(MPI_Comm *comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_free(comm);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_get_name
int MPIASP_Comm_get_name(MPI_Comm comm, char *comm_name, int *resultlen) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_get_name(comm, comm_name, resultlen);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_group
int MPIASP_Comm_group(MPI_Comm comm, MPI_Group *group) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_group(comm, group);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_rank
int MPIASP_Comm_rank(MPI_Comm comm, int *rank) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_rank(comm, rank);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_remote_group
int MPIASP_Comm_remote_group(MPI_Comm comm, MPI_Group *group) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_remote_group(comm, group);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_remote_size
int MPIASP_Comm_remote_size(MPI_Comm comm, int *size) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_remote_size(comm, size);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_set_name
int MPIASP_Comm_set_name(MPI_Comm comm, char *comm_name) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_set_name(comm, comm_name);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_size
int MPIASP_Comm_size(MPI_Comm comm, int *size) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_size(comm, size);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_split
int MPIASP_Comm_split(MPI_Comm comm, int color, int key, MPI_Comm *newcomm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_split(comm, color, key, newcomm);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_test_inter
int MPIASP_Comm_test_inter(MPI_Comm comm, int *flag) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_test_inter(comm, flag);
}

#undef FCNAME
#define FCNAME MPIASP_Intercomm_create
int MPIASP_Intercomm_create(MPI_Comm local_comm, int local_leader,
        MPI_Comm peer_comm, int remote_leader, int tag, MPI_Comm *newintercomm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Intercomm_create(local_comm, local_leader, peer_comm,
            remote_leader, tag, newintercomm);
}

#undef FCNAME
#define FCNAME MPIASP_Intercomm_merge
int MPIASP_Intercomm_merge(MPI_Comm intercomm, int high, MPI_Comm *newintracomm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Intercomm_merge(intercomm, high, newintracomm);
}

#undef FCNAME
#define FCNAME MPIASP_Address
int MPIASP_Address(void *location, MPI_Aint *address) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Address(location, address);
}

#undef FCNAME
#define FCNAME MPIASP_Get_address
int MPIASP_Get_address(void *location, MPI_Aint *address) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Get_address(location, address);
}

#undef FCNAME
#define FCNAME MPIASP_Get_count
int MPIASP_Get_count(MPI_Status *status, MPI_Datatype datatype, int *count) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Get_count(status, datatype, count);
}

#undef FCNAME
#define FCNAME MPIASP_Get_elements
int MPIASP_Get_elements(MPI_Status *status, MPI_Datatype datatype,
        int *elements) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Get_elements(status, datatype, elements);
}

#undef FCNAME
#define FCNAME MPIASP_Pack
int MPIASP_Pack(void *inbuf, int incount, MPI_Datatype datatype, void *outbuf,
        int outcount, int *position, MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Pack(inbuf, incount, datatype, outbuf, outcount, position, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Pack_external
int MPIASP_Pack_external(char *datarep, void *inbuf, int incount,
        MPI_Datatype datatype, void *outbuf, MPI_Aint outcount,
        MPI_Aint *position) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Pack_external(datarep, inbuf, incount, datatype, outbuf,
            outcount, position);
}

#undef FCNAME
#define FCNAME MPIASP_Pack_external_size
int MPIASP_Pack_external_size(char *datarep, int incount, MPI_Datatype datatype,
        MPI_Aint *size) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Pack_external_size(datarep, incount, datatype, size);
}

#undef FCNAME
#define FCNAME MPIASP_Pack_size
int MPIASP_Pack_size(int incount, MPI_Datatype datatype, MPI_Comm comm,
        int *size) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Pack_size(incount, datatype, comm, size);
}

#undef FCNAME
#define FCNAME MPIASP_Status_set_elements
int MPIASP_Status_set_elements(MPI_Status *status, MPI_Datatype datatype,
        int count) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Status_set_elements(status, datatype, count);
}

#undef FCNAME
#define FCNAME MPIASP_Type_commit
int MPIASP_Type_commit(MPI_Datatype *datatype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_commit(datatype);
}

#undef FCNAME
#define FCNAME MPIASP_Type_contiguous
int MPIASP_Type_contiguous(int count, MPI_Datatype old_type,
        MPI_Datatype *new_type_p) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_contiguous(count, old_type, new_type_p);
}

#undef FCNAME
#define FCNAME MPIASP_Type_create_darray
int MPIASP_Type_create_darray(int size, int rank, int ndims,
        int array_of_gsizes[], int array_of_distribs[], int array_of_dargs[],
        int array_of_psizes[], int order, MPI_Datatype oldtype,
        MPI_Datatype *newtype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_create_darray(size, rank, ndims, array_of_gsizes,
            array_of_distribs, array_of_dargs, array_of_psizes, order, oldtype,
            newtype);
}

#undef FCNAME
#define FCNAME MPIASP_Type_create_hindexed
int MPIASP_Type_create_hindexed(int count, int blocklengths[],
        MPI_Aint displacements[], MPI_Datatype oldtype, MPI_Datatype *newtype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_create_hindexed(count, blocklengths, displacements,
            oldtype, newtype);
}

#undef FCNAME
#define FCNAME MPIASP_Type_create_hvector
int MPIASP_Type_create_hvector(int count, int blocklength, MPI_Aint stride,
        MPI_Datatype oldtype, MPI_Datatype *newtype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_create_hvector(count, blocklength, stride, oldtype,
            newtype);
}

#undef FCNAME
#define FCNAME MPIASP_Type_create_indexed_block
int MPIASP_Type_create_indexed_block(int count, int blocklength,
        int array_of_displacements[], MPI_Datatype oldtype,
        MPI_Datatype *newtype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_create_indexed_block(count, blocklength,
            array_of_displacements, oldtype, newtype);
}

#undef FCNAME
#define FCNAME MPIASP_Type_create_hindexed_block
int MPIASP_Type_create_hindexed_block(int count, int blocklength,
        const MPI_Aint array_of_displacements[], MPI_Datatype oldtype,
        MPI_Datatype *newtype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_create_hindexed_block(count, blocklength,
            array_of_displacements, oldtype, newtype);
}

#undef FCNAME
#define FCNAME MPIASP_Type_create_resized
int MPIASP_Type_create_resized(MPI_Datatype oldtype, MPI_Aint lb,
        MPI_Aint extent, MPI_Datatype *newtype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_create_resized(oldtype, lb, extent, newtype);
}

#undef FCNAME
#define FCNAME MPIASP_Type_create_struct
int MPIASP_Type_create_struct(int count, int array_of_blocklengths[],
        MPI_Aint array_of_displacements[], MPI_Datatype array_of_types[],
        MPI_Datatype *newtype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_create_struct(count, array_of_blocklengths,
            array_of_displacements, array_of_types, newtype);
}

#undef FCNAME
#define FCNAME MPIASP_Type_create_subarray
int MPIASP_Type_create_subarray(int ndims, int array_of_sizes[],
        int array_of_subsizes[], int array_of_starts[], int order,
        MPI_Datatype oldtype, MPI_Datatype *newtype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_create_subarray(ndims, array_of_sizes, array_of_subsizes,
            array_of_starts, order, oldtype, newtype);
}

#undef FCNAME
#define FCNAME MPIASP_Type_dup
int MPIASP_Type_dup(MPI_Datatype datatype, MPI_Datatype *newtype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_dup(datatype, newtype);
}

#undef FCNAME
#define FCNAME MPIASP_Type_extent
int MPIASP_Type_extent(MPI_Datatype datatype, MPI_Aint *extent) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_extent(datatype, extent);
}

#undef FCNAME
#define FCNAME MPIASP_Type_free
int MPIASP_Type_free(MPI_Datatype *datatype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_free(datatype);
}

#undef FCNAME
#define FCNAME MPIASP_Type_get_contents
int MPIASP_Type_get_contents(MPI_Datatype datatype, int max_integers,
        int max_addresses, int max_datatypes, int array_of_integers[],
        MPI_Aint array_of_addresses[], MPI_Datatype array_of_datatypes[]) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_get_contents(datatype, max_integers, max_addresses,
            max_datatypes, array_of_integers, array_of_addresses,
            array_of_datatypes);
}

#undef FCNAME
#define FCNAME MPIASP_Type_get_envelope
int MPIASP_Type_get_envelope(MPI_Datatype datatype, int *num_integers,
        int *num_addresses, int *num_datatypes, int *combiner) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_get_envelope(datatype, num_integers, num_addresses,
            num_datatypes, combiner);
}

#undef FCNAME
#define FCNAME MPIASP_Type_get_extent
int MPIASP_Type_get_extent(MPI_Datatype datatype, MPI_Aint *lb,
        MPI_Aint *extent) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_get_extent(datatype, lb, extent);
}

#undef FCNAME
#define FCNAME MPIASP_Type_get_name
int MPIASP_Type_get_name(MPI_Datatype datatype, char *type_name, int *resultlen) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_get_name(datatype, type_name, resultlen);
}

#undef FCNAME
#define FCNAME MPIASP_Type_get_true_extent
int MPIASP_Type_get_true_extent(MPI_Datatype datatype, MPI_Aint *true_lb,
        MPI_Aint *true_extent) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_get_true_extent(datatype, true_lb, true_extent);
}

#undef FCNAME
#define FCNAME MPIASP_Type_hindexed
int MPIASP_Type_hindexed(int count, int blocklens[], MPI_Aint indices[],
        MPI_Datatype old_type, MPI_Datatype *newtype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_hindexed(count, blocklens, indices, old_type, newtype);
}

#undef FCNAME
#define FCNAME MPIASP_Type_hvector
int MPIASP_Type_hvector(int count, int blocklen, MPI_Aint stride,
        MPI_Datatype old_type, MPI_Datatype *newtype_p) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_hvector(count, blocklen, stride, old_type, newtype_p);
}

#undef FCNAME
#define FCNAME MPIASP_Type_indexed
int MPIASP_Type_indexed(int count, int blocklens[], int indices[],
        MPI_Datatype old_type, MPI_Datatype *newtype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_indexed(count, blocklens, indices, old_type, newtype);
}

#undef FCNAME
#define FCNAME MPIASP_Type_lb
int MPIASP_Type_lb(MPI_Datatype datatype, MPI_Aint *displacement) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_lb(datatype, displacement);
}

#undef FCNAME
#define FCNAME MPIASP_Type_match_size
int MPIASP_Type_match_size(int typeclass, int size, MPI_Datatype *datatype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_match_size(typeclass, size, datatype);
}

#undef FCNAME
#define FCNAME MPIASP_Type_set_name
int MPIASP_Type_set_name(MPI_Datatype type, char *type_name) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_set_name(type, type_name);
}

#undef FCNAME
#define FCNAME MPIASP_Type_size
int MPIASP_Type_size(MPI_Datatype datatype, int *size) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_size(datatype, size);
}

#undef FCNAME
#define FCNAME MPIASP_Type_struct
int MPIASP_Type_struct(int count, int blocklens[], MPI_Aint indices[],
        MPI_Datatype old_types[], MPI_Datatype *newtype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_struct(count, blocklens, indices, old_types, newtype);
}

#undef FCNAME
#define FCNAME MPIASP_Type_ub
int MPIASP_Type_ub(MPI_Datatype datatype, MPI_Aint *displacement) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_ub(datatype, displacement);
}

#undef FCNAME
#define FCNAME MPIASP_Type_vector
int MPIASP_Type_vector(int count, int blocklength, int stride,
        MPI_Datatype old_type, MPI_Datatype *newtype_p) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Type_vector(count, blocklength, stride, old_type, newtype_p);
}

#undef FCNAME
#define FCNAME MPIASP_Unpack
int MPIASP_Unpack(void *inbuf, int insize, int *position, void *outbuf,
        int outcount, MPI_Datatype datatype, MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Unpack(inbuf, insize, position, outbuf, outcount, datatype,
            comm);
}

#undef FCNAME
#define FCNAME MPIASP_Unpack_external
int MPIASP_Unpack_external(char *datarep, void *inbuf, MPI_Aint insize,
        MPI_Aint *position, void *outbuf, int outcount, MPI_Datatype datatype) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Unpack_external(datarep, inbuf, insize, position, outbuf,
            outcount, datatype);
}

#undef FCNAME
#define FCNAME MPIASP_Add_error_class
int MPIASP_Add_error_class(int *errorclass) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Add_error_class(errorclass);
}

#undef FCNAME
#define FCNAME MPIASP_Add_error_code
int MPIASP_Add_error_code(int errorclass, int *errorcode) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Add_error_code(errorclass, errorcode);
}

#undef FCNAME
#define FCNAME MPIASP_Add_error_string
int MPIASP_Add_error_string(int errorcode, char *string) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Add_error_string(errorcode, string);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_call_errhandler
int MPIASP_Comm_call_errhandler(MPI_Comm comm, int errorcode) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_call_errhandler(comm, errorcode);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_create_errhandler
int MPIASP_Comm_create_errhandler(MPI_Comm_errhandler_function *function,
        MPI_Errhandler *errhandler) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_create_errhandler(function, errhandler);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_get_errhandler
int MPIASP_Comm_get_errhandler(MPI_Comm comm, MPI_Errhandler *errhandler) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_get_errhandler(comm, errhandler);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_set_errhandler
int MPIASP_Comm_set_errhandler(MPI_Comm comm, MPI_Errhandler errhandler) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_set_errhandler(comm, errhandler);
}

#undef FCNAME
#define FCNAME MPIASP_Errhandler_create
int MPIASP_Errhandler_create(MPI_Handler_function *function,
        MPI_Errhandler *errhandler) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Errhandler_create(function, errhandler);
}

#undef FCNAME
#define FCNAME MPIASP_Errhandler_free
int MPIASP_Errhandler_free(MPI_Errhandler *errhandler) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Errhandler_free(errhandler);
}

#undef FCNAME
#define FCNAME MPIASP_Errhandler_get
int MPIASP_Errhandler_get(MPI_Comm comm, MPI_Errhandler *errhandler) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Errhandler_get(comm, errhandler);
}

#undef FCNAME
#define FCNAME MPIASP_Errhandler_set
int MPIASP_Errhandler_set(MPI_Comm comm, MPI_Errhandler errhandler) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Errhandler_set(comm, errhandler);
}

#undef FCNAME
#define FCNAME MPIASP_Error_class
int MPIASP_Error_class(int errorcode, int *errorclass) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Error_class(errorcode, errorclass);
}

#undef FCNAME
#define FCNAME MPIASP_Error_string
int MPIASP_Error_string(int errorcode, char *string, int *resultlen) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Error_string(errorcode, string, resultlen);
}

#undef FCNAME
#define FCNAME MPIASP_Win_call_errhandler
int MPIASP_Win_call_errhandler(MPI_Win win, int errorcode) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_call_errhandler(win, errorcode);
}

#undef FCNAME
#define FCNAME MPIASP_Win_create_errhandler
int MPIASP_Win_create_errhandler(MPI_Win_errhandler_function *function,
        MPI_Errhandler *errhandler) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_create_errhandler(function, errhandler);
}

#undef FCNAME
#define FCNAME MPIASP_Win_get_errhandler
int MPIASP_Win_get_errhandler(MPI_Win win, MPI_Errhandler *errhandler) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_get_errhandler(win, errhandler);
}

#undef FCNAME
#define FCNAME MPIASP_Win_set_errhandler
int MPIASP_Win_set_errhandler(MPI_Win win, MPI_Errhandler errhandler) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_set_errhandler(win, errhandler);
}

#undef FCNAME
#define FCNAME MPIASP_Group_compare
int MPIASP_Group_compare(MPI_Group group1, MPI_Group group2, int *result) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Group_compare(group1, group2, result);
}

#undef FCNAME
#define FCNAME MPIASP_Group_difference
int MPIASP_Group_difference(MPI_Group group1, MPI_Group group2,
        MPI_Group *newgroup) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Group_difference(group1, group2, newgroup);
}

#undef FCNAME
#define FCNAME MPIASP_Group_excl
int MPIASP_Group_excl(MPI_Group group, int n, int *ranks, MPI_Group *newgroup) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Group_excl(group, n, ranks, newgroup);
}

#undef FCNAME
#define FCNAME MPIASP_Group_free
int MPIASP_Group_free(MPI_Group *group) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Group_free(group);
}

#undef FCNAME
#define FCNAME MPIASP_Group_incl
int MPIASP_Group_incl(MPI_Group group, int n, int *ranks, MPI_Group *newgroup) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Group_incl(group, n, ranks, newgroup);
}

#undef FCNAME
#define FCNAME MPIASP_Group_intersection
int MPIASP_Group_intersection(MPI_Group group1, MPI_Group group2,
        MPI_Group *newgroup) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Group_intersection(group1, group2, newgroup);
}

#undef FCNAME
#define FCNAME MPIASP_Group_range_excl
int MPIASP_Group_range_excl(MPI_Group group, int n, int ranges[][3],
        MPI_Group *newgroup) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Group_range_excl(group, n, ranges, newgroup);
}

#undef FCNAME
#define FCNAME MPIASP_Group_range_incl
int MPIASP_Group_range_incl(MPI_Group group, int n, int ranges[][3],
        MPI_Group *newgroup) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Group_range_incl(group, n, ranges, newgroup);
}

#undef FCNAME
#define FCNAME MPIASP_Group_rank
int MPIASP_Group_rank(MPI_Group group, int *rank) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Group_rank(group, rank);
}

#undef FCNAME
#define FCNAME MPIASP_Group_size
int MPIASP_Group_size(MPI_Group group, int *size) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Group_size(group, size);
}

#undef FCNAME
#define FCNAME MPIASP_Group_translate_ranks
int MPIASP_Group_translate_ranks(MPI_Group group1, int n, int *ranks1,
        MPI_Group group2, int *ranks2) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Group_translate_ranks(group1, n, ranks1, group2, ranks2);
}

#undef FCNAME
#define FCNAME MPIASP_Group_union
int MPIASP_Group_union(MPI_Group group1, MPI_Group group2, MPI_Group *newgroup) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Group_union(group1, group2, newgroup);
}

#undef FCNAME
#define FCNAME MPIASP_Finalized
int MPIASP_Finalized(int *flag) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Finalized(flag);
}

#undef FCNAME
#define FCNAME MPIASP_Initialized
int MPIASP_Initialized(int *flag) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Initialized(flag);
}

#undef FCNAME
#define FCNAME MPIASP_Is_thread_main
int MPIASP_Is_thread_main(int *flag) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Is_thread_main(flag);
}

#undef FCNAME
#define FCNAME MPIASP_Query_thread
int MPIASP_Query_thread(int *provided) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Query_thread(provided);
}

#undef FCNAME
#define FCNAME MPIASP_Get_processor_name
int MPIASP_Get_processor_name(char *name, int *resultlen) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Get_processor_name(name, resultlen);
}

#undef FCNAME
#define FCNAME MPIASP_Pcontrol
int MPIASP_Pcontrol(const int level, ...) {
    int ret_val;
    va_list list;

    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;

    va_start(list, level);
    ret_val = PMPI_Pcontrol(level, list);
    va_end(list);
    return ret_val;
}

#undef FCNAME
#define FCNAME MPIASP_Get_version
int MPIASP_Get_version(int *version, int *subversion) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Get_version(version, subversion);
}

#undef FCNAME
#define FCNAME MPIASP_Bsend
int MPIASP_Bsend(void *buf, int count, MPI_Datatype datatype, int dest, int tag,
        MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Bsend(buf, count, datatype, dest, tag, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Bsend_init
int MPIASP_Bsend_init(void *buf, int count, MPI_Datatype datatype, int dest,
        int tag, MPI_Comm comm, MPI_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Bsend_init(buf, count, datatype, dest, tag, comm, request);
}

#undef FCNAME
#define FCNAME MPIASP_Buffer_attach
int MPIASP_Buffer_attach(void *buffer, int size) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Buffer_attach(buffer, size);
}

#undef FCNAME
#define FCNAME MPIASP_Buffer_detach
int MPIASP_Buffer_detach(void *buffer, int *size) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Buffer_detach(buffer, size);
}

#undef FCNAME
#define FCNAME MPIASP_Cancel
int MPIASP_Cancel(MPI_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Cancel(request);
}

#undef FCNAME
#define FCNAME MPIASP_Grequest_complete
int MPIASP_Grequest_complete(MPI_Request request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Grequest_complete(request);
}

#undef FCNAME
#define FCNAME MPIASP_Grequest_start
int MPIASP_Grequest_start(MPI_Grequest_query_function *query_fn,
        MPI_Grequest_free_function *free_fn,
        MPI_Grequest_cancel_function *cancel_fn, void *extra_state,
        MPI_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Grequest_start(query_fn, free_fn, cancel_fn, extra_state,
            request);
}

#undef FCNAME
#define FCNAME MPIASP_Ibsend
int MPIASP_Ibsend(void *buf, int count, MPI_Datatype datatype, int dest,
        int tag, MPI_Comm comm, MPI_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Ibsend(buf, count, datatype, dest, tag, comm, request);
}

#undef FCNAME
#define FCNAME MPIASP_Iprobe
int MPIASP_Iprobe(int source, int tag, MPI_Comm comm, int *flag,
        MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Iprobe(source, tag, comm, flag, status);
}

#undef FCNAME
#define FCNAME MPIASP_Irecv
int MPIASP_Irecv(void *buf, int count, MPI_Datatype datatype, int source,
        int tag, MPI_Comm comm, MPI_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Irecv(buf, count, datatype, source, tag, comm, request);
}

#undef FCNAME
#define FCNAME MPIASP_Irsend
int MPIASP_Irsend(void *buf, int count, MPI_Datatype datatype, int dest,
        int tag, MPI_Comm comm, MPI_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Irsend(buf, count, datatype, dest, tag, comm, request);
}

#undef FCNAME
#define FCNAME MPIASP_Isend
int MPIASP_Isend(void *buf, int count, MPI_Datatype datatype, int dest, int tag,
        MPI_Comm comm, MPI_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Isend(buf, count, datatype, dest, tag, comm, request);
}

#undef FCNAME
#define FCNAME MPIASP_Issend
int MPIASP_Issend(void *buf, int count, MPI_Datatype datatype, int dest,
        int tag, MPI_Comm comm, MPI_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Issend(buf, count, datatype, dest, tag, comm, request);
}

#undef FCNAME
#define FCNAME MPIASP_Probe
int MPIASP_Probe(int source, int tag, MPI_Comm comm, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Probe(source, tag, comm, status);
}

#undef FCNAME
#define FCNAME MPIASP_Recv
int MPIASP_Recv(void *buf, int count, MPI_Datatype datatype, int source,
        int tag, MPI_Comm comm, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Recv(buf, count, datatype, source, tag, comm, status);
}

#undef FCNAME
#define FCNAME MPIASP_Recv_init
int MPIASP_Recv_init(void *buf, int count, MPI_Datatype datatype, int source,
        int tag, MPI_Comm comm, MPI_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Recv_init(buf, count, datatype, source, tag, comm, request);
}

#undef FCNAME
#define FCNAME MPIASP_Request_free
int MPIASP_Request_free(MPI_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Request_free(request);
}

#undef FCNAME
#define FCNAME MPIASP_Request_get_status
int MPIASP_Request_get_status(MPI_Request request, int *flag,
        MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Request_get_status(request, flag, status);
}

#undef FCNAME
#define FCNAME MPIASP_Rsend
int MPIASP_Rsend(void *buf, int count, MPI_Datatype datatype, int dest, int tag,
        MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Rsend(buf, count, datatype, dest, tag, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Rsend_init
int MPIASP_Rsend_init(void *buf, int count, MPI_Datatype datatype, int dest,
        int tag, MPI_Comm comm, MPI_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Rsend_init(buf, count, datatype, dest, tag, comm, request);
}

#undef FCNAME
#define FCNAME MPIASP_Send
int MPIASP_Send(void *buf, int count, MPI_Datatype datatype, int dest, int tag,
        MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Send(buf, count, datatype, dest, tag, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Sendrecv
int MPIASP_Sendrecv(void *sendbuf, int sendcount, MPI_Datatype sendtype,
        int dest, int sendtag, void *recvbuf, int recvcount,
        MPI_Datatype recvtype, int source, int recvtag, MPI_Comm comm,
        MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Sendrecv(sendbuf, sendcount, sendtype, dest, sendtag, recvbuf,
            recvcount, recvtype, source, recvtag, comm, status);
}

#undef FCNAME
#define FCNAME MPIASP_Sendrecv_replace
int MPIASP_Sendrecv_replace(void *buf, int count, MPI_Datatype datatype,
        int dest, int sendtag, int source, int recvtag, MPI_Comm comm,
        MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Sendrecv_replace(buf, count, datatype, dest, sendtag, source,
            recvtag, comm, status);
}

#undef FCNAME
#define FCNAME MPIASP_Send_init
int MPIASP_Send_init(void *buf, int count, MPI_Datatype datatype, int dest,
        int tag, MPI_Comm comm, MPI_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Send_init(buf, count, datatype, dest, tag, comm, request);
}

#undef FCNAME
#define FCNAME MPIASP_Ssend
int MPIASP_Ssend(void *buf, int count, MPI_Datatype datatype, int dest, int tag,
        MPI_Comm comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Ssend(buf, count, datatype, dest, tag, comm);
}

#undef FCNAME
#define FCNAME MPIASP_Ssend_init
int MPIASP_Ssend_init(void *buf, int count, MPI_Datatype datatype, int dest,
        int tag, MPI_Comm comm, MPI_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Ssend_init(buf, count, datatype, dest, tag, comm, request);
}

#undef FCNAME
#define FCNAME MPIASP_Start
int MPIASP_Start(MPI_Request *request) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Start(request);
}

#undef FCNAME
#define FCNAME MPIASP_Startall
int MPIASP_Startall(int count, MPI_Request array_of_requests[]) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Startall(count, array_of_requests);
}

#undef FCNAME
#define FCNAME MPIASP_Status_set_cancelled
int MPIASP_Status_set_cancelled(MPI_Status *status, int flag) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Status_set_cancelled(status, flag);
}

#undef FCNAME
#define FCNAME MPIASP_Test
int MPIASP_Test(MPI_Request *request, int *flag, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Test(request, flag, status);
}

#undef FCNAME
#define FCNAME MPIASP_Testall
int MPIASP_Testall(int count, MPI_Request array_of_requests[], int *flag,
        MPI_Status array_of_statuses[]) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Testall(count, array_of_requests, flag, array_of_statuses);
}

#undef FCNAME
#define FCNAME MPIASP_Testany
int MPIASP_Testany(int count, MPI_Request array_of_requests[], int *index,
        int *flag, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Testany(count, array_of_requests, index, flag, status);
}

#undef FCNAME
#define FCNAME MPIASP_Testsome
int MPIASP_Testsome(int incount, MPI_Request array_of_requests[], int *outcount,
        int array_of_indices[], MPI_Status array_of_statuses[]) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Testsome(incount, array_of_requests, outcount, array_of_indices,
            array_of_statuses);
}

#undef FCNAME
#define FCNAME MPIASP_Test_cancelled
int MPIASP_Test_cancelled(MPI_Status *status, int *flag) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Test_cancelled(status, flag);
}

#undef FCNAME
#define FCNAME MPIASP_Wait
int MPIASP_Wait(MPI_Request *request, MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Wait(request, status);
}

#undef FCNAME
#define FCNAME MPIASP_Waitall
int MPIASP_Waitall(int count, MPI_Request array_of_requests[],
        MPI_Status array_of_statuses[]) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Waitall(count, array_of_requests, array_of_statuses);
}

#undef FCNAME
#define FCNAME MPIASP_Waitany
int MPIASP_Waitany(int count, MPI_Request array_of_requests[], int *index,
        MPI_Status *status) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Waitany(count, array_of_requests, index, status);
}

#undef FCNAME
#define FCNAME MPIASP_Waitsome
int MPIASP_Waitsome(int incount, MPI_Request array_of_requests[], int *outcount,
        int array_of_indices[], MPI_Status array_of_statuses[]) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Waitsome(incount, array_of_requests, outcount, array_of_indices,
            array_of_statuses);
}

#undef FCNAME
#define FCNAME MPIASP_Accumulate
int MPIASP_Accumulate(void *origin_addr, int origin_count,
        MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp,
        int target_count, MPI_Datatype target_datatype, MPI_Op op, MPI_Win win) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Accumulate(origin_addr, origin_count, origin_datatype,
            target_rank, target_disp, target_count, target_datatype, op, win);
}

#undef FCNAME
#define FCNAME MPIASP_Alloc_mem
int MPIASP_Alloc_mem(MPI_Aint size, MPI_Info info, void *baseptr) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Alloc_mem(size, info, baseptr);
}

#undef FCNAME
#define FCNAME MPIASP_Free_mem
int MPIASP_Free_mem(void *base) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Free_mem(base);
}

#undef FCNAME
#define FCNAME MPIASP_Get
int MPIASP_Get(void *origin_addr, int origin_count,
        MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp,
        int target_count, MPI_Datatype target_datatype, MPI_Win win) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Get(origin_addr, origin_count, origin_datatype, target_rank,
            target_disp, target_count, target_datatype, win);
}

#undef FCNAME
#define FCNAME MPIASP_Put
int MPIASP_Put(void *origin_addr, int origin_count,
        MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp,
        int target_count, MPI_Datatype target_datatype, MPI_Win win) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Put(origin_addr, origin_count, origin_datatype, target_rank,
            target_disp, target_count, target_datatype, win);
}

#undef FCNAME
#define FCNAME MPIASP_Win_complete
int MPIASP_Win_complete(MPI_Win win) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_complete(win);
}

#undef FCNAME
#define FCNAME MPIASP_Win_fence
int MPIASP_Win_fence(int assert, MPI_Win win) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_fence(assert, win);
}

#undef FCNAME
#define FCNAME MPIASP_Win_free
int MPIASP_Win_free(MPI_Win *win) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_free(win);
}

#undef FCNAME
#define FCNAME MPIASP_Win_get_group
int MPIASP_Win_get_group(MPI_Win win, MPI_Group *group) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_get_group(win, group);
}

#undef FCNAME
#define FCNAME MPIASP_Win_get_name
int MPIASP_Win_get_name(MPI_Win win, char *win_name, int *resultlen) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_get_name(win, win_name, resultlen);
}

#undef FCNAME
#define FCNAME MPIASP_Win_lock
int MPIASP_Win_lock(int lock_type, int rank, int assert, MPI_Win win) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_lock(lock_type, rank, assert, win);
}

#undef FCNAME
#define FCNAME MPIASP_Win_post
int MPIASP_Win_post(MPI_Group group, int assert, MPI_Win win) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_post(group, assert, win);
}

#undef FCNAME
#define FCNAME MPIASP_Win_set_name
int MPIASP_Win_set_name(MPI_Win win, char *win_name) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_set_name(win, win_name);
}

#undef FCNAME
#define FCNAME MPIASP_Win_start
int MPIASP_Win_start(MPI_Group group, int assert, MPI_Win win) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_start(group, assert, win);
}

#undef FCNAME
#define FCNAME MPIASP_Win_test
int MPIASP_Win_test(MPI_Win win, int *flag) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_test(win, flag);
}

#undef FCNAME
#define FCNAME MPIASP_Win_unlock
int MPIASP_Win_unlock(int rank, MPI_Win win) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_unlock(rank, win);
}

#undef FCNAME
#define FCNAME MPIASP_Win_wait
int MPIASP_Win_wait(MPI_Win win) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Win_wait(win);
}

#undef FCNAME
#define FCNAME MPIASP_Info_create
int MPIASP_Info_create(MPI_Info *info) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Info_create(info);
}

#undef FCNAME
#define FCNAME MPIASP_Info_delete
int MPIASP_Info_delete(MPI_Info info, char *key) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Info_delete(info, key);
}

#undef FCNAME
#define FCNAME MPIASP_Info_dup
int MPIASP_Info_dup(MPI_Info info, MPI_Info *newinfo) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Info_dup(info, newinfo);
}

#undef FCNAME
#define FCNAME MPIASP_Info_free
int MPIASP_Info_free(MPI_Info *info) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Info_free(info);
}

#undef FCNAME
#define FCNAME MPIASP_Info_get
int MPIASP_Info_get(MPI_Info info, char *key, int valuelen, char *value,
        int *flag) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Info_get(info, key, valuelen, value, flag);
}

#undef FCNAME
#define FCNAME MPIASP_Info_get_nkeys
int MPIASP_Info_get_nkeys(MPI_Info info, int *nkeys) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Info_get_nkeys(info, nkeys);
}

#undef FCNAME
#define FCNAME MPIASP_Info_get_nthkey
int MPIASP_Info_get_nthkey(MPI_Info info, int n, char *key) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Info_get_nthkey(info, n, key);
}

#undef FCNAME
#define FCNAME MPIASP_Info_get_valuelen
int MPIASP_Info_get_valuelen(MPI_Info info, char *key, int *valuelen, int *flag) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Info_get_valuelen(info, key, valuelen, flag);
}

#undef FCNAME
#define FCNAME MPIASP_Info_set
int MPIASP_Info_set(MPI_Info info, char *key, char *value) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Info_set(info, key, value);
}

#undef FCNAME
#define FCNAME MPIASP_Close_port
int MPIASP_Close_port(char *port_name) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Close_port(port_name);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_accept
int MPIASP_Comm_accept(char *port_name, MPI_Info info, int root, MPI_Comm comm,
        MPI_Comm *newcomm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_accept(port_name, info, root, comm, newcomm);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_connect
int MPIASP_Comm_connect(char *port_name, MPI_Info info, int root, MPI_Comm comm,
        MPI_Comm *newcomm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_connect(port_name, info, root, comm, newcomm);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_disconnect
int MPIASP_Comm_disconnect(MPI_Comm * comm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_disconnect(comm);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_get_parent
int MPIASP_Comm_get_parent(MPI_Comm *parent) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_get_parent(parent);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_join
int MPIASP_Comm_join(int fd, MPI_Comm *intercomm) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_join(fd, intercomm);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_spawn
int MPIASP_Comm_spawn(char *command, char *argv[], int maxprocs, MPI_Info info,
        int root, MPI_Comm comm, MPI_Comm *intercomm, int array_of_errcodes[]) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_spawn(command, argv, maxprocs, info, root, comm, intercomm,
            array_of_errcodes);
}

#undef FCNAME
#define FCNAME MPIASP_Comm_spawn_multiple
int MPIASP_Comm_spawn_multiple(int count, char *array_of_commands[],
        char* *array_of_argv[], int array_of_maxprocs[],
        MPI_Info array_of_info[], int root, MPI_Comm comm, MPI_Comm *intercomm,
        int array_of_errcodes[]) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Comm_spawn_multiple(count, array_of_commands, array_of_argv,
            array_of_maxprocs, array_of_info, root, comm, intercomm,
            array_of_errcodes);
}

#undef FCNAME
#define FCNAME MPIASP_Lookup_name
int MPIASP_Lookup_name(char *service_name, MPI_Info info, char *port_name) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Lookup_name(service_name, info, port_name);
}

#undef FCNAME
#define FCNAME MPIASP_Open_port
int MPIASP_Open_port(MPI_Info info, char *port_name) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Open_port(info, port_name);
}

#undef FCNAME
#define FCNAME MPIASP_Publish_name
int MPIASP_Publish_name(char *service_name, MPI_Info info, char *port_name) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Publish_name(service_name, info, port_name);
}

#undef FCNAME
#define FCNAME MPIASP_Unpublish_name
int MPIASP_Unpublish_name(char *service_name, MPI_Info info, char *port_name) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Unpublish_name(service_name, info, port_name);
}

#undef FCNAME
#define FCNAME MPIASP_Cartdim_get
int MPIASP_Cartdim_get(MPI_Comm comm, int *ndims) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Cartdim_get(comm, ndims);
}

#undef FCNAME
#define FCNAME MPIASP_Cart_coords
int MPIASP_Cart_coords(MPI_Comm comm, int rank, int maxdims, int *coords) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Cart_coords(comm, rank, maxdims, coords);
}

#undef FCNAME
#define FCNAME MPIASP_Cart_create
int MPIASP_Cart_create(MPI_Comm comm_old, int ndims, int *dims, int *periods,
        int reorder, MPI_Comm *comm_cart) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Cart_create(comm_old, ndims, dims, periods, reorder, comm_cart);
}

#undef FCNAME
#define FCNAME MPIASP_Cart_get
int MPIASP_Cart_get(MPI_Comm comm, int maxdims, int *dims, int *periods,
        int *coords) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Cart_get(comm, maxdims, dims, periods, coords);
}

#undef FCNAME
#define FCNAME MPIASP_Cart_map
int MPIASP_Cart_map(MPI_Comm comm_old, int ndims, int *dims, int *periods,
        int *newrank) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Cart_map(comm_old, ndims, dims, periods, newrank);
}

#undef FCNAME
#define FCNAME MPIASP_Cart_rank
int MPIASP_Cart_rank(MPI_Comm comm, int *coords, int *rank) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Cart_rank(comm, coords, rank);
}

#undef FCNAME
#define FCNAME MPIASP_Cart_shift
int MPIASP_Cart_shift(MPI_Comm comm, int direction, int displ, int *source,
        int *dest) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Cart_shift(comm, direction, displ, source, dest);
}

#undef FCNAME
#define FCNAME MPIASP_Cart_sub
int MPIASP_Cart_sub(MPI_Comm comm, int *remain_dims, MPI_Comm *comm_new) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Cart_sub(comm, remain_dims, comm_new);
}

#undef FCNAME
#define FCNAME MPIASP_Dims_create
int MPIASP_Dims_create(int nnodes, int ndims, int *dims) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Dims_create(nnodes, ndims, dims);
}

#undef FCNAME
#define FCNAME MPIASP_Graph_create
int MPIASP_Graph_create(MPI_Comm comm_old, int nnodes, int *index, int *edges,
        int reorder, MPI_Comm *comm_graph) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Graph_create(comm_old, nnodes, index, edges, reorder,
            comm_graph);
}

#undef FCNAME
#define FCNAME MPIASP_Dist_graph_create
int MPIASP_Dist_graph_create(MPI_Comm comm_old, int n, int sources[],
        int degrees[], int destinations[], int weights[], MPI_Info info,
        int reorder, MPI_Comm *comm_dist_graph) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Dist_graph_create(comm_old, n, sources, degrees, destinations,
            weights, info, reorder, comm_dist_graph);
}

#undef FCNAME
#define FCNAME MPIASP_Dist_graph_create_adjacent
int MPIASP_Dist_graph_create_adjacent(MPI_Comm comm_old, int indegree,
        int sources[], int sourceweights[], int outdegree, int destinations[],
        int destweights[], MPI_Info info, int reorder,
        MPI_Comm *comm_dist_graph) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Dist_graph_create_adjacent(comm_old, indegree, sources,
            sourceweights, outdegree, destinations, destweights, info, reorder,
            comm_dist_graph);
}

#undef FCNAME
#define FCNAME MPIASP_Graphdims_get
int MPIASP_Graphdims_get(MPI_Comm comm, int *nnodes, int *nedges) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Graphdims_get(comm, nnodes, nedges);
}

#undef FCNAME
#define FCNAME MPIASP_Graph_neighbors_count
int MPIASP_Graph_neighbors_count(MPI_Comm comm, int rank, int *nneighbors) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Graph_neighbors_count(comm, rank, nneighbors);
}

#undef FCNAME
#define FCNAME MPIASP_Graph_get
int MPIASP_Graph_get(MPI_Comm comm, int maxindex, int maxedges, int *index,
        int *edges) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Graph_get(comm, maxindex, maxedges, index, edges);
}

#undef FCNAME
#define FCNAME MPIASP_Graph_map
int MPIASP_Graph_map(MPI_Comm comm_old, int nnodes, int *index, int *edges,
        int *newrank) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Graph_map(comm_old, nnodes, index, edges, newrank);
}

#undef FCNAME
#define FCNAME MPIASP_Graph_neighbors
int MPIASP_Graph_neighbors(MPI_Comm comm, int rank, int maxneighbors,
        int *neighbors) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Graph_neighbors(comm, rank, maxneighbors, neighbors);
}

#undef FCNAME
#define FCNAME MPIASP_Dist_graph_neighbors
int MPIASP_Dist_graph_neighbors(MPI_Comm comm, int maxindegree, int sources[],
        int sourceweights[], int maxoutdegree, int destinations[],
        int destweights[]) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Dist_graph_neighbors(comm, maxindegree, sources, sourceweights,
            maxoutdegree, destinations, destweights);
}

#undef FCNAME
#define FCNAME MPIASP_Dist_graph_neighbors_count
int MPIASP_Dist_graph_neighbors_count(MPI_Comm comm, int *indegree,
        int *outdegree, int *weighted) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Dist_graph_neighbors_count(comm, indegree, outdegree, weighted);
}

#undef FCNAME
#define FCNAME MPIASP_Topo_test
int MPIASP_Topo_test(MPI_Comm comm, int *topo_type) {
    MPIASP_DBG_PRINT_FCNAME();
    if (MPIASP_Comm_rank_isasp())
        return MPI_SUCCESS;
    return PMPI_Topo_test(comm, topo_type);
}

#undef FCNAME
#define FCNAME MPIASP_Wtime
double MPIASP_Wtime() {
    /*MPIASP_DBG_PRINT_FCNAME_VOID(FCNAME);*//* No checking for performance */
    return PMPI_Wtime();
}

#undef FCNAME
#define FCNAME MPIASP_Wtick
double MPIASP_Wtick() {
    /*MPIASP_DBG_PRINT_FCNAME_VOID(FCNAME);*//* No checking for performance */
    return PMPI_Wtick();
}

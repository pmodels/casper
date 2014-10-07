#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

noinst_PROGRAMS = \
	lockall_overhead	\
	mtcore_lockall_overhead \
	mtcore_lockall_overhead_no_loadstore \
	lockall_overhead_async \
	mtcore_lockall_overhead_async	\
	async_2np	\
	mtcore_async_2np	\
	async_all2all	\
	mtcore_async_all2all	\
	lock_overhead	\
	mtcore_lock_overhead \
	lock_self_overhead	\
	mtcore_lock_self_overhead \
	mtcore_lock_self_overhead_no_loadstore \
	mtcore_lock_self_overhead_no_check	\
	op_overhead \
	mtcore_op_overhead \
	static_load_async \
	mtcore_static_load_async \
	static_load_nop \
	mtcore_static_load_nop \
	static_load_nseg \
	mtcore_static_load_nseg \
	runtime_load_nop \
	mtcore_runtime_load_nop \
	runtime_load_opsize \
	mtcore_runtime_load_opsize \
	runtime_load_nop_acc \
	mtcore_runtime_load_nop_acc \
	async_fence	\
	async_fence_th	\
	mtcore_async_fence \
	async_pscw	\
	mtcore_async_pscw	\
	win_alloc_overhead

lockall_overhead_CFLAGS= -O2
mtcore_lockall_overhead_SOURCES= lockall_overhead.c
mtcore_lockall_overhead_LDFLAGS= -L$(libdir) -lmtcore -lmpich
mtcore_lockall_overhead_CFLAGS= -O2 -DMTCORE

mtcore_lockall_overhead_no_loadstore_SOURCES= lockall_overhead_no_loadstore.c
mtcore_lockall_overhead_no_loadstore_LDFLAGS= -L$(libdir) -lmtcore
mtcore_lockall_overhead_no_loadstore_CFLAGS= -O2 -DMTCORE

op_overhead_CFLAGS= -O2
mtcore_op_overhead_SOURCES= op_overhead.c
mtcore_op_overhead_LDFLAGS= -L$(libdir) -lmtcore -lmpich
mtcore_op_overhead_CFLAGS= -O2 -DMTCORE

async_fence_CFLAGS= -O2
async_fence_th_CFLAGS= -O2 -lpthread
mtcore_async_fence_SOURCES= async_fence.c
mtcore_async_fence_LDFLAGS= -L$(libdir) -lmtcore -lmpich
mtcore_async_fence_CFLAGS= -O2 -DMTCORE

async_pscw_CFLAGS= -O2
mtcore_async_pscw_SOURCES= async_pscw.c
mtcore_async_pscw_LDFLAGS= -L$(libdir) -lmtcore -lmpich
mtcore_async_pscw_CFLAGS= -O2 -DMTCORE

lock_overhead_CFLAGS= -O2
mtcore_lock_overhead_SOURCES= lock_overhead.c
mtcore_lock_overhead_LDFLAGS= -L$(libdir) -lmtcore
mtcore_lock_overhead_CFLAGS= -O2 -DMTCORE

lockall_overhead_async_CFLAGS= -O2
mtcore_lockall_overhead_async_SOURCES= lockall_overhead_async.c
mtcore_lockall_overhead_async_LDFLAGS= -L$(libdir) -lmtcore
mtcore_lockall_overhead_async_CFLAGS= -O2 -DMTCORE

static_load_async_CFLAGS= -O2
mtcore_static_load_async_SOURCES= static_load_async.c
mtcore_static_load_async_LDFLAGS= -L$(libdir) -lmtcore
mtcore_static_load_async_CFLAGS= -O2 -DMTCORE

static_load_nop_CFLAGS= -O2
mtcore_static_load_nop_SOURCES= static_load_nop.c
mtcore_static_load_nop_LDFLAGS= -L$(libdir) -lmtcore
mtcore_static_load_nop_CFLAGS= -O2 -DMTCORE

static_load_nseg_CFLAGS= -O2
mtcore_static_load_nseg_SOURCES= static_load_nseg.c
mtcore_static_load_nseg_LDFLAGS= -L$(libdir) -lmtcore
mtcore_static_load_nseg_CFLAGS= -O2 -DMTCORE

runtime_load_nop_CFLAGS= -O2
mtcore_runtime_load_nop_SOURCES= runtime_load_nop.c
mtcore_runtime_load_nop_LDFLAGS= -L$(libdir) -lmtcore
mtcore_runtime_load_nop_CFLAGS= -O2 -DMTCORE

runtime_load_nop_acc_CFLAGS= -O2
mtcore_runtime_load_nop_acc_SOURCES= runtime_load_nop_acc.c
mtcore_runtime_load_nop_acc_LDFLAGS= -L$(libdir) -lmtcore
mtcore_runtime_load_nop_acc_CFLAGS= -O2 -DMTCORE

runtime_load_opsize_CFLAGS= -O2
mtcore_runtime_load_opsize_SOURCES= runtime_load_opsize.c
mtcore_runtime_load_opsize_LDFLAGS= -L$(libdir) -lmtcore
mtcore_runtime_load_opsize_CFLAGS= -O2 -DMTCORE

async_2np_CFLAGS= -O2
mtcore_async_2np_SOURCES= async_2np.c
mtcore_async_2np_LDFLAGS= -L$(libdir) -lmtcore
mtcore_async_2np_CFLAGS= -O2 -DMTCORE

async_all2all_CFLAGS= -O2
mtcore_async_all2all_SOURCES= async_all2all.c
mtcore_async_all2all_LDFLAGS= -L$(libdir) -lmtcore
mtcore_async_all2all_CFLAGS= -O2 -DMTCORE

lock_self_overhead_CFLAGS= -O2
mtcore_lock_self_overhead_SOURCES= lock_self_overhead.c
mtcore_lock_self_overhead_LDFLAGS= -L$(libdir) -lmtcore
mtcore_lock_self_overhead_CFLAGS= -O2 -DMTCORE

mtcore_lock_self_overhead_no_loadstore_SOURCES= lock_self_overhead_no_loadstore.c
mtcore_lock_self_overhead_no_loadstore_LDFLAGS= -L$(libdir) -lmtcore
mtcore_lock_self_overhead_no_loadstore_CFLAGS= -O2 -DMTCORE

mtcore_lock_self_overhead_no_check_SOURCES= lock_self_overhead_no_check.c
mtcore_lock_self_overhead_no_check_LDFLAGS= -L$(libdir) -lmtcore
mtcore_lock_self_overhead_no_check_CFLAGS= -O2 -DMTCORE

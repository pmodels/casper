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
	lock_self_overhead	\
	mtcore_lock_self_overhead \
	mtcore_lock_self_overhead_no_loadstore \
	mtcore_lock_self_overhead_no_check

#	lock_overhead	\
#	mtcore_lock_overhead \
#	lock_overhead_inter	\
#	mtcore_lock_overhead_inter	\
#	lock_overhead_intra	\
#	mtcore_lock_overhead_intra	\
#	lock_overhead_intra_wincreate	\
#	balance_put	\
#	mtcore_balance_put \
#	balance_put1	\
#	mtcore_balance_put1 \
#	balance_put2	\
#	mtcore_balance_put2 \
#	mtcore_balance_put2_no_loadstore \
#	balance_acc2	\
#	mtcore_balance_acc2
	
lockall_overhead_CFLAGS= -O2
mtcore_lockall_overhead_SOURCES= lockall_overhead.c
mtcore_lockall_overhead_LDFLAGS= -L$(libdir) -lmtcore -lmpich
mtcore_lockall_overhead_CFLAGS= -O2 -DMTCORE

mtcore_lockall_overhead_no_loadstore_SOURCES= lockall_overhead_no_loadstore.c
mtcore_lockall_overhead_no_loadstore_LDFLAGS= -L$(libdir) -lmtcore
mtcore_lockall_overhead_no_loadstore_CFLAGS= -O2 -DMTCORE


#lock_overhead_CFLAGS= -O2
#mtcore_lock_overhead_SOURCES= lock_overhead.c
#mtcore_lock_overhead_LDFLAGS= -L$(libdir) -lmtcore
#mtcore_lock_overhead_CFLAGS= -O2 -DMTCORE

lockall_overhead_async_CFLAGS= -O2
mtcore_lockall_overhead_async_SOURCES= lockall_overhead_async.c
mtcore_lockall_overhead_async_LDFLAGS= -L$(libdir) -lmtcore
mtcore_lockall_overhead_async_CFLAGS= -O2 -DMTCORE

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

#mtcore_balance_put_SOURCES= balance_put.c
#mtcore_balance_put_LDFLAGS= -L$(libdir) -lmtcore
#mtcore_balance_put_CFLAGS= -O2 -DMTCORE
#
#mtcore_balance_put1_SOURCES= balance_put1.c
#mtcore_balance_put1_LDFLAGS= -L$(libdir) -lmtcore
#mtcore_balance_put1_CFLAGS= -O2 -DMTCORE
#
#mtcore_balance_put2_SOURCES= balance_put2.c
#mtcore_balance_put2_LDFLAGS= -L$(libdir) -lmtcore
#mtcore_balance_put2_CFLAGS= -O2 -DMTCORE
#
#mtcore_balance_put2_no_loadstore_SOURCES= balance_put2_no_loadstore.c
#mtcore_balance_put2_no_loadstore_LDFLAGS= -L$(libdir) -lmtcore
#mtcore_balance_put2_no_loadstore_CFLAGS= -O2 -DMTCORE
#
#mtcore_balance_acc2_SOURCES= balance_acc2.c
#mtcore_balance_acc2_LDFLAGS= -L$(libdir) -lmtcore
#mtcore_balance_acc2_CFLAGS= -O2 -DMTCORE
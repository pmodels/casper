#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

noinst_PROGRAMS = \
	lockall_overhead	\
	mtcore_lockall_overhead \
	mtcore_lockall_overhead_no_loadstore \
	lock_overhead	\
	mtcore_lock_overhead \
	lock_self_overhead	\
	lock_overhead_inter	\
	mtcore_lock_overhead_inter	\
	lock_overhead_intra	\
	mtcore_lock_overhead_intra	\
	lock_overhead_intra_wincreate	\
	mtcore_lock_self_overhead \
	mtcore_lock_self_overhead_no_loadstore \
	balance_put	\
	mtcore_balance_put \
	balance_put1	\
	mtcore_balance_put1 \
	balance_put2	\
	mtcore_balance_put2 \
	mtcore_balance_put2_no_loadstore \
	balance_acc2	\
	mtcore_balance_acc2
	
lockall_overhead_CFLAGS= -O2
mtcore_lockall_overhead_SOURCES= lockall_overhead.c
mtcore_lockall_overhead_LDFLAGS= -L$(libdir) -lmtcore -lmpich
mtcore_lockall_overhead_CFLAGS= -O2 -DMTCORE

mtcore_lockall_overhead_no_loadstore_SOURCES= lockall_overhead_no_loadstore.c
mtcore_lockall_overhead_no_loadstore_LDFLAGS= -L$(libdir) -lmtcore
mtcore_lockall_overhead_no_loadstore_CFLAGS= -O2 -DMTCORE

lock_overhead_CFLAGS= -O2
mtcore_lock_overhead_SOURCES= lock_overhead.c
mtcore_lock_overhead_LDFLAGS= -L$(libdir) -lmtcore
mtcore_lock_overhead_CFLAGS= -O2 -DMTCORE

lock_overhead_inter_CFLAGS= -O2
mtcore_lock_overhead_inter_SOURCES= lock_overhead_inter.c
mtcore_lock_overhead_inter_LDFLAGS= -L$(libdir) -lmtcore
mtcore_lock_overhead_inter_CFLAGS= -O2 -DMTCORE

lock_overhead_intra_CFLAGS= -O2
mtcore_lock_overhead_intra_SOURCES= lock_overhead_intra.c
mtcore_lock_overhead_intra_LDFLAGS= -L$(libdir) -lmtcore
mtcore_lock_overhead_intra_CFLAGS= -O2 -DMTCORE

lock_overhead_intra_wincreate_CFLAGS= -O2

lock_self_overhead_CFLAGS= -O2
mtcore_lock_self_overhead_SOURCES= lock_self_overhead.c
mtcore_lock_self_overhead_LDFLAGS= -L$(libdir) -lmtcore
mtcore_lock_self_overhead_CFLAGS= -O2 -DMTCORE

mtcore_lock_self_overhead_no_loadstore_SOURCES= lock_self_overhead_no_loadstore.c
mtcore_lock_self_overhead_no_loadstore_LDFLAGS= -L$(libdir) -lmtcore
mtcore_lock_self_overhead_no_loadstore_CFLAGS= -O2 -DMTCORE

mtcore_balance_put_SOURCES= balance_put.c
mtcore_balance_put_LDFLAGS= -L$(libdir) -lmtcore
mtcore_balance_put_CFLAGS= -O2 -DMTCORE

mtcore_balance_put1_SOURCES= balance_put1.c
mtcore_balance_put1_LDFLAGS= -L$(libdir) -lmtcore
mtcore_balance_put1_CFLAGS= -O2 -DMTCORE

mtcore_balance_put2_SOURCES= balance_put2.c
mtcore_balance_put2_LDFLAGS= -L$(libdir) -lmtcore
mtcore_balance_put2_CFLAGS= -O2 -DMTCORE

mtcore_balance_put2_no_loadstore_SOURCES= balance_put2_no_loadstore.c
mtcore_balance_put2_no_loadstore_LDFLAGS= -L$(libdir) -lmtcore
mtcore_balance_put2_no_loadstore_CFLAGS= -O2 -DMTCORE

mtcore_balance_acc2_SOURCES= balance_acc2.c
mtcore_balance_acc2_LDFLAGS= -L$(libdir) -lmtcore
mtcore_balance_acc2_CFLAGS= -O2 -DMTCORE
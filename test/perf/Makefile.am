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
	mtcore_lock_self_overhead \
	mtcore_lock_self_overhead_no_loadstore
	
lockall_overhead_CFLAGS= -O2
mtcore_lockall_overhead_SOURCES= lockall_overhead.c
mtcore_lockall_overhead_LDFLAGS= -L$(libdir) -lmtcore 
mtcore_lockall_overhead_CFLAGS= -O2 -DMTCORE

mtcore_lockall_overhead_no_loadstore_SOURCES= lockall_overhead_no_loadstore.c
mtcore_lockall_overhead_no_loadstore_LDFLAGS= -L$(libdir) -lmtcore
mtcore_lockall_overhead_no_loadstore_CFLAGS= -O2 -DMTCORE

lock_overhead_CFLAGS= -O2
mtcore_lock_overhead_SOURCES= lock_overhead.c
mtcore_lock_overhead_LDFLAGS= -L$(libdir) -lmtcore
mtcore_lock_overhead_CFLAGS= -O2 -DMTCORE

lock_self_overhead_CFLAGS= -O2
mtcore_lock_self_overhead_SOURCES= lock_self_overhead.c
mtcore_lock_self_overhead_LDFLAGS= -L$(libdir) -lmtcore
mtcore_lock_self_overhead_CFLAGS= -O2 -DMTCORE

mtcore_lock_self_overhead_no_loadstore_SOURCES= lock_self_overhead_no_loadstore.c
mtcore_lock_self_overhead_no_loadstore_LDFLAGS= -L$(libdir) -lmtcore
mtcore_lock_self_overhead_no_loadstore_CFLAGS= -O2 -DMTCORE

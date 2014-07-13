#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

noinst_PROGRAMS = \
	lockall_overhead	\
	mtcore_lockall_overhead \
	mtcore_lockall_overhead_no_loadstore
	
mtcore_lockall_overhead_SOURCES= lockall_overhead.c
mtcore_lockall_overhead_LDFLAGS= -L$(libdir) -lasp 
mtcore_lockall_overhead_CFLAGS= -O2 -DASP
mtcore_lockall_overhead_no_loadstore_SOURCES= lockall_overhead_no_loadstore.c
mtcore_lockall_overhead_no_loadstore_LDFLAGS= -L$(libdir) -lasp 
mtcore_lockall_overhead_no_loadstore_CFLAGS= -O2 -DASP
lockall_overhead_CFLAGS= -O2
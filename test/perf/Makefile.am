#
# Copyright (C) 2014. See COPYRIGHT in top-level directory.
#

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(includedir)

noinst_HEADERS = $(top_srcdir)/include/ctest.h

noinst_PROGRAMS = \
	lockall_overhead	\
	csp_lockall_overhead \
	csp_lockall_overhead_no_loadstore \
	lockall_overhead_async \
	csp_lockall_overhead_async	\
	async_2np	\
	csp_async_2np	\
	async_all2all	\
	csp_async_all2all	\
	lock_overhead	\
	csp_lock_overhead \
	lock_self_overhead	\
	csp_lock_self_overhead \
	csp_lock_self_overhead_no_loadstore \
	csp_lock_self_overhead_no_check	\
	op_overhead \
	csp_op_overhead \
	static_load_async \
	csp_static_load_async \
	static_load_nop \
	csp_static_load_nop \
	runtime_load_nop \
	csp_runtime_load_nop \
	runtime_load_opsize \
	csp_runtime_load_opsize \
	runtime_load_nop_acc \
	csp_runtime_load_nop_acc \
	runtime_load_opsize_acc \
	csp_runtime_load_opsize_acc \
	async_fence	\
	async_fence_th	\
	csp_async_fence \
	async_pscw	\
	csp_async_pscw	\
	win_alloc_overhead
#	dmapp_async_2np \
#	dmapp_async_all2all \
#	dmapp_async_fence	\
#	dmapp_async_pscw


#dmapp_async_2np_CFLAGS= -O2
#dmapp_async_2np_SOURCES = async_2np.c
#dmapp_async_2np_LDFLAGS = $(CRAY_DMAPP_POST_LINK_OPTS) -ldmapp

#dmapp_async_all2all_CFLAGS= -O2
#dmapp_async_all2all_SOURCES = async_all2all.c
#dmapp_async_all2all_LDFLAGS = $(CRAY_DMAPP_POST_LINK_OPTS) -ldmapp

#dmapp_async_fence_CFLAGS= -O2
#dmapp_async_fence_SOURCES = async_fence.c
#dmapp_async_fence_LDFLAGS = $(CRAY_DMAPP_POST_LINK_OPTS) -ldmapp

#dmapp_async_pscw_CFLAGS= -O2
#dmapp_async_pscw_SOURCES = async_pscw.c
#dmapp_async_pscw_LDFLAGS = $(CRAY_DMAPP_POST_LINK_OPTS) -ldmapp

runtime_load_opsize_acc_CFLAGS= -O2
csp_runtime_load_opsize_acc_SOURCES= runtime_load_opsize_acc.c
csp_runtime_load_opsize_acc_LDADD= -L$(libdir) -lcasper
csp_runtime_load_opsize_acc_LDFLAGS= -Wl,-rpath -Wl,$(libdir)
csp_runtime_load_opsize_acc_CFLAGS= -O2 -DENABLE_CSP

lockall_overhead_CFLAGS= -O2
csp_lockall_overhead_SOURCES= lockall_overhead.c
csp_lockall_overhead_LDADD= -L$(libdir) -lcasper
csp_lockall_overhead_LDFLAGS= -Wl,-rpath -Wl,$(libdir)
csp_lockall_overhead_CFLAGS= -O2 -DENABLE_CSP

csp_lockall_overhead_no_loadstore_SOURCES= lockall_overhead_no_loadstore.c
csp_lockall_overhead_no_loadstore_LDADD= -L$(libdir) -lcasper
csp_lockall_overhead_no_loadstore_LDFLAGS= -Wl,-rpath -Wl,$(libdir)
csp_lockall_overhead_no_loadstore_CFLAGS= -O2 -DENABLE_CSP

op_overhead_CFLAGS= -O2
csp_op_overhead_SOURCES= op_overhead.c
csp_op_overhead_LDADD= -L$(libdir) -lcasper
csp_op_overhead_LDFLAGS= -Wl,-rpath -Wl,$(libdir)
csp_op_overhead_CFLAGS= -O2 -DENABLE_CSP

async_fence_CFLAGS= -O2
async_fence_th_LDADD= -lpthread
csp_async_fence_SOURCES= async_fence.c
csp_async_fence_LDADD= -L$(libdir) -lcasper
csp_async_fence_LDFLAGS= -Wl,-rpath -Wl,$(libdir)
csp_async_fence_CFLAGS= -O2 -DENABLE_CSP

async_pscw_CFLAGS= -O2
csp_async_pscw_SOURCES= async_pscw.c
csp_async_pscw_LDADD= -L$(libdir) -lcasper
csp_async_pscw_LDFLAGS= -Wl,-rpath -Wl,$(libdir)
csp_async_pscw_CFLAGS= -O2 -DENABLE_CSP

lock_overhead_CFLAGS= -O2
csp_lock_overhead_SOURCES= lock_overhead.c
csp_lock_overhead_LDADD= -L$(libdir) -lcasper
csp_lock_overhead_LDFLAGS= -Wl,-rpath -Wl,$(libdir)
csp_lock_overhead_CFLAGS= -O2 -DENABLE_CSP

lockall_overhead_async_CFLAGS= -O2
csp_lockall_overhead_async_SOURCES= lockall_overhead_async.c
csp_lockall_overhead_async_LDADD= -L$(libdir) -lcasper
csp_lockall_overhead_async_LDFLAGS= -Wl,-rpath -Wl,$(libdir)
csp_lockall_overhead_async_CFLAGS= -O2 -DENABLE_CSP

static_load_async_CFLAGS= -O2
csp_static_load_async_SOURCES= static_load_async.c
csp_static_load_async_LDADD= -L$(libdir) -lcasper
csp_static_load_async_LDFLAGS= -Wl,-rpath -Wl,$(libdir)
csp_static_load_async_CFLAGS= -O2 -DENABLE_CSP

static_load_nop_CFLAGS= -O2
csp_static_load_nop_SOURCES= static_load_nop.c
csp_static_load_nop_LDADD= -L$(libdir) -lcasper
csp_static_load_nop_LDFLAGS= -Wl,-rpath -Wl,$(libdir)
csp_static_load_nop_CFLAGS= -O2 -DENABLE_CSP

runtime_load_nop_CFLAGS= -O2
csp_runtime_load_nop_SOURCES= runtime_load_nop.c
csp_runtime_load_nop_LDADD= -L$(libdir) -lcasper
csp_runtime_load_nop_LDFLAGS= -Wl,-rpath -Wl,$(libdir)
csp_runtime_load_nop_CFLAGS= -O2 -DENABLE_CSP

runtime_load_nop_acc_CFLAGS= -O2
csp_runtime_load_nop_acc_SOURCES= runtime_load_nop_acc.c
csp_runtime_load_nop_acc_LDADD= -L$(libdir) -lcasper
csp_runtime_load_nop_acc_LDFLAGS= -Wl,-rpath -Wl,$(libdir)
csp_runtime_load_nop_acc_CFLAGS= -O2 -DENABLE_CSP

runtime_load_opsize_CFLAGS= -O2
csp_runtime_load_opsize_SOURCES= runtime_load_opsize.c
csp_runtime_load_opsize_LDADD= -L$(libdir) -lcasper
csp_runtime_load_opsize_LDFLAGS= -Wl,-rpath -Wl,$(libdir)
csp_runtime_load_opsize_CFLAGS= -O2 -DENABLE_CSP

async_2np_CFLAGS= -O2
csp_async_2np_SOURCES= async_2np.c
csp_async_2np_LDADD= -L$(libdir) -lcasper
csp_async_2np_LDFLAGS= -Wl,-rpath -Wl,$(libdir)
csp_async_2np_CFLAGS= -O2 -DENABLE_CSP

async_all2all_CFLAGS= -O2
csp_async_all2all_SOURCES= async_all2all.c
csp_async_all2all_LDADD= -L$(libdir) -lcasper
csp_async_all2all_LDFLAGS= -Wl,-rpath -Wl,$(libdir)
csp_async_all2all_CFLAGS= -O2 -DENABLE_CSP

lock_self_overhead_CFLAGS= -O2
csp_lock_self_overhead_SOURCES= lock_self_overhead.c
csp_lock_self_overhead_LDADD= -L$(libdir) -lcasper
csp_lock_self_overhead_LDFLAGS= -Wl,-rpath -Wl,$(libdir)
csp_lock_self_overhead_CFLAGS= -O2 -DENABLE_CSP

csp_lock_self_overhead_no_loadstore_SOURCES= lock_self_overhead_no_loadstore.c
csp_lock_self_overhead_no_loadstore_LDADD= -L$(libdir) -lcasper
csp_lock_self_overhead_no_loadstore_LDFLAGS= -Wl,-rpath -Wl,$(libdir)
csp_lock_self_overhead_no_loadstore_CFLAGS= -O2 -DENABLE_CSP

csp_lock_self_overhead_no_check_SOURCES= lock_self_overhead_no_check.c
csp_lock_self_overhead_no_check_LDADD= -L$(libdir) -lcasper
csp_lock_self_overhead_no_check_LDFLAGS= -Wl,-rpath -Wl,$(libdir)
csp_lock_self_overhead_no_check_CFLAGS= -O2 -DENABLE_CSP

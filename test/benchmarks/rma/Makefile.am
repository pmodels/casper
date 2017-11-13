#
# Copyright (C) 2016. See COPYRIGHT in top-level directory.
#

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(includedir) -O2
AM_LDFLAGS = -Wl,-rpath -Wl,$(libdir)
CSP_LDADD = -L$(libdir) -lcasper 

noinst_HEADERS = $(top_srcdir)/include/ctest.h

noinst_PROGRAMS = \
	lockall_overhead \
	lockall_overhead_no_loadstore \
	lockall_overhead_async	\
	async_2np	\
	async_all2all	\
	lock_overhead \
	lock_self_overhead \
	lock_self_overhead_no_loadstore \
	lock_self_overhead_no_check	\
	op_overhead \
	static_load_async \
	static_load_nop \
	runtime_load_nop \
	runtime_load_opsize \
	runtime_load_nop_acc \
	runtime_load_opsize_acc \
	async_fence_th	\
	async_fence \
	async_pscw	\
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

runtime_load_opsize_acc_LDADD= $(CSP_LDADD)
runtime_load_opsize_acc_CFLAGS= -DENABLE_CSP

lockall_overhead_LDADD= $(CSP_LDADD)
lockall_overhead_CFLAGS= -DENABLE_CSP

lockall_overhead_no_loadstore_LDADD= $(CSP_LDADD)
lockall_overhead_no_loadstore_CFLAGS= -DENABLE_CSP

op_overhead_LDADD= $(CSP_LDADD)
op_overhead_LDFLAGS= -Wl,-rpath -Wl,$(libdir)
op_overhead_CFLAGS= -DENABLE_CSP

async_fence_th_SOURCES= async_fence.c
async_fence_th_LDADD= -lpthread

async_fence_LDADD= $(CSP_LDADD)
async_fence_CFLAGS= -DENABLE_CSP

async_pscw_LDADD= $(CSP_LDADD)
async_pscw_CFLAGS= -DENABLE_CSP

lock_overhead_LDADD= $(CSP_LDADD)
lock_overhead_CFLAGS= -DENABLE_CSP

lockall_overhead_async_LDADD= $(CSP_LDADD)
lockall_overhead_async_CFLAGS= -DENABLE_CSP

static_load_async_LDADD= $(CSP_LDADD)
static_load_async_CFLAGS= -DENABLE_CSP

static_load_nop_LDADD= $(CSP_LDADD)
static_load_nop_CFLAGS= -DENABLE_CSP

runtime_load_nop_LDADD= $(CSP_LDADD)
runtime_load_nop_CFLAGS= -DENABLE_CSP

runtime_load_nop_acc_LDADD= $(CSP_LDADD)
runtime_load_nop_acc_CFLAGS= -DENABLE_CSP

runtime_load_opsize_LDADD= $(CSP_LDADD)
runtime_load_opsize_CFLAGS= -DENABLE_CSP

async_2np_LDADD= $(CSP_LDADD)
async_2np_CFLAGS= -DENABLE_CSP

async_all2all_LDADD= $(CSP_LDADD)
async_all2all_CFLAGS= -DENABLE_CSP

lock_self_overhead_LDADD= $(CSP_LDADD)
lock_self_overhead_CFLAGS= -DENABLE_CSP

lock_self_overhead_no_loadstore_LDADD= $(CSP_LDADD)
lock_self_overhead_no_loadstore_CFLAGS= -DENABLE_CSP

lock_self_overhead_no_check_LDADD= $(CSP_LDADD)
lock_self_overhead_no_check_CFLAGS= -DENABLE_CSP

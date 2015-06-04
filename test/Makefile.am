#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

noinst_PROGRAMS = \
	get	\
	csp_get \
	put	\
	csp_put	\
	put_l_seg	\
	csp_put_l_seg	\
	acc	\
	csp_acc \
	acc_flush_local	\
	csp_acc_flush_local \
	acclock	\
	csp_acclock \
	getacc	\
	csp_getacc	\
	self_acclock	\
	csp_self_acclock	\
	no_loadstore	\
	csp_no_loadstore	\
	csp_no_check	\
	csp_no_accumulate_ordering	\
	acc_pscw	\
	csp_acc_pscw \
	put_fence	\
	csp_put_fence	\
	acc_get_fence	\
	csp_acc_get_fence	\
	fetch_and_op	\
	csp_fetch_and_op	\
	win_allocate	\
	csp_win_allocate	\
	win_create_acc	\
	csp_win_create_acc	\
	init_thread_acc	\
	csp_init_thread_acc	\
	epoch_type	\
	csp_epoch_type	\
	epoch_type_assert	\
	csp_epoch_type_assert 

csp_get_SOURCES= get.c
csp_get_LDFLAGS= -L$(libdir) -lcasper

csp_put_SOURCES= put.c
csp_put_LDFLAGS= -L$(libdir) -lcasper

csp_put_l_seg_SOURCES= put_l_seg.c
csp_put_l_seg_LDFLAGS= -L$(libdir) -lcasper

csp_acc_SOURCES= acc.c
csp_acc_LDFLAGS= -L$(libdir) -lcasper

csp_acc_flush_local_SOURCES= acc_flush_local.c
csp_acc_flush_local_LDFLAGS= -L$(libdir) -lcasper

csp_acclock_SOURCES= acclock.c
csp_acclock_LDFLAGS= -L$(libdir) -lcasper

csp_getacc_SOURCES= getacc.c
csp_getacc_LDFLAGS= -L$(libdir) -lcasper

csp_self_acclock_SOURCES= self_acclock.c
csp_self_acclock_LDFLAGS= -L$(libdir) -lcasper

csp_no_loadstore_SOURCES= no_loadstore.c
csp_no_loadstore_LDFLAGS= -L$(libdir) -lcasper

csp_no_check_SOURCES= no_check.c
csp_no_check_LDFLAGS= -L$(libdir) -lcasper

csp_no_accumulate_ordering_SOURCES= no_accumulate_ordering.c
csp_no_accumulate_ordering_LDFLAGS= -L$(libdir) -lcasper

csp_acc_pscw_SOURCES= acc_pscw.c
csp_acc_pscw_LDFLAGS= -L$(libdir) -lcasper

csp_put_fence_SOURCES= put_fence.c
csp_put_fence_LDFLAGS= -L$(libdir) -lcasper

csp_acc_get_fence_SOURCES= acc_get_fence.c
csp_acc_get_fence_LDFLAGS= -L$(libdir) -lcasper

csp_fetch_and_op_SOURCES= fetch_and_op.c
csp_fetch_and_op_LDFLAGS= -L$(libdir) -lcasper

csp_win_allocate_SOURCES= win_allocate.c
csp_win_allocate_LDFLAGS= -L$(libdir) -lcasper

csp_win_create_acc_SOURCES= win_create_acc.c
csp_win_create_acc_LDFLAGS= -L$(libdir) -lcasper

csp_init_thread_acc_SOURCES= init_thread_acc.c
csp_init_thread_acc_LDFLAGS= -L$(libdir) -lcasper

csp_epoch_type_SOURCES= epoch_type.c
csp_epoch_type_LDFLAGS= -L$(libdir) -lcasper

csp_epoch_type_assert_SOURCES= epoch_type_assert.c
csp_epoch_type_assert_LDFLAGS= -L$(libdir) -lcasper

MPIEXEC=mpiexec

testing:
	./runtest 
	
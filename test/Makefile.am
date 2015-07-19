#
# Copyright (C) 2014. See COPYRIGHT in top-level directory.
#

AM_CPPFLAGS = -I$(top_srcdir)/test/include
LDADD = -L$(libdir) -lcasper

noinst_PROGRAMS = \
	get	\
	put	\
	put_l_seg	\
	acc	\
	acc_flush_local	\
	acclock	\
	getacc	\
	self_acclock	\
	no_loadstore	\
	no_check	\
	no_accumulate_ordering	\
	acc_pscw	\
	put_fence	\
	acc_get_fence	\
	fetch_and_op	\
	win_allocate	\
	win_create_acc	\
	init_thread_acc	\
	epoch_type	\
	epoch_type_assert	\
	win_allocate_info

MPIEXEC=mpiexec

testing:
	./runtest 
	
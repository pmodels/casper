#
# Copyright (C) 2014. See COPYRIGHT in top-level directory.
#

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_LDFLAGS = -Wl,-rpath -Wl,$(libdir)
LDADD = -L$(libdir) -lcasper 

noinst_HEADERS = $(top_srcdir)/include/ctest.h

if CTEST_ENABLE_THREAD_TEST_COND
THREAD_TESTS =  thread_acc_flush 	\
				thread_acc_lock 	\
				thread_multiwins
else
THREAD_TESTS =
endif

noinst_PROGRAMS = \
	get	\
	get_lockall_epoch	\
	put	\
	put_lockall_epoch	\
	acc	\
	acc_lockall_epoch	\
	acc_flush_local	\
	acclock	\
	getacc	\
	getacc_lockall_epoch	\
	subcomm			\
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
	epoch_type	\
	epoch_type_assert	\
	win_allocate_info	\
	win_errhan			\
	comm_errhan			\
	finalize			\
	$(THREAD_TESTS)

MPIEXEC=mpiexec

get_lockall_epoch_SOURCES        = get.c
get_lockall_epoch_CPPFLAGS  = -DTEST_EPOCHS_USED_LOCKALL $(AM_CPPFLAGS)

put_lockall_epoch_SOURCES        = put.c
put_lockall_epoch_CPPFLAGS  = -DTEST_EPOCHS_USED_LOCKALL $(AM_CPPFLAGS)

acc_lockall_epoch_SOURCES        = acc.c
acc_lockall_epoch_CPPFLAGS  = -DTEST_EPOCHS_USED_LOCKALL $(AM_CPPFLAGS)

getacc_lockall_epoch_SOURCES        = getacc.c
getacc_lockall_epoch_CPPFLAGS  = -DTEST_EPOCHS_USED_LOCKALL $(AM_CPPFLAGS)

testing:
	./runtest 

DIST_SUBDIRS = perf

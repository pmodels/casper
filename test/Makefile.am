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
	isend_test			\
	isend_test_l		\
	isend_wait			\
	isend_wait_l		\
	isend_wait_noinfo	\
	isend_wait_anysrc	\
	isend_wait_anysrc_anytag   \
	isend_wait_anysrc_anytag_l \
	isend_wait_anysrc_notag   \
	isend_wait_anysrc_notag_l \
	isend_wait_offload_minsz  \
	isend_wait_nodtypeinfo    \
	isend_wait_deriveddtype   \
	isend_waitall		\
	isend_waitall_l		\
	isendirecv_waitall	\
	isendirecv_waitall_l\
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

isend_test_l_SOURCES        = isend_test.c
isend_test_l_CPPFLAGS       = -DTEST_LMSG $(AM_CPPFLAGS)

isend_wait_l_SOURCES        = isend_wait.c
isend_wait_l_CPPFLAGS       = -DTEST_LMSG $(AM_CPPFLAGS)

isend_wait_noinfo_SOURCES   = isend_wait.c
isend_wait_noinfo_CPPFLAGS  = -DUSE_NOINFO $(AM_CPPFLAGS)

isend_wait_anysrc_SOURCES   = isend_wait.c
isend_wait_anysrc_CPPFLAGS  = -DUSE_ANYSRC $(AM_CPPFLAGS)

isend_wait_anysrc_anytag_SOURCES   = isend_wait.c
isend_wait_anysrc_anytag_CPPFLAGS  = -DUSE_ANYSRC_ANYTAG $(AM_CPPFLAGS)

isend_wait_anysrc_anytag_l_SOURCES = isend_wait.c
isend_wait_anysrc_anytag_l_CPPFLAGS= -DTEST_LMSG -DUSE_ANYSRC_ANYTAG $(AM_CPPFLAGS)

isend_wait_anysrc_notag_SOURCES    = isend_wait.c
isend_wait_anysrc_notag_CPPFLAGS   = -DUSE_ANYSRC_NOTAG $(AM_CPPFLAGS)

isend_wait_anysrc_notag_l_SOURCES    = isend_wait.c
isend_wait_anysrc_notag_l_CPPFLAGS   = -DTEST_LMSG -DUSE_ANYSRC_NOTAG $(AM_CPPFLAGS)

isend_wait_offload_minsz_SOURCES    = isend_wait.c
isend_wait_offload_minsz_CPPFLAGS   = -DUSE_OFFLOAD_MIN_MSGSZ $(AM_CPPFLAGS)

isend_wait_nodtypeinfo_SOURCES   = isend_wait.c
isend_wait_nodtypeinfo_CPPFLAGS  = -DUSE_NODTYPE $(AM_CPPFLAGS)

isend_wait_deriveddtype_SOURCES   = isend_wait.c
isend_wait_deriveddtype_CPPFLAGS  = -DUSE_DERIVED_DTYPE $(AM_CPPFLAGS)

isend_waitall_l_SOURCES     = isend_waitall.c
isend_waitall_l_CPPFLAGS    = -DTEST_LMSG $(AM_CPPFLAGS)

isendirecv_waitall_l_SOURCES     = isendirecv_waitall.c
isendirecv_waitall_l_CPPFLAGS    = -DTEST_LMSG $(AM_CPPFLAGS)

testing:
	./runtest 

EXTRA_DIST = xfail.ompi
DIST_SUBDIRS = benchmarks/rma benchmarks/pt2pt 
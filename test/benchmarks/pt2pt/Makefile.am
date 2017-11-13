#
# Copyright (C) 2016. See COPYRIGHT in top-level directory.
#

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(includedir) -O2
AM_LDFLAGS = -Wl,-rpath -Wl,$(libdir)
LDADD = -L$(libdir) -lcasper 

noinst_HEADERS = $(top_srcdir)/include/ctest.h

noinst_PROGRAMS = \
	comm_creation_overhead  \
	comm_creation_overhead_dupcomm  \
	comm_creation_overhead_tagtrans  \
	osu_latency_async       \
	osu_latency_async_dupcomm       \
	osu_latency_async_step  \
	osu_latency_async_step_withthresh  \
	osu_latency_async_step_dupcomm  \
	orig_osu_latency_async_step     \
	osu_mbw_mr_async        \
	osu_mbw_mr_async_dupcomm \
	osu_mbw_mr_async_step   \
	osu_mbw_mr_async_withthresh   \
	2d_halo_ddt             \
	2d_halo_ddt_step        \
	2d_halo_async           \
	2d_halo_async_step

comm_creation_overhead_dupcomm_SOURCES= comm_creation_overhead.c
comm_creation_overhead_dupcomm_LDADD= $(LDADD)
comm_creation_overhead_dupcomm_LDFLAGS= $(AM_LDFLAGS)
comm_creation_overhead_dupcomm_CFLAGS= $(AM_CPPFLAGS) -DUSE_DUPCOMM

comm_creation_overhead_tagtrans_SOURCES= comm_creation_overhead.c
comm_creation_overhead_tagtrans_LDADD= $(LDADD)
comm_creation_overhead_tagtrans_LDFLAGS= $(AM_LDFLAGS)
comm_creation_overhead_tagtrans_CFLAGS= $(AM_CPPFLAGS) -DUSE_TAGTRANS

osu_latency_async_step_SOURCES= osu_latency_async.c
osu_latency_async_step_LDADD= $(LDADD)
osu_latency_async_step_LDFLAGS= $(AM_LDFLAGS)
osu_latency_async_step_CFLAGS= $(AM_CPPFLAGS) -DSTEP_TIME

osu_latency_async_step_dupcomm_SOURCES= osu_latency_async.c
osu_latency_async_step_dupcomm_LDADD= $(LDADD)
osu_latency_async_step_dupcomm_LDFLAGS= $(AM_LDFLAGS)
osu_latency_async_step_dupcomm_CFLAGS= $(AM_CPPFLAGS) -DSTEP_TIME -DUSE_DUPCOMM

osu_latency_async_dupcomm_SOURCES= osu_latency_async.c
osu_latency_async_dupcomm_LDADD= $(LDADD)
osu_latency_async_dupcomm_LDFLAGS= $(AM_LDFLAGS)
osu_latency_async_dupcomm_CFLAGS= $(AM_CPPFLAGS) -DUSE_DUPCOMM

osu_latency_async_step_withthresh_SOURCES= osu_latency_async.c
osu_latency_async_step_withthresh_LDADD= $(LDADD)
osu_latency_async_step_withthresh_LDFLAGS= $(AM_LDFLAGS)
osu_latency_async_step_withthresh_CFLAGS= $(AM_CPPFLAGS) -DSTEP_TIME -DUSE_OFFLOAD_MIN

orig_osu_latency_async_step_SOURCES= osu_latency_async.c
orig_osu_latency_async_step_LDADD=
orig_osu_latency_async_step_LDFLAGS= $(AM_LDFLAGS)
orig_osu_latency_async_step_CFLAGS= $(AM_CPPFLAGS) -DSTEP_TIME

osu_mbw_mr_async_step_SOURCES= osu_mbw_mr_async.c
osu_mbw_mr_async_step_LDADD= $(LDADD)
osu_mbw_mr_async_step_LDFLAGS= $(AM_LDFLAGS)
osu_mbw_mr_async_step_CFLAGS= $(AM_CPPFLAGS) -DSTEP_TIME

osu_mbw_mr_async_dupcomm_SOURCES= osu_mbw_mr_async.c
osu_mbw_mr_async_dupcomm_LDADD= $(LDADD)
osu_mbw_mr_async_dupcomm_LDFLAGS= $(AM_LDFLAGS)
osu_mbw_mr_async_dupcomm_CFLAGS= $(AM_CPPFLAGS) -DUSE_DUPCOMM

osu_mbw_mr_async_withthresh_SOURCES= osu_mbw_mr_async.c
osu_mbw_mr_async_withthresh_LDADD= $(LDADD)
osu_mbw_mr_async_withthresh_LDFLAGS= $(AM_LDFLAGS)
osu_mbw_mr_async_withthresh_CFLAGS= $(AM_CPPFLAGS) -DUSE_OFFLOAD_MIN

2d_halo_ddt_step_SOURCES= 2d_halo_ddt.c
2d_halo_ddt_step_LDADD= $(LDADD)
2d_halo_ddt_step_LDFLAGS= $(AM_LDFLAGS)
2d_halo_ddt_step_CFLAGS= $(AM_CPPFLAGS) -DSTEP_TIME

2d_halo_async_step_SOURCES= 2d_halo_async.c
2d_halo_async_step_LDADD= $(LDADD)
2d_halo_async_step_LDFLAGS= $(AM_LDFLAGS)
2d_halo_async_step_CFLAGS= $(AM_CPPFLAGS) -DSTEP_TIME
#
# Copyright (C) 2016. See COPYRIGHT in top-level directory.
#

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(includedir) -O2
AM_LDFLAGS = -Wl,-rpath -Wl,$(libdir)
LDADD = -L$(libdir) -lcasper

noinst_HEADERS = $(top_srcdir)/include/ctest.h

noinst_PROGRAMS = \
	comm_creation_overhead  \
	osu_latency_async       \
	osu_latency_async_dupcomm       \
	osu_latency_async_step  \
	osu_latency_async_step_dupcomm  \
	osu_mbw_mr_async        \
	osu_mbw_mr_async_step   \
	halo

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

osu_mbw_mr_async_step_SOURCES= osu_mbw_mr_async.c
osu_mbw_mr_async_step_LDADD= $(LDADD)
osu_mbw_mr_async_step_LDFLAGS= $(AM_LDFLAGS)
osu_mbw_mr_async_step_CFLAGS= $(AM_CPPFLAGS) -DSTEP_TIME
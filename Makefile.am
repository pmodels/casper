#
# Copyright (C) 2014. See COPYRIGHT in top-level directory.
#

ACLOCAL_AMFLAGS = -I confdb
AM_CPPFLAGS =

lib_LTLIBRARIES = libcasper.la
include_HEADERS = include/casper.h

libcasper_la_SOURCES =

external_subdirs = @hwlocsrcdir@
external_ldflags = @hwloclibdir@
convenience_libs = @hwloclib@

libcasper_la_LDFLAGS = $(external_ldflags)
libcasper_la_LIBADD = $(convenience_libs)
EXTRA_libcasper_la_DEPENDENCIES = $(convenience_libs)

include src/Makefile.mk

EXTRA_DIST = CHANGES COPYRIGHT maint autogen.sh

DIST_SUBDIRS = test ${external_subdirs}
SUBDIRS = ${external_subdirs}

buildtests:
	(cd test && make)

check:	buildtests
	(cd test && make testing MODE=auto)

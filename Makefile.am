#
# Copyright (C) 2014. See COPYRIGHT in top-level directory.
#

ACLOCAL_AMFLAGS = -I confdb
AM_CPPFLAGS =

lib_LTLIBRARIES = libcasper.la
include_HEADERS = include/casper.h

libcasper_la_SOURCES =

include src/Makefile.mk

EXTRA_DIST = CHANGES COPYRIGHT maint autogen.sh

DIST_SUBDIRS = test

buildtests:
	(cd test && make)

check:	buildtests
	(cd test && make testing MODE=auto)

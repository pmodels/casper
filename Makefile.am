#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

ACLOCAL_AMFLAGS = -I confdb
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include

lib_LTLIBRARIES = libasp.la

libasp_la_SOURCES = src/mpi.c \
                    src/mpio.c \
                    src/mpi/rma/win_create.c \
                    src/mpi/init/init.c \
                    src/mpi/init/finalize.c \
                    src/mpi/init/abort.c \
                    src/mpi/comm/comm_dup.c

asp_headers =	include/asp.h

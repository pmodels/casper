#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

ACLOCAL_AMFLAGS = -I confdb
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include 

lib_LTLIBRARIES = libasp.la

libasp_la_SOURCES = src/mpi_wrap.c	\
                    src/mpi/rma/win_allocate.c \
                    src/mpi/rma/win_free.c \
                    src/mpi/rma/put.c \
                    src/mpi/rma/get.c \
                    src/mpi/rma/accumulate.c \
                    src/mpi/rma/win_lock_all.c	\
                    src/mpi/rma/win_unlock_all.c	\
                    src/mpi/rma/win_flush.c	\
                    src/mpi/rma/win_flush_all.c	\
                    src/mpi/init/init.c \
                    src/mpi/init/finalize.c \
                    src/asp/main.c \
                    src/asp/mpi/finalize.c \
                    src/asp/rma/win_allocate.c \
                    src/asp/rma/win_free.c	\
                    src/util/hash.c

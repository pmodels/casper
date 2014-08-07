#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

ACLOCAL_AMFLAGS = -I confdb
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include 

lib_LTLIBRARIES = libmtcore.la

libmtcore_la_SOURCES = src/mpi_wrap.c	\
                    src/mpi/func.c \
                    src/mpi/rma/win_allocate.c \
                    src/mpi/rma/win_free.c \
                    src/mpi/rma/put.c \
                    src/mpi/rma/get.c \
                    src/mpi/rma/accumulate.c \
                    src/mpi/rma/win_lock_all.c	\
                    src/mpi/rma/win_unlock_all.c	\
                    src/mpi/rma/win_lock.c	\
                    src/mpi/rma/win_unlock.c	\
                    src/mpi/rma/win_flush.c	\
                    src/mpi/rma/win_flush_all.c	\
                    src/mpi/rma/get_helper.c	\
                    src/mpi/init/init.c \
                    src/mpi/init/finalize.c \
                    src/helper/func.c \
                    src/helper/main.c \
                    src/helper/mpi/finalize.c \
                    src/helper/rma/win_allocate.c \
                    src/helper/rma/win_free.c	\
                    src/util/hash.c

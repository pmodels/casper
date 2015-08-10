#
# Copyright (C) 2014. See COPYRIGHT in top-level directory.
#

ACLOCAL_AMFLAGS = -I confdb
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include
AM_CPPFLAGS += -I$(top_srcdir)/src/user/include

lib_LTLIBRARIES = libcasper.la
include_HEADERS = include/casper.h

libcasper_la_SOURCES = src/mpi_wrap.c	\
                    src/casper.c	\
                    src/user/cmd.c	\
                    src/user/rma/win_allocate.c \
                    src/user/rma/win_create.c \
                    src/user/rma/win_create_dynamic.c \
                    src/user/rma/win_allocate_shared.c \
                    src/user/rma/win_free.c \
                    src/user/rma/put.c \
                    src/user/rma/get.c \
                    src/user/rma/accumulate.c \
                    src/user/rma/get_accumulate.c \
                    src/user/rma/fetch_and_op.c \
                    src/user/rma/compare_and_swap.c \
                    src/user/rma/rput.c \
                    src/user/rma/rget.c \
                    src/user/rma/raccumulate.c \
                    src/user/rma/rget_accumulate.c \
                    src/user/rma/win_lock_all.c	\
                    src/user/rma/win_unlock_all.c	\
                    src/user/rma/win_lock.c	\
                    src/user/rma/win_unlock.c	\
                    src/user/rma/win_sync.c	\
                    src/user/rma/win_flush.c	\
                    src/user/rma/win_flush_all.c	\
                    src/user/rma/win_flush_local.c	\
                    src/user/rma/win_flush_local_all.c	\
                    src/user/rma/win_fence.c	\
                    src/user/rma/win_post.c	\
                    src/user/rma/win_start.c	\
                    src/user/rma/win_wait.c	\
                    src/user/rma/win_test.c	\
                    src/user/rma/win_complete.c	\
                    src/user/rma/win_get_attr.c	\
                    src/user/rma/csp_get_ghost.c	\
                    src/user/rma/csp_op_segment.c	\
                    src/user/rma/csp_bind_ghost.c	\
                    src/user/init/init.c \
                    src/user/init/initthread.c \
                    src/user/init/finalize.c \
                    src/user/errhan/comm_set_errhandler.c \
                    src/user/errhan/errhandler_set.c \
                    src/ghost/cmd.c \
                    src/ghost/main.c \
                    src/ghost/mpi/finalize.c \
                    src/ghost/rma/win_allocate.c \
                    src/ghost/rma/win_free.c	\
                    src/util/info.c

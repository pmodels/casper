#
# Copyright (C) 2014. See COPYRIGHT in top-level directory.
#

AM_CPPFLAGS += -I$(top_srcdir)/src/user/include

libcasper_la_SOURCES += src/user/ghost_size.c	\
                        src/user/mpi_wrap.c

include $(top_srcdir)/src/user/common/Makefile.mk
include $(top_srcdir)/src/user/comm/Makefile.mk
include $(top_srcdir)/src/user/errhan/Makefile.mk
include $(top_srcdir)/src/user/init/Makefile.mk
include $(top_srcdir)/src/user/rma/Makefile.mk
include $(top_srcdir)/src/user/topo/Makefile.mk

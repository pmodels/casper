#
# Copyright (C) 2014. See COPYRIGHT in top-level directory.
#

AM_CPPFLAGS += -I$(top_srcdir)/src/user/include

libcasper_la_SOURCES += src/user/ghost_size.c	\
                        src/user/mpi_wrap.c

libcasper_la_SOURCES += src/user/include/cspu.h \
			src/user/include/cspu_cwp.h \
			src/user/include/cspu_errhan.h \
			src/user/include/cspu_rma_sync.h \
			src/user/include/cspu_thread.h

include $(top_srcdir)/src/user/include/Makefile.mk
include $(top_srcdir)/src/user/common/Makefile.mk
include $(top_srcdir)/src/user/comm/Makefile.mk
include $(top_srcdir)/src/user/errhan/Makefile.mk
include $(top_srcdir)/src/user/init/Makefile.mk
include $(top_srcdir)/src/user/rma/Makefile.mk
include $(top_srcdir)/src/user/topo/Makefile.mk
include $(top_srcdir)/src/user/spawn/Makefile.mk

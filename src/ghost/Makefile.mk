#
# Copyright (C) 2015. See COPYRIGHT in top-level directory.
#

AM_CPPFLAGS += -I$(top_srcdir)/src/ghost/include

libcasper_la_SOURCES += src/ghost/cmd.c

include $(top_srcdir)/src/ghost/init/Makefile.mk
include $(top_srcdir)/src/ghost/rma/Makefile.mk
#
# Copyright (C) 2015. See COPYRIGHT in top-level directory.
#

AM_CPPFLAGS += -I$(top_srcdir)/include -I$(top_builddir)/include

libcasper_la_SOURCES += src/init/init.c \
                        src/init/initthread.c \
                        src/util/info.c


include $(top_srcdir)/src/user/Makefile.mk
include $(top_srcdir)/src/ghost/Makefile.mk
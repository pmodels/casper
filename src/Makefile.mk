#
# Copyright (C) 2015. See COPYRIGHT in top-level directory.
#

AM_CPPFLAGS += -I$(top_builddir)/include -I$(top_srcdir)/include  \
               -I$(top_srcdir)/src/common/include  \
               -I$(top_srcdir)/src/common/util

include $(top_srcdir)/src/user/Makefile.mk
include $(top_srcdir)/src/ghost/Makefile.mk
include $(top_srcdir)/src/common/Makefile.mk
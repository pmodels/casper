#
# Copyright (C) 2016. See COPYRIGHT in top-level directory.
#

include $(top_srcdir)/src/common/include/Makefile.mk
include $(top_srcdir)/src/common/init/Makefile.mk
include $(top_srcdir)/src/common/msg/Makefile.mk
include $(top_srcdir)/src/common/error/Makefile.mk
include $(top_srcdir)/src/common/util/Makefile.mk

if csp_have_topo_opt
include $(top_srcdir)/src/common/topo/Makefile.mk
endif

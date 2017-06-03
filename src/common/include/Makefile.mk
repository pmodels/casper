#
# Copyright (C) 2016. See COPYRIGHT in top-level directory.
#

libcasper_la_SOURCES += src/common/include/csp.h \
			src/common/include/csp_cwp.h     \
			src/common/include/csp_error.h   \
			src/common/include/csp_mlock.h   \
			src/common/include/csp_msg.h     \
			src/common/include/csp_thread.h  \
			src/common/include/csp_util.h    \
			src/common/include/csp_offload.h \
			src/common/include/csp_datatype.h

if csp_have_topo_opt
libcasper_la_SOURCES += src/common/include/csp_topo.h
endif

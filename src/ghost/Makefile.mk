#
# Copyright (C) 2015. See COPYRIGHT in top-level directory.
#

AM_CPPFLAGS += -I$(top_srcdir)/src/ghost/include

libcasper_la_SOURCES += src/ghost/cmd.c \
                        src/ghost/init/init.c \
                        src/ghost/init/finalize.c \
                        src/ghost/rma/win_allocate.c \
                        src/ghost/rma/win_free.c
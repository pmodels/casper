#
# Copyright (C) 2014. See COPYRIGHT in top-level directory.
#

noinst_PROGRAMS = lu_passive_1d	\
				mtcore_lu_passive_1d

lu_pscw_1d_new_CFLAGS= -O2
lu_pscw_1d_new_xin_CFLAGS= -O2

lu_passive_1d_CFLAGS= -O2
mtcore_lu_passive_1d_SOURCES= lu_passive_1d.c
mtcore_lu_passive_1d_LDFLAGS= -L$(libdir) -lmtcore -lmpich
mtcore_lu_passive_1d_CFLAGS= -O2 -DMTCORE

#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

noinst_PROGRAMS = \
	get	\
	mtcore_get \
	put	\
	mtcore_put	\
	acc	\
	mtcore_acc \
	acclock	\
	mtcore_acclock \
	getacc	\
	mtcore_getacc	\
	self_acclock	\
	mtcore_self_acclock	\
	no_loadstore	\
	mtcore_no_loadstore	\
	mtcore_no_conflict_epoch
	
mtcore_get_SOURCES= get.c
mtcore_get_LDFLAGS= -L$(libdir) -lmtcore

mtcore_put_SOURCES= put.c
mtcore_put_LDFLAGS= -L$(libdir) -lmtcore

mtcore_acc_SOURCES= acc.c
mtcore_acc_LDFLAGS= -L$(libdir) -lmtcore

mtcore_acclock_SOURCES= acclock.c
mtcore_acclock_LDFLAGS= -L$(libdir) -lmtcore

mtcore_getacc_SOURCES= getacc.c
mtcore_getacc_LDFLAGS= -L$(libdir) -lmtcore

mtcore_self_acclock_SOURCES= self_acclock.c
mtcore_self_acclock_LDFLAGS= -L$(libdir) -lmtcore

mtcore_no_loadstore_SOURCES= no_loadstore.c
mtcore_no_loadstore_LDFLAGS= -L$(libdir) -lmtcore

mtcore_no_conflict_epoch_SOURCES= no_conflict_epoch.c
mtcore_no_conflict_epoch_LDFLAGS= -L$(libdir) -lmtcore
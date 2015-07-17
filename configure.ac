#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([casper], [1.0])

AC_CONFIG_AUX_DIR(confdb)
AC_CONFIG_MACRO_DIR(confdb)
AM_INIT_AUTOMAKE([-Wall -Werror -Wno-portability-recursive silent-rules foreign 1.12.3 subdir-objects])

# Bug in libtool adds -O2 and -g by default
PAC_PUSH_FLAG([CFLAGS])
AC_PROG_CC(mpicc)
AM_PROG_CC_C_O
PAC_POP_FLAG([CFLAGS])

AM_PROG_AR

LT_PREREQ([2.2.6])

AC_CONFIG_HEADER([include/casperconf.h])

# Bug in libtool adds -O2 and -g by default
PAC_PUSH_FLAG([CFLAGS])
LT_INIT()
PAC_POP_FLAG([CFLAGS])

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h])

# Non-verbose make
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Set default value for CFLAGS if user does not specify
if test -z "$CFLAGS"; then
   CFLAGS=-O2
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_RESTRICT
AC_C_INLINE

# Check for enabling strict
PAC_ARG_STRICT

# Runtime load balancing optimization
AC_ARG_ENABLE(runtime-load, AC_HELP_STRING([--enable-runtime-load],[Enable Runtime Load Balancing]),
                 [ enable_runtime_load=$enableval ],
                 [ enable_runtime_load=no ])
AC_MSG_CHECKING(runtime Load balancing support)
AC_MSG_RESULT($enable_runtime_load)
if test "$enable_runtime_load" = "yes"; then
   AC_DEFINE(CSP_ENABLE_RUNTIME_LOAD_OPT,1,[Define if enable runtime load balancing])
fi

# Epoch statuc check
AC_ARG_ENABLE(epochstat-check, AC_HELP_STRING([--disable-epochstat-check],[Disable epoch status check for better performance]),
                 [ enable_epochstat_check=$enableval ],
                 [ enable_epochstat_check=yes ])
AC_MSG_CHECKING(epoch status checking support)
AC_MSG_RESULT($enable_epochstat_check)
if test "$enable_epochstat_check" = "yes"; then
   AC_DEFINE(CSP_ENABLE_EPOCH_STAT_CHECK,1,[Define if disable epoch status check])
fi

# Check for __attribute__ support.  This was originally taken from
# the PAC_C_GNU_ATTRIBUTE macro in mpich.
#
# We start by requiring Gcc.  Some other compilers accept __attribute__
# but generate warning messages, or have different interpretations
# (which seems to make __attribute__ just as bad as #pragma)
# For example, the Intel icc compiler accepts __attribute__ and
# __attribute__((pure)) but generates warnings for __attribute__((format...))
if test "$GCC" = "yes" ; then
    AC_CACHE_CHECK([whether __attribute__ allowed],
pac_cv_gnu_attr_pure,[
AC_TRY_COMPILE([int foo(int) __attribute__ ((pure));],[int a;],
pac_cv_gnu_attr_pure=yes,pac_cv_gnu_attr_pure=no)])
AC_CACHE_CHECK([whether __attribute__((format)) allowed],
pac_cv_gnu_attr_format,[
AC_TRY_COMPILE([int foo(char *,...) __attribute__ ((format(printf,1,2)));],[int a;],
pac_cv_gnu_attr_format=yes,pac_cv_gnu_attr_format=no)])
    if test "$pac_cv_gnu_attr_pure" = "yes" -a "$pac_cv_gnu_attr_format" = "yes" ; then
        AC_DEFINE(HAVE_GCC_ATTRIBUTE,1,[Define if GNU __attribute__ is supported])
    fi
fi

# check for compiler support for the __typeof() extension
AC_CACHE_CHECK([whether the compiler supports __typeof(variable)],
               [pac_cv_have___typeof],
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]],[[double foo = 0.0; __typeof(foo) bar = 1.0;]])],
                  [pac_cv_have___typeof=yes],
                  [pac_cv_have___typeof=no])]
)
if test "$pac_cv_have___typeof" = "yes" ; then
    AC_DEFINE([HAVE___TYPEOF],[1],[defined if the C compiler supports __typeof(variable)])
fi

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([test/Makefile])
AC_CONFIG_FILES([test/perf/Makefile])
AC_CONFIG_FILES([test/testlist])
AC_CONFIG_FILES([test/runtest])
AC_OUTPUT_COMMANDS([chmod a+x test/runtest])

AC_OUTPUT

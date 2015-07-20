#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([casper-testsuite], [1.0])

AC_CONFIG_AUX_DIR(confdb)
AC_CONFIG_MACRO_DIR(confdb)

echo "Running configure for Casper tests"
AM_INIT_AUTOMAKE([-Wall -Werror -Wno-portability-recursive silent-rules foreign 1.12.3 subdir-objects])

AC_CONFIG_HEADER([include/ctestconf.h])

# Non-verbose make
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Running C compiler tests
PAC_PROG_CC
AM_PROG_CC_C_O

# not using libtool for the test suite, so no LT_INIT.  Instead, test here
# for Library programs
AC_PROG_RANLIB
AM_PROG_AR

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(sys/sysctl.h sys/sysinfo.h pthread_np.h)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_RESTRICT
AC_C_INLINE

# Check for functions.
AC_CHECK_FUNCS(sysctl get_nprocs get_nprocs_conf pthread_getaffinity_np)

# Check for types
AC_CHECK_TYPES([cpu_set_t],,,[[#include <sched.h>]])

# Check for enabling strict
PAC_ARG_STRICT

# check for attribute support
PAC_C_GNU_ATTRIBUTE

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([perf/Makefile])
AC_CONFIG_FILES([testlist])
AC_CONFIG_FILES([runtest])
AC_OUTPUT_COMMANDS([chmod a+x runtest])

AC_OUTPUT

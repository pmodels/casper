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
# Bug in libtool adds -O2 and -g by default
PAC_PUSH_FLAG([CFLAGS])
AC_PROG_CC(mpicc)
PAC_POP_FLAG([CFLAGS])

# not using libtool for the test suite, so no LT_INIT.  Instead, test here
# for Library programs
AC_PROG_RANLIB
AM_PROG_AR

# Check for enabling strict
AC_ARG_ENABLE(stricttest,
AC_HELP_STRING([--enable-stricttest], [Turn on strict compilation for test suite]))
PAC_CC_STRICT($enable_stricttest)
CFLAGS="$CFLAGS $pac_cc_strict_flags"
# Checking should be executed after --enable-stricttest being interpreted.

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

# Checkes for enabling thread tests
AC_ARG_ENABLE(thread-test, AC_HELP_STRING([--enable-thread-test],
                 [Enable Casper thread tests (no by default).
                 Thread safety is required in casper, otherwise the results of
                 thread tests are undefined. Also see option --enable-thread-safety
                 in Casper configure. These tests require POSIX threads.]),
                 [ enable_thread_test=$enableval ],
                 [ enable_thread_test=no ])
AC_MSG_CHECKING(enable thread tests)
AC_MSG_RESULT($enable_thread_test)
AM_CONDITIONAL(CTEST_ENABLE_THREAD_TEST_COND, [test "$enable_thread_test" = "yes"])
CTEST_ENABLE_THREAD_TEST=0
if test "$enable_thread_test" = "yes"; then
    CTEST_ENABLE_THREAD_TEST=1
    AC_DEFINE(CTEST_ENABLE_THREAD_TEST,1,[Define if enable thread tests])
fi
AC_SUBST(CTEST_ENABLE_THREAD_TEST)

# Checks if pthread is supported for thread tests
if test "$enable_thread_test" = "yes"; then
    have_pthreads=no
    AC_CHECK_HEADERS(pthread.h)
    AC_CHECK_LIB([pthread],[pthread_key_create],have_lpthread=yes)
    if test "$have_lpthread" = "yes" ; then
        PAC_PREPEND_FLAG([-lpthread],[LIBS])
        # this check should come after the AC_CHECK_LIB for -lpthread
        AC_CHECK_FUNC([pthread_create],[],[AC_MSG_ERROR([unable to find pthreads library])])
    fi
fi

# check for attribute support
PAC_C_GNU_ATTRIBUTE

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([perf/Makefile])
AC_CONFIG_FILES([testlist])
AC_CONFIG_FILES([runtest])
AC_OUTPUT_COMMANDS([chmod a+x runtest])

AC_OUTPUT

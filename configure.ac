dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)

AC_INIT([fbsql], [0.03], [barwick@sql-info.de])

AC_CONFIG_AUX_DIR([config])


AC_CHECK_HEADERS([fcntl.h limits.h string.h stdlib.h math.h sys/time.h unistd.h])
AC_CHECK_FUNCS([gettimeofday memmove memset strchr strdup strstr strtol])
AM_INIT_AUTOMAKE([1.9 foreign])

AC_PROG_CC
AM_PROG_LEX
AC_PROG_YACC

AC_CONFIG_FILES([Makefile src/Makefile])

AC_OUTPUT

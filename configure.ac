dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)

AC_INIT([fbsql], [0.1.4], [barwick@sql-info.de])

AC_CONFIG_AUX_DIR([config])

ibase=/usr/include
AC_SUBST(ibase)
readline=/usr/include
AC_SUBST(readline)

AC_CHECK_HEADERS([fcntl.h limits.h string.h stdlib.h math.h sys/time.h unistd.h])
AC_CHECK_FUNCS([gettimeofday memmove memset strchr strdup strstr strtol])
AM_INIT_AUTOMAKE([1.9 foreign])

AC_PROG_CC
AM_PROG_LEX
AC_PROG_YACC

AC_CONFIG_FILES([Makefile src/Makefile])

AC_ARG_WITH(
    ibase,
    AS_HELP_STRING(
        [--with-ibase=DIR],
        [directory containing ibase.h]
    ),
    [ibase="$withval"]
)


AC_ARG_WITH(
    readline,
    AS_HELP_STRING(
        [--with-readline=DIR],
        [directory containing readline.h]
    ),
    [readline="$withval"]
)


AC_OUTPUT

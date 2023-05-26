bin_PROGRAMS = fbsql
fbsql_SOURCES = main.c common.c input.c inputloop.c tab-complete.c command.c command_test.c query.c port/strlcpy.c port/pgstrcasecmp.c fbsqlscan.l
fbsql_LDADD = -lfq -lfbclient -lreadline -lm
AM_CPPFLAGS = -I$(ibase) -I$(readline)


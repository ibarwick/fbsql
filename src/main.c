/* ---------------------------------------------------------------------
 *
 * main.c
 *
 * fbsql startup code
 *
 * ---------------------------------------------------------------------
 */

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <getopt.h>

#include "ibase.h"

#include "libfq.h"

#include "fbsql.h"
#include "settings.h"
#include "input.h"
#include "inputloop.h"
#include "common.h"


/*
 * Global fbsql options
 */
fbsqlSettings fset;

/* Internal helper functions */
static void parse_fbsql_options(int argc, char *argv[]);
static void usage(void);
static void show_version(void);


/**
 * main()
 *
 * Connect to database and launch the InputLoop.
 */
int
main(int argc, char *argv[])
{
	int result;
	const char *kw[5];
	const char *val[5];
	int i = 0;

	init_settings();

	/* initialise readline/history */
	init_readline();

	parse_fbsql_options(argc, argv);

	printf("fbsql %s\n", FBSQL_VERSION);

	/* The Firebird library will pick up the ISC_* variables by itself, but
	 * let's handle them here so we can display explicit warnings if a
	 * connection parameter is missing completely.
	 */
	if (fset.dbpath == NULL)
	{
		fset.dbpath = getenv("ISC_DATABASE");
		if (fset.dbpath == NULL) {
			puts("need -d dbpath\n");
			return 1;
		}
	}

	if (fset.username == NULL)
	{
		fset.username = getenv("ISC_USER");
		if (fset.username == NULL) {
			puts("need -u username\n");
			return 1;
		}
	}

	kw[i] = "db_path";
	val[i] = fset.dbpath;
	i++;

	kw[i] = "user";
	val[i] = fset.username;
	i++;

	kw[i] = "password";
	val[i] = fset.password;
	i++;

	kw[i] = "client_encoding";
	val[i] = fset.client_encoding;
	i++;

	kw[i] = NULL;
	val[i] = NULL;

	fset.conn = FQconnectdbParams(kw, val);

	if (FQstatus(fset.conn) == CONNECTION_BAD)
	{
		printf("Error connecting to '%s' as '%s'\n", fset.dbpath, fset.username);
		printf("%s\n", FQerrorMessage(fset.conn));
		exit(1);
	}

	fset.sversion = FQserverVersionString(fset.conn);
	fset.client_encoding_id = FQclientEncodingId(fset.conn);
	FQsetGetdsplen(fset.conn, true);

	printf("Connected to Firebird v%s (libfq version %s)\n", fset.sversion, FQlibVersionString());

	FQsetAutocommit(fset.conn, fset.autocommit);

	result = InputLoop(stdin);

	save_history(fset.fbsql_history);

	if (fset.conn->trans != 0L)
		puts("Rolling back uncommitted transaction");

	FQfinish(fset.conn);

	return result;
}


/**
 * parse_fbsql_options()
 *
 * Parse command line options
 */
void
parse_fbsql_options(int argc, char *argv[])
{
	static struct option long_options[] =
	{
		{"database", required_argument, NULL, 'd'},
		{"echo-internal", no_argument, NULL, 'E'},
		{"username", required_argument, NULL, 'u'},
		{"password", required_argument, NULL, 'p'},
		{"client-encoding", required_argument, NULL, 'C'},
		{"help", no_argument, NULL, '?'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};

	int			optindex;
	extern char *optarg;
	extern int	optind;
	int			c;

	while ((c = getopt_long(argc, argv, "d:Eu:p:C:?V",
							long_options, &optindex)) != -1)
	{

		switch (c)
		{
			case 'd':
				fset.dbpath = strdup(optarg);
				break;

			case 'E':
				fset.echo_hidden = true;
				break;

			case 'u':
				fset.username = strdup(optarg);
				break;

			case 'p':
				fset.password = strdup(optarg);
				break;

			case 'C':
				fset.client_encoding = strdup(optarg);
				break;

			case '?':
				usage();
				exit(0);

			case 'V':
				show_version();
				exit(0);
		}
	}

	/*
	 * If arguments still remain, use them as the database name and username
	 */
	while (argc - optind >= 1)
	{
		if (!fset.dbpath)
			fset.dbpath = argv[optind];
		else if (!fset.username)
			fset.username = argv[optind];
		/* else ignore */

		optind++;
	}
}


/**
 * show_version()
 *
 * Display version information
 */

void
show_version(void)
{
	printf("fbsql (Firebird) %s\n", FBSQL_VERSION);
}


/**
 * usage()
 *
 * Show usage and command line options
 */
void
usage(void)
{
	const char *username;
	const char *dbname;

	/* Find default user and database from environment variables */
	username = getenv("ISC_USER");
	dbname	 = getenv("ISC_DATABASE");

	printf("fbsql is an interactive terminal for Firebird.\n\n");
	printf("Usage:\n");
	printf("\n");
	printf("  fbsql [OPTION]... [DBNAME [USERNAME]]\n\n");

	printf("General options:\n");
	printf("  -V, --version            output version information, then exit\n");
	printf("  -?, --help               show this help, then exit\n");
	printf("\n");

	printf("Connection options:\n");

	printf("  -d, --dbname=DBNAME      database to connect to");
	if (dbname)
		printf(" (default: \"%s\")", dbname);
	printf("\n");

	printf("  -u, --username=USERNAME  database user name");
	if (username)
		  printf(" (default: \"%s\")", username);
	printf("\n");

	printf("  -p, --password           password\n");

	printf("  -C, --client-encoding    client encoding (default: UTF-8)\n");

	printf("\n");

	printf("Display options:\n");

	printf("  -E, --echo-internal      display queries generated by internal commands\n");

	printf("\n");
}

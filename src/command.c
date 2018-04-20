/* ---------------------------------------------------------------------
 *
 * command.c
 *
 * Handle slash commands
 *
 * useful links:
 *   http://www.alberton.info/firebird_sql_meta_info.html
 *   http://ibexpert.net/ibe/index.php?n=Doc.SystemObjects
 *   http://edn.embarcadero.com/article/25259
 *
 * ---------------------------------------------------------------------
 */
#define _XOPEN_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <time.h>

#include "libfq.h"
#include "fbsql.h"
#include "command.h"
#include "settings.h"
#include "query.h"
#include "common.h"


static FBresult* commandExec(const char *query);
static void commandExecPrint(const char *query, const printQueryOpt *pqopt);

static void showUsage(void);

static void describeObject(char *name);
static void _describeObject(char *name, char *object_type, char *query);

static void describeTable(char *name);
static void describeView(char *name);
static void describeIndex(char *name);

static bool execUtil(char *command);
static bool _execUtilSetIndexStatistics(void);

static void listDatabaseInfo(void);
static void listFunctions(char *pattern);
static void listIndexes(char *pattern, bool show_system, bool show_extended);
static void listProcedures(char *pattern);
static void listSequences(char *pattern, bool show_system);
static void listTables(char *pattern, bool show_system);
static void listUsers(void);
static void listViews(char *pattern);

static char *_listIndexSegments(char *index_name);

static void showActivity(void);
static void showCopyright(void);
static void showUtilOptions(void);

static bool do_plan_display(const char *value);
static char *render_plan_display(short plan_display);

static void _wildcard_pattern_clause(char *pattern, char *field, FQExpBufferData *buf);
static const char *_align2string(enum printFormat in);
static const char *_border2string(enum borderFormat in);
static char *_sqlFieldType(void);

static void _command_test(const char *param);
static void _command_test_ins(void);

static FBresult*
commandExec(const char *query)
{
	if (fset.echo_hidden == true)
		printf("%s\n", query);

	return FQexecTransaction(fset.conn, query);
}


static void
commandExecPrint(const char *query, const printQueryOpt *pqopt)
{
	FBresult   *query_result;

	query_result = commandExec(query);

	/* XXX fails silently... may want to add error handling */

	if (FQresultStatus(query_result) != FBRES_TUPLES_OK)
	{
		FQclear(query_result);
		return;
	}

	if (FQntuples(query_result))
	{
		printQuery(query_result, pqopt);

		puts("");
	}
	else
	{
		puts("No items found");
	}

	FQclear(query_result);
}


backslashResult
HandleSlashCmds(FbsqlScanState scan_state,
                FQExpBuffer query_buf)
{
	backslashResult status = FBSQL_CMD_UNKNOWN;
	char	   *cmd;
	char	   *arg;

	/* Parse the command name */
	cmd = fbsql_scan_slash_command(scan_state);

	/* Execute it */
	status = execSlashCommand(cmd, scan_state, query_buf);

	if (status == FBSQL_CMD_UNKNOWN)
	{
		status = FBSQL_CMD_ERROR;
	}

	if (status != FBSQL_CMD_ERROR)
	{
		/* consume remaining arguments after a valid command */
		/* note we suppress evaluation of backticks here */
		while ((arg = fbsql_scan_slash_option(scan_state,
											 OT_NO_EVAL, NULL, false)))
		{
			fbsql_error("\\%s: extra argument \"%s\" ignored\n", cmd, arg);
			free(arg);
		}
	}
	else
	{
		/* silently throw away rest of line after an erroneous command */
		while ((arg = fbsql_scan_slash_option(scan_state,
											 OT_WHOLE_LINE, NULL, false)))
			free(arg);
	}

	/* if there is a trailing \\, swallow it */
	fbsql_scan_slash_command_end(scan_state);

	free(cmd);

	/* some commands write to queryFout, so make sure output is sent */
	//fflush(fset.queryFout);

	return status;
}


static backslashResult
execSlashCommand(const char *cmd,
             FbsqlScanState scan_state,
             FQExpBuffer query_buf)
{
	bool		success = true;
	backslashResult status = FBSQL_CMD_SKIP_LINE;
	bool show_extended, show_system;

	show_extended = strchr(cmd, '+') ? true : false;
	show_system = strchr(cmd, 'S') ? true : false;

	/* \q - quit session */
	if (strncmp(cmd, "q", 1) == 0)
	{
		status = FBSQL_CMD_TERMINATE;
	}

	/* \? - help/usage */
	else if (strncmp(cmd, "?", 1) == 0)
	{
		showUsage();
	}

	/* \activity - show active connections */
	else if (strncmp(cmd, "activity", 8) == 0)
	{
		showActivity();
	}

	/* \autocommit */
	else if (strncmp(cmd, "autocommit", 10) == 0)
	{
		if (fset.autocommit == true)
		{
			fset.autocommit = false;
			if (fset.conn)
				fset.conn->autocommit = false;
			puts("Autocommit off");
		}
		else
		{
			fset.autocommit = true;
			if (fset.conn)
				fset.conn->autocommit = true;
			puts("Autocommit on");
		}
	}

	/* \a - toggle output align mode */
	else if (strncmp(cmd, "a", 1) == 0)
	{
		if (fset.popt.topt.format != PRINT_ALIGNED)
			success = do_format("alignment", "aligned", &fset.popt, fset.quiet);
		else
			success = do_format("alignment", "unaligned", &fset.popt, fset.quiet);
	}

	/* \copyright */
	else if (strncmp(cmd, "copyright", 9) == 0)
	{
		showCopyright();
	}

	/* \conninfo -- display information about the current connection */
	else if (strncmp(cmd, "conninfo", 8) == 0)
	{
		if (!fset.conn) {
			puts("You are not connected to any database");
		}
		else {
			printf("You are currently connected as user '%s' to '%s'\n", fset.username, fset.dbpath);
		}
	}

	/* \df - describe functions */
	else if (strncmp(cmd, "df", 2) == 0)
	{
		char *opt0 = fbsql_scan_slash_option(scan_state,
											 OT_NORMAL, NULL, false);

		listFunctions(opt0);

		free(opt0);
	}

	/* \di - describe indexes */

	else if (strncmp(cmd, "di", 2) == 0)
	{


		char *opt0 = fbsql_scan_slash_option(scan_state,
											 OT_NORMAL, NULL, false);

		listIndexes(opt0, show_system, show_extended);

		free(opt0);
	}
	/* \dp - describe procedures */
	else if (strncmp(cmd, "dp", 2) == 0)
	{
		char *opt0 = fbsql_scan_slash_option(scan_state,
											 OT_NORMAL, NULL, false);

		listProcedures(opt0);

		free(opt0);
	}

	/* \ds - describe sequences */
	else if (strncmp(cmd, "ds", 2) == 0)
	{
		char *opt0 = fbsql_scan_slash_option(scan_state,
											 OT_NORMAL, NULL, false);

		listSequences(opt0, show_system);

		free(opt0);
	}
	/* \dt - describe tables */
	else if (strncmp(cmd, "dt", 2) == 0)
	{
		char *opt0 = fbsql_scan_slash_option(scan_state,
											 OT_NORMAL, NULL, false);

		listTables(opt0, show_system);

		free(opt0);
	}
	/* \du - list users */
	else if (strncmp(cmd, "du", 2) == 0)
	{
		listUsers();
	}
	/* \dv - describe views */
	else if (strncmp(cmd, "dv", 2) == 0)
	{
		char *opt0 = fbsql_scan_slash_option(scan_state,
											 OT_NORMAL, NULL, false);

		listViews(opt0);

		free(opt0);
	}
	/* \d - describe object */
	else if (strncmp(cmd, "d", 1) == 0)
	{
		char *opt0 = fbsql_scan_slash_option(scan_state,
											 OT_NORMAL, NULL, false);

		if (!opt0)
		{
			fbsql_error("\\%s: missing required argument\n", cmd);
			success = false;
		}
		else
		{
			describeObject(opt0);

			free(opt0);
		}
	}

	/* \g - execute command */
	else if (strncmp(cmd, "g", 1) == 0)
	{
		status = FBSQL_CMD_SEND;
	}

	/* \l - list database info */
	else if (strncmp(cmd, "l", 1) == 0)
	{
		listDatabaseInfo();
	}

	/* \plan - on|off|only */
	else if (strcmp(cmd, "plan") == 0)
	{
		char *opt0 = fbsql_scan_slash_option(scan_state,
											 OT_NORMAL, NULL, false);

		if (!opt0)
		{
			printf("Plan display is currently %s\n", render_plan_display(fset.plan_display));
		}
		else
		{
			do_plan_display(opt0);
		}

		free(opt0);
	}


	/* \format - set printing parameters */
	else if (strcmp(cmd, "format") == 0)
	{
		char *opt0 = fbsql_scan_slash_option(scan_state,
											 OT_NORMAL, NULL, false);
		char *opt1 = fbsql_scan_slash_option(scan_state,
											 OT_NORMAL, NULL, false);

		if (!opt0)
		{
			fbsql_error("\\%s: missing required argument\n", cmd);
			success = false;
		}
		else
			success = do_format(opt0, opt1, &fset.popt, fset.quiet);

		free(opt0);
		free(opt1);
	}

	/* \timing - toggle timing */
	else if (strncmp(cmd, "timing", 6) == 0)
	{
		if (fset.timing == false)
		{
			fset.timing = true;
			printf("Timing on\n");
		}
		else {
			fset.timing = false;
			printf("Timing off\n");
		}
	}
	/* \util - perform various utility functions */
	else if (strncmp(cmd, "util", 4) == 0)
	{
		char *opt0 = fbsql_scan_slash_option(scan_state,
											 OT_NORMAL, NULL, false);
		if (!opt0)
		{
			showUtilOptions();
		}
		else
		{
			execUtil(opt0);
		}

		free(opt0);
	}
	/* \test - misc dev tests, remove for release! */
	else if (strncmp(cmd, "test_ins", 8) == 0)
	{
		_command_test_ins();
	}
	/* \test - misc dev tests, remove for release! */
	else if (strncmp(cmd, "test", 4) == 0)
	{
		char *opt0 = fbsql_scan_slash_option(scan_state,
											 OT_NORMAL, NULL, false);
		_command_test(opt0);
	}
	else
	{
		status = FBSQL_CMD_UNKNOWN;
	}

	if (!success)
		status = FBSQL_CMD_ERROR;

	return status;
}


/**
 * do_format()
 *
 */
bool
do_format(const char *param, const char *value, printQueryOpt *popt, bool quiet)
{
	/* set output format */
	if (strcmp(param, "alignment") == 0)
	{
		if (!value)
			;
		else if (strcmp("unaligned", value) == 0)
			popt->topt.format = PRINT_UNALIGNED;
		else if (strcmp("aligned", value) == 0)
			popt->topt.format = PRINT_ALIGNED;
/*		else if (strcmp("wrapped", value) == 0)
			popt->topt.format = PRINT_WRAPPED;
		else if (strcmp("html", value) == 0)
		popt->topt.format = PRINT_HTML;*/
		else
		{
			printf("\\format alignment: allowed formats are unaligned, aligned");
			return false;
		}

		if (!quiet)
			printf("Alignment format is %s.\n", _align2string(popt->topt.format));
	}
	/* set border format */
	else if (strcmp(param, "border") == 0)
	{
		if (!value)
			;
		else if (strcmp("minimal", value) == 0)
		{
			popt->topt.border = BORDER_MINIMAL;
		}
		else if (strcmp("classic", value) == 0)
		{
			popt->topt.border = BORDER_CLASSIC;
		}
		else
		{
			printf("\\format border: allowed formats are minimal, classic\n");
			return false;
		}

		popt->topt.border_format = _getBorderFormat();

		if (!quiet)
			printf("Border format is \"%s\".\n", _border2string(popt->topt.border));

	}
	/* null display */
	else if (strcmp(param, "null") == 0)
	{
		if (value)
		{
			int value_len = strlen(value);

			free(popt->nullPrint);
			popt->nullPrint = malloc(value_len);
			strncpy(popt->nullPrint, value, value_len);
		}
		if (!quiet)
			printf("Null display is \"%s\".\n", popt->nullPrint ? popt->nullPrint : "");
	}

	return true;
}


bool
do_plan_display(const char *value)
{
	if (strcmp("off", value) == 0)
	{
		fset.plan_display = PLAN_DISPLAY_OFF;
	}
	else if (strcmp("on", value) == 0)
	{
		fset.plan_display = PLAN_DISPLAY_ON;
	}
	else if (strcmp("only", value) == 0)
	{
		fset.plan_display = PLAN_DISPLAY_ONLY;
	}
	else
	{
		printf("\\plan: allowed options are on, off, only\n");
		return false;
	}

	printf("Plan display is %s\n", render_plan_display(fset.plan_display));
	return true;
}


static char *
render_plan_display(short plan_display)
{
	switch(plan_display)
	{
		case PLAN_DISPLAY_ON:
			return "on";
		case PLAN_DISPLAY_ONLY:
			return "only";
		case PLAN_DISPLAY_OFF:
			return "off";
		default:
			return "unknown";
	}
}


static void
_wildcard_pattern_clause(char *pattern, char *field, FQExpBufferData *buf)
{
	size_t pattern_len = strlen(pattern);

	if (pattern_len == 1 && pattern[0] == '*')
	{
		/* do nothing for now */
	}
	else if (pattern_len && pattern[ pattern_len - 1 ] == '*')
	{
		char *like_pattern = malloc(pattern_len);

		strncpy(like_pattern, pattern, pattern_len);

		appendFQExpBuffer(buf,
						  "			 AND TRIM(LOWER(%s)) LIKE TRIM(LOWER('%s%%'))\n",
						  field,
						  like_pattern);

		free(like_pattern);
	}
	else
	{
		appendFQExpBuffer(buf,
						  "			 AND TRIM(LOWER(%s)) = TRIM(LOWER('%s'))\n",
						  field,
						  pattern);
	}
}


static const char *
_align2string(enum printFormat in)
{
	switch (in)
	{
		case PRINT_NOTHING:
			return "nothing";
		case PRINT_UNALIGNED:
			return "unaligned";
		case PRINT_ALIGNED:
			return "aligned";
		case PRINT_WRAPPED:
			return "wrapped";
		case PRINT_HTML:
			return "html";
	}

	return "unknown";
}


static const char *
_border2string(enum borderFormat in)
{
	switch (in)
	{
		case BORDER_MINIMAL:
			return "minimal";
		case BORDER_CLASSIC:
			/* XXX TODO: implement BORDER_BOX */
		case BORDER_BOX:
			return "classic";
	}

	return "unknown";
}


/**
 * showActivity()
 *
 * \activity
 */
static void
showActivity(void)
{
	char *query;
	printQueryOpt pqopt = fset.popt;

	pqopt.header = "Current activity";

	query =
"    SELECT TRIM(mon$user) AS \"User\",\n"
"           mon$timestamp AS \"Connection start\",\n"
"           mon$remote_address AS \"Client address\",\n"
"           COALESCE(mon$remote_process, '-') AS \"Client application\",\n"
"           TRIM(mon$role) AS \"Role\",\n"
"           mon$state AS \"State\",\n"
"           mon$server_pid AS \"Server PID\",\n"
"           mon$remote_pid AS \"Client PID\",\n"
"           TRIM(rdb$character_set_name) AS \"Client encoding\"\n"
"      FROM mon$attachments\n"
"INNER JOIN rdb$character_sets\n"
"        ON mon$character_set_id = rdb$character_set_id";

	commandExecPrint(query, &pqopt);
}


/**
 * showCopyright()
 *
 */
static void
showCopyright(void)
{
	printf("fbsql v%s (c) Copyright 2013-2018 Ian Barwick\n", FBSQL_VERSION);
}


/**
 * showUsage()
 *
 */
static void
showUsage(void)
{
	printf("General\n");
	printf("  \\copyright             Show fbsql copyright information\n");
	printf("  \\g or ;                execute query\n");
	printf("  \\q                     quit fbsql\n");
	printf("\n");

	printf("Display\n");
	printf("  \\a                     Toggle aligned mode (currently %s)\n",
		   fset.popt.topt.format == PRINT_ALIGNED ? "on" : "off");
	printf("  \\format OPTION [VALUE] Set or show table output formatting option:\n");
	printf("                           {alignment|border|null}\n");
	printf("  \\plan [SETTING]        Display plan {off|on|only} (currently %s)\n",
           render_plan_display(fset.plan_display));
	printf("  \\timing                Toggle excution timing (currently %s)\n",
           fset.timing ? "on" : "off");
	printf("\n");

	printf("Environment\n");
	printf("  \\activity              Show information about current database activity\n");
	printf("  \\conninfo              Show information about the current connection\n");
	printf("\n");

	printf("Database\n");
	printf("  (options: S = show system objects, + = additional detail)\n");
	printf("  \\l                     List information about the current database\n");
	printf("  \\autocommit            Toggle autocommit (currently %s)\n",
           fset.autocommit ? "on" : "off");
	printf("  \\d      NAME           List information about the specified object\n");
	printf("  \\df     [PATTERN]      List information about functions matching [PATTERN]\n");
	printf("  \\di[S+] [PATTERN]      List information about indexes matching [PATTERN]\n");
	printf("  \\dp     [PATTERN]      List information about procedures matching [PATTERN]\n");
	printf("  \\ds[S]  [PATTERN]      List information about sequences (generators) matching [PATTERN]\n");
	printf("  \\dt[S]  [PATTERN]      List information about tables matching [PATTERN]\n");
	printf("  \\du                    List users granted privileges on this database\n");
	printf("  \\dv     [PATTERN]      List information about views matching [PATTERN]\n");
	printf("  \\util   [COMMAND]      execute utility command\n");
	printf("                            {set_index_statistics}\n");
}


static
void showUtilOptions(void)
{
	puts("");
	printf("Options for \\util:\n");
	puts("");
	printf("  \\util set_index_statistics		 Set global index statistics\n");
	puts("");
}


/* \d NAME */
void
describeObject(char *name)
{
	char *type_query_tmpl =
" SELECT 't' AS objtype                                \n"
"   FROM rdb$relations                                 \n"
"  WHERE TRIM(LOWER(rdb$relation_name)) = LOWER('%s')  \n"
"    AND rdb$view_blr IS NULL                          \n"
"     UNION                                            \n"
" SELECT 'v' AS objtype                                \n"
"   FROM rdb$relations                                 \n"
"  WHERE TRIM(LOWER(rdb$relation_name)) = LOWER('%s')  \n"
"    AND rdb$view_blr IS NOT NULL                      \n"
"     UNION                                            \n"
" SELECT 'i' AS objtype                                \n"
"   FROM rdb$indices                                   \n"
"  WHERE TRIM(LOWER(rdb$index_name)) = LOWER('%s')     \n";

	char *type_query = malloc(strlen(type_query_tmpl) +
							  (strlen(name) * 3) +
							  1);

	char *type;
	FBresult   *query_result;

	/* get object's type */

	sprintf(type_query, type_query_tmpl, name, name, name);

	query_result = commandExec(type_query);

	free(type_query);

	if (query_result == NULL)
		return;

	if (FQntuples(query_result) == 0)
	{
		printf("No object found\n");
		return;
	}

	type = FQgetvalue(query_result, 0, 0);

	switch(type[0])
	{
		case 't':
			describeTable(name);
			break;
		case 'v':
			describeView(name);
			break;
		case 'i':
			describeIndex(name);
			break;
		default:
			printf("Unknown object type %c\n", type[0]);
	}

	FQclear(query_result);
}


/* Output object information from query */
void
_describeObject(char *name, char *object_type, char *query)
{
	printQueryOpt pqopt = fset.popt;

	char *format = "%s \"%s\"";
	pqopt.header = malloc(strlen(name) + strlen(object_type) + 4);

	sprintf(pqopt.header, format, object_type, name);

	commandExecPrint(query, &pqopt);

	free(pqopt.header);
}


/**
 * describeTable()
 *
 * \d table_name
 */
void
describeTable(char *name)
{
	FBresult   *query_result;
	FQExpBufferData buf;
	initFQExpBuffer(&buf);

	appendFQExpBuffer(&buf,
"  SELECT TRIM(LOWER(rf.rdb$field_name))  AS \"Column\", \n"
        );

	appendFQExpBuffer(&buf, _sqlFieldType());

	appendFQExpBuffer(&buf,
"         CASE WHEN rf.rdb$null_flag <> 0 THEN TRIM('NOT NULL') ELSE '' END AS \"Modifiers\", \n"
"         COALESCE(CAST(rf.rdb$default_source AS VARCHAR(80)), '') \n"
"           AS \"Default value\", \n"
"         COALESCE(CAST(rf.rdb$description AS VARCHAR(80)), '') \n"
"           AS \"Description\" \n"
"      FROM rdb$relation_fields rf \n"
" LEFT JOIN rdb$fields f \n"
"        ON rf.rdb$field_source = f.rdb$field_name\n"
"     WHERE TRIM(LOWER(rf.rdb$relation_name)) = LOWER('%s')\n"
"  ORDER BY rf.rdb$field_position\n",
                    name
        );

	_describeObject(name, "Table", buf.data);

	termFQExpBuffer(&buf);

	/* List indexes */
	initFQExpBuffer(&buf);
	appendFQExpBuffer(&buf,
"    SELECT LOWER(TRIM(i.rdb$index_name)) AS index_name, \n"
"           TRIM(COALESCE(rc.rdb$constraint_type,'')) AS constraint_type \n"
"      FROM rdb$indices i \n"
" LEFT JOIN rdb$relation_constraints rc \n"
"        ON (rc.rdb$index_name = i.rdb$index_name) \n"
"     WHERE LOWER(i.rdb$relation_name)='%s' \n"
"       AND i.rdb$foreign_key IS NULL \n",
                      name
		);

	query_result = commandExec(buf.data);
	termFQExpBuffer(&buf);

	if (FQresultStatus(query_result) == FBRES_TUPLES_OK && FQntuples(query_result) > 0)
	{
		int row = 0;

		puts("Indexes:");

		for(row = 0; row < FQntuples(query_result); row++)
		{
			char *index_name;
			char *primary_key;
			char *index_segments;

			index_name = FQgetvalue(query_result, row, 0);
			index_segments = _listIndexSegments(index_name);

			printf("  %s",
				   index_name
				);

			primary_key = FQgetvalue(query_result, row, 1);
			if (strlen(primary_key))
			{
				printf(" %s", primary_key);
			}

			printf(" (%s)",
				   index_segments
				);

			puts("");
		}
	}

	FQclear(query_result);

	/* List foreign key constraints */
	/* TODO: show associated index name? See FB book p300 */

	initFQExpBuffer(&buf);
	appendFQExpBuffer(&buf,
"    SELECT LOWER(TRIM(from_table.rdb$index_name)) AS index_name, \n"
"           LOWER(TRIM(from_field.rdb$field_name)) AS from_field, \n"
"           LOWER(TRIM(to_table.rdb$relation_name)) AS to_table, \n"
"           LOWER(TRIM(to_field.rdb$field_name)) AS to_field, \n"
"           TRIM(refc.rdb$update_rule) AS on_update, \n"
"           TRIM(refc.rdb$delete_rule) AS on_delete, \n"
"           TRIM(rc.rdb$deferrable) AS is_deferrable, \n"
"           TRIM(rc.rdb$initially_deferred) AS is_deferred \n"
"      FROM rdb$indices from_table  \n"
"INNER JOIN rdb$index_segments from_field \n"
"        ON from_field.rdb$index_name = from_table.rdb$index_name \n"
"INNER JOIN rdb$indices to_table  \n"
"        ON to_table.rdb$index_name = from_table.rdb$foreign_key \n"
"INNER JOIN rdb$index_segments to_field  \n"
"        ON to_table.rdb$index_name = to_field.rdb$index_name \n"
" LEFT JOIN rdb$relation_constraints rc \n"
"        ON rc.rdb$index_name = from_table.rdb$index_name \n"
" LEFT JOIN rdb$ref_constraints refc \n"
"        ON rc.rdb$constraint_name = refc.rdb$constraint_name\n"
"     WHERE LOWER(from_table.rdb$relation_name) = '%s' \n"
"       AND from_table.rdb$foreign_key IS NOT NULL \n",
					  name
		);

	query_result = commandExec(buf.data);
	termFQExpBuffer(&buf);

	if (FQresultStatus(query_result) == FBRES_TUPLES_OK && FQntuples(query_result) > 0)
	{
		int row = 0;

		printf("Foreign keys:\n");
		for(row = 0; row < FQntuples(query_result); row++)
		{
			char *fk_action;
			printf("  %s FOREIGN KEY (%s) REFERENCES %s (%s)",
				   FQgetvalue(query_result, row, 0),
				   FQgetvalue(query_result, row, 1),
				   FQgetvalue(query_result, row, 2),
				   FQgetvalue(query_result, row, 3)
				);

			/* ON UPDATE? */
			fk_action = FQgetvalue(query_result, row, 4);
			if (strncmp(fk_action, "NO ACTION", 9) != 0)
			{
				printf(" ON UPDATE %s",
					   fk_action
					);
			}

			/* ON DELETE? */
			fk_action = FQgetvalue(query_result, row, 5);
			if (strncmp(fk_action, "NO ACTION", 9) != 0)
			{
				printf(" ON DELETE %s",
					   fk_action
					);
			}

			/* not sure if DEFERRABLE is actually supported */
			fk_action = FQgetvalue(query_result, row, 6);
			if (strncmp(fk_action, "YES", 3) == 0)
			{
				puts(" DEFERRABLE");
			}

			/* not sure if IS DEFERRED is actually supported */
			fk_action = FQgetvalue(query_result, row, 7);
			if (strncmp(fk_action, "YES", 3) == 0)
			{
				puts(" IS DEFERRED");
			}
			puts("");
		}
	}

	FQclear(query_result);


	/* List triggers */
	/* TODO:
	 *	- find out what trigger_sequence / trigger_flags mean
	 *	- what are the non-system triggers?
	 */

	initFQExpBuffer(&buf);
	appendFQExpBuffer(&buf,
"    SELECT LOWER(TRIM(t.rdb$trigger_name)) AS trigger_name, \n"
"           CASE t.rdb$trigger_type \n"
"             WHEN 1 THEN TRIM('BEFORE INSERT') \n"
"             WHEN 2 THEN TRIM('AFTER INSERT') \n"
"             WHEN 3 THEN TRIM('BEFORE UPDATE') \n"
"             WHEN 4 THEN TRIM('AFTER UPDATE') \n"
"             WHEN 5 THEN TRIM('BEFORE DELETE') \n"
"             WHEN 6 THEN TRIM('AFTER DELETE') \n"
"           END AS trigger_type, \n"
"           t.rdb$trigger_sequence AS trigger_sequence, \n"
"           t.rdb$flags AS trigger_flags, \n"
"           CASE t.rdb$trigger_inactive \n"
"             WHEN 1 THEN 0 ELSE 1 \n"
"           END AS trigger_inactive \n"
"      FROM rdb$triggers t \n"
"     WHERE TRIM(LOWER(t.rdb$relation_name)) = '%s' \n"
"       AND t.rdb$system_flag = 0 \n"
"  ORDER BY t.rdb$trigger_name\n",
					  name
		);

	query_result = commandExec(buf.data);
	termFQExpBuffer(&buf);

	if (FQresultStatus(query_result) == FBRES_TUPLES_OK && FQntuples(query_result) > 0)
	{
		int row = 0;

		puts("");
		printf("Triggers:\n");
		for(row = 0; row < FQntuples(query_result); row++)
		{
			printf("  %s: %s (%s)",
				   FQgetvalue(query_result, row, 0),
				   FQgetvalue(query_result, row, 1),
				   FQgetvalue(query_result, row, 5) ? "inactive" : "active"
				);

			puts("");
		}
	}


	FQclear(query_result);

	puts("");
}


/* \di */
void
describeIndex(char *name)
{
	FBresult   *query_result;
	FQExpBufferData buf;

	/* Display field information */

	initFQExpBuffer(&buf);
	appendFQExpBuffer(&buf,
"    SELECT TRIM(LOWER(isg.rdb$field_name)) AS \"Column\", \n"
        );

	appendFQExpBuffer(&buf, _sqlFieldType());

	appendFQExpBuffer(&buf,
"           isg.rdb$statistics AS \"Statistics\" \n"
"      FROM rdb$indices i  \n"
"INNER JOIN rdb$index_segments isg  \n"
"        ON isg.rdb$index_name = i.rdb$index_name \n"
"INNER JOIN rdb$relation_fields rf \n"
"        ON (rf.rdb$relation_name  = i.rdb$relation_name \n"
"            AND isg.rdb$field_name = rf.rdb$field_name) \n"
" LEFT JOIN rdb$fields f \n"
"        ON rf.rdb$field_source = f.rdb$field_name \n"
"     WHERE LOWER(TRIM(i.rdb$index_name)) = '%s' \n"
"  ORDER BY isg.rdb$field_position \n",
					  name
		);

	_describeObject(name, "Index", buf.data);

	termFQExpBuffer(&buf);

	initFQExpBuffer(&buf);

	/* Display meta-information */

	appendFQExpBuffer(&buf,
"      SELECT TRIM(LOWER(i.rdb$relation_name)) AS table_name,\n"
"             COALESCE(CAST(i.rdb$description AS VARCHAR(80)), '') AS description\n"
"        FROM rdb$indices i\n"
"   LEFT JOIN rdb$relation_constraints r\n"
"          ON r.rdb$index_name = i.rdb$index_name\n"
"       WHERE LOWER(i.rdb$index_name) = LOWER('%s')\n",
					  name
		);

	query_result = commandExec(buf.data);
	termFQExpBuffer(&buf);

	if (FQresultStatus(query_result) == FBRES_TUPLES_OK && FQntuples(query_result) > 0)
	{
		char *description;

		printf("  Table: %s\n",
			   FQgetvalue(query_result, 0, 0)
			);

		description = FQgetvalue(query_result, 0, 1);
		if (strlen(description))
			printf("  Description: %s\n",
				   description
				);
	}

	FQclear(query_result);
}


/* \ds */
void
describeSequence(char *name)
{
	puts("\\ds not yet implemented");
}


/* \dv  */
void
describeView(char *name)
{
	FQExpBufferData buf;
	initFQExpBuffer(&buf);

	appendFQExpBuffer(&buf,
"    SELECT TRIM(LOWER(r.rdb$field_name)) \n"
"             AS \"Column\", \n"
		);

	appendFQExpBuffer(&buf, _sqlFieldType());

	appendFQExpBuffer(&buf,
"           COALESCE(CAST(r.rdb$description AS VARCHAR(80)), '') \n"
"             AS \"Description\" \n"
"      FROM rdb$relation_fields r\n"
" LEFT JOIN rdb$fields f\n"
"        ON r.rdb$field_source = f.rdb$field_name\n"
"     WHERE TRIM(LOWER(rdb$relation_name)) = LOWER('%s')\n"
"  ORDER BY rdb$field_position\n",
					name
		);

	_describeObject(name, "View", buf.data);

	termFQExpBuffer(&buf);
}


/* \l */
static void
listDatabaseInfo(void)
{
	printQueryOpt pqopt = fset.popt;

	pqopt.header = "Database information";

	char * query = \
" SELECT mon$database_name AS \"Name\", \n"
"        mon$sql_dialect   AS \"SQL Dialect\", \n"
"        mon$creation_date AS \"Creation Date\", \n"
"        mon$pages * mon$page_size AS \"Size (bytes)\", \n"
"        TRIM(rdb$character_set_name) AS \"Encoding\", \n"
"        COALESCE(CAST(rdb$description AS VARCHAR(80)), '') AS \"Description\" \n"
"   FROM mon$database, rdb$database\n";

	commandExecPrint(query, &pqopt);
}


/* \util [command] */
static bool
execUtil(char *command)
{
	if (strncmp(command, "set_index_statistics", 20) == 0)
		return _execUtilSetIndexStatistics();

	printf("Unknown \\util option \"%s\"\n",
		   command);

	return false;
}


/* \util set_index_statistics */
static bool
_execUtilSetIndexStatistics(void)
{
	FBresult *res;
	char *query = \
"EXECUTE BLOCK AS \n"
"  DECLARE VARIABLE index_name VARCHAR(31); \n"
"BEGIN \n"
"  FOR SELECT rdb$index_name FROM rdb$indices INTO :index_name DO \n"
"    EXECUTE STATEMENT 'SET statistics INDEX ' || :index_name || ';'; \n"
"END \n";

	res = FQexec(fset.conn, query);
	if (FQresultStatus(res) != FBRES_COMMAND_OK)
	{
		FQclear(res);
		fbsql_error("error updating index statistics\n");
		return false;
	}

	FQclear(res);

	puts("Index statistics updated");

	return true;
}


/* \df */
static void
listFunctions(char *pattern)
{
	FQExpBufferData buf;
	printQueryOpt pqopt = fset.popt;

	pqopt.header = "List of functions";
	initFQExpBuffer(&buf);

	appendFQExpBuffer(&buf,
"   SELECT TRIM(LOWER(rdb$function_name)) AS \"Name\", \n"
"          TRIM(rdb$module_name) AS \"Module\", \n"
"          COALESCE(CAST(rdb$description AS VARCHAR(80)), '') AS \"Description\"  \n"
"     FROM rdb$functions  \n"
"    WHERE rdb$system_flag = 0 \n"
		);

	if (pattern != NULL)
	{
		_wildcard_pattern_clause(pattern, "rdb$function_name", &buf);
	}

	appendFQExpBuffer(&buf,
"  ORDER BY 1"
		);

	commandExecPrint(buf.data, &pqopt);

	termFQExpBuffer(&buf);
}


/* \di */

static void
listIndexes(char *pattern, bool show_system, bool show_extended)
{
	FQExpBufferData buf;
	printQueryOpt pqopt = fset.popt;

	pqopt.header = "List of indexes";
	initFQExpBuffer(&buf);

	appendFQExpBuffer(&buf,
"   SELECT TRIM(LOWER(rdb$index_name)) AS \"Name\", \n"
"          TRIM(LOWER(rdb$relation_name)) AS \"Table\",  \n"
"          COALESCE(CAST(rdb$description AS VARCHAR(80)), '') AS \"Description\"  \n"
		);

	if (show_extended == true)
		appendFQExpBuffer(&buf,
"        , rdb$statistics AS \"Statistics\" \n"
			);

	appendFQExpBuffer(&buf,
"     FROM rdb$indices  \n"
"    WHERE 1 = 1"
		);

	if (pattern != NULL)
		_wildcard_pattern_clause(pattern, "rdb$index_name", &buf);
	else if (show_system == false)
		appendFQExpBuffer(&buf,
"      AND rdb$system_flag = 0 \n"
			);

	appendFQExpBuffer(&buf,
					  " ORDER BY 1"
		);

	commandExecPrint(buf.data, &pqopt);

	termFQExpBuffer(&buf);
}


/* \dp */
static void
listProcedures(char *pattern)
{
	FQExpBufferData buf;
	printQueryOpt pqopt = fset.popt;

	pqopt.header = "List of procedures";
	initFQExpBuffer(&buf);

	appendFQExpBuffer(&buf,
"   SELECT TRIM(LOWER(rdb$procedure_name)) AS \"Name\", \n"
"          rdb$procedure_id AS \"Id\", \n"
"          TRIM(LOWER(rdb$owner_name)) AS \"Owner\", \n"
"          CASE rdb$procedure_type \n"
"            WHEN 1 THEN TRIM('Selectable') \n"
"            WHEN 2 THEN TRIM('Executable') \n"
"            ELSE TRIM('Legacy') \n"
"          END AS \"Type\", \n"
"          COALESCE(CAST(rdb$description AS VARCHAR(80)), '') AS \"Description\"  \n"
"     FROM rdb$procedures  \n"
"    WHERE 1 = 1\n"
		);

	if (pattern != NULL)
		_wildcard_pattern_clause(pattern, "rdb$procedure_name", &buf);

	appendFQExpBuffer(&buf,
"  ORDER BY 1"
		);

	commandExecPrint(buf.data, &pqopt);

	termFQExpBuffer(&buf);
}


/* \ds */
static void
listSequences(char *pattern, bool show_system)
{
	FQExpBufferData buf;
	printQueryOpt pqopt = fset.popt;

	pqopt.header = "List of sequences";
	initFQExpBuffer(&buf);

	appendFQExpBuffer(&buf,
"   SELECT TRIM(LOWER(rdb$generator_name)) AS \"Name\", \n"
"          rdb$generator_id AS \"Id\", \n"
"          COALESCE(CAST(rdb$description AS VARCHAR(80)), '') AS \"Description\"  \n"
"     FROM rdb$generators  \n"
"    WHERE 1 = 1\n"
		);

	if (pattern != NULL)
		_wildcard_pattern_clause(pattern, "rdb$generator_name", &buf);
	else if (show_system == false)
		appendFQExpBuffer(&buf,
"      AND rdb$system_flag = 0\n"
			);

	appendFQExpBuffer(&buf,
"  ORDER BY 1"
		);

	commandExecPrint(buf.data, &pqopt);

	termFQExpBuffer(&buf);
}


/* \dt */
static void
listTables(char *pattern, bool show_system)
{
	FQExpBufferData buf;
	printQueryOpt pqopt = fset.popt;

	pqopt.header = "List of tables";
	initFQExpBuffer(&buf);

	appendFQExpBuffer(&buf,
"   SELECT TRIM(LOWER(rdb$relation_name)) AS \"Name\", \n"
"          TRIM(LOWER(rdb$owner_name)) AS \"Owner\",  \n"
"          COALESCE(CAST(rdb$description AS VARCHAR(80)), '') AS \"Description\"  \n"
"     FROM rdb$relations  \n"
"    WHERE rdb$view_blr IS NULL \n"
		);


	if (pattern != NULL)
		_wildcard_pattern_clause(pattern, "rdb$relation_name", &buf);
	else if (show_system == false)
		appendFQExpBuffer(&buf,
"      AND rdb$system_flag = 0\n"
			);

	appendFQExpBuffer(&buf,
"    ORDER BY 1"
		);

	commandExecPrint(buf.data, &pqopt);

	termFQExpBuffer(&buf);
}


/* \du */
static void
listUsers(void)
{
	FQExpBufferData buf;
	printQueryOpt pqopt = fset.popt;

	pqopt.header = "List of users";
	initFQExpBuffer(&buf);

	appendFQExpBuffer(&buf,
"    SELECT DISTINCT TRIM(rdb$user) AS \"User\"\n"
"      FROM rdb$user_privileges\n"
"  ORDER BY 1"
		);

	commandExecPrint(buf.data, &pqopt);

	termFQExpBuffer(&buf);
}


/* \dv */
static void
listViews(char *pattern)
{
	FQExpBufferData buf;
	printQueryOpt pqopt = fset.popt;

	pqopt.header = "List of views";
	initFQExpBuffer(&buf);

	appendFQExpBuffer(&buf,
"   SELECT TRIM(LOWER(rdb$relation_name)) AS \"Name\", \n"
"          TRIM(LOWER(rdb$owner_name)) AS \"Owner\",  \n"
"          COALESCE(CAST(rdb$description AS VARCHAR(80)), '') AS \"Description\"  \n"
"     FROM rdb$relations  \n"
"    WHERE rdb$view_blr IS NOT NULL  \n"
		);

	if (pattern != NULL)
		_wildcard_pattern_clause(pattern, "rdb$relation_name", &buf);

	appendFQExpBuffer(&buf,
					  " ORDER BY 1"
		);

	commandExecPrint(buf.data, &pqopt);

	termFQExpBuffer(&buf);
}


static char *
_listIndexSegments(char *index_name)
{
	FBresult   *query_result;
	FQExpBufferData buf;
	char *result;

	initFQExpBuffer(&buf);
	appendFQExpBuffer(&buf,
"    SELECT TRIM(LOWER(rdb$field_name)) AS field_name \n"
"      FROM rdb$index_segments \n"
"     WHERE LOWER(TRIM(rdb$index_name)) = '%s' \n"
"  ORDER BY rdb$field_position \n",
					  index_name
		);

	query_result = commandExec(buf.data);
	termFQExpBuffer(&buf);

	initFQExpBuffer(&buf);

	if (FQresultStatus(query_result) == FBRES_TUPLES_OK && FQntuples(query_result) > 0)
	{
		int row = 0;
		for(row = 0; row < FQntuples(query_result); row++)
		{
			if (row)
			{
				appendFQExpBuffer(&buf, ", ");
			}

			appendFQExpBuffer(&buf,
							  FQgetvalue(query_result, row, 0));
		}
	}

	FQclear(query_result);

	result = malloc(strlen(buf.data) + 1);
	strncpy(result, buf.data, strlen(buf.data) + 1);
	termFQExpBuffer(&buf);

	return result;
}


/**
 * _sqlFieldType()
 *
 *
 */
static char *
_sqlFieldType(void)
{
	char *sqlFieldType =
"          CASE f.rdb$field_type\n"
"            WHEN 261 THEN 'BLOB'\n"
"            WHEN 14  THEN 'CHAR(' || f.rdb$field_length|| ')'\n"
"            WHEN 40  THEN 'CSTRING'\n"
"            WHEN 11  THEN 'D_FLOAT'\n"
"            WHEN 27  THEN 'DOUBLE'\n"
"            WHEN 10  THEN 'FLOAT'\n"
"            WHEN 16  THEN \n"
"              CASE f.rdb$field_sub_type \n"
"                WHEN 1 THEN 'NUMERIC(' || f.rdb$field_precision || ',' || (-f.rdb$field_scale) || ')' \n"
"                WHEN 2 THEN 'DECIMAL(' || f.rdb$field_precision || ',' || (-f.rdb$field_scale) || ')' \n"
"               ELSE 'BIGINT' \n"
"              END \n"
"            WHEN 8   THEN \n"
"              CASE f.rdb$field_sub_type \n"
"                WHEN 1 THEN 'NUMERIC(' || f.rdb$field_precision || ',' || (-f.rdb$field_scale) || ')' \n"
"                WHEN 2 THEN 'DECIMAL(' || f.rdb$field_precision || ',' || (-f.rdb$field_scale) || ')' \n"
"                ELSE 'INTEGER' \n"
"              END \n"
"            WHEN 9   THEN 'QUAD'\n"
"            WHEN 7   THEN \n"
"              CASE f.rdb$field_sub_type \n"
"                WHEN 1 THEN 'NUMERIC(' || f.rdb$field_precision || ',' || (-f.rdb$field_scale) || ')' \n"
"                WHEN 2 THEN 'DECIMAL(' || f.rdb$field_precision || ',' || (-f.rdb$field_scale) || ')' \n"
"                ELSE 'SMALLINT' \n"
"              END \n"
"            WHEN 12  THEN 'DATE'\n"
"            WHEN 13  THEN 'TIME'\n"
"            WHEN 35  THEN 'TIMESTAMP'\n"
"            WHEN 37  THEN 'VARCHAR(' || f.rdb$field_length|| ')'\n"
"            ELSE 'UNKNOWN'\n"
"          END AS \"Field type\",\n";

	return sqlFieldType;
}


void
_command_test_ins()
{
	FBresult	  *result;
	const char *paramValues[2];

	paramValues[0] = "99";
	paramValues[1] = "2041/01/01 01:14:33.1234";
	//paramValues[2] = NULL;

	result = FQexecParams(
		fset.conn,
		"INSERT INTO ts_test (id, ts) VALUES(?,?)",
		//"INSERT INTO module (module_name, module_id) VALUES(?,?)",
		//"INSERT INTO module (module_id, module_position) VALUES(?,?)",
		//"INSERT INTO foo (v1, v2) VALUES(?,?)",
		2,
		NULL,
		paramValues,
		NULL,
		NULL,
		0);
}


/* \test */
void
_command_test(const char *param)
{
	struct tm tm;
	if (strptime(param, "%Y-%m-%d_%H:%M:%S", &tm) == NULL)
		puts("error");
	else
		puts("OK");

	printf("year: %i; month: %i; day: %i;\n",
		tm.tm_year, tm.tm_mon, tm.tm_mday);
}


/* \test */
void
_command_test_param(char *param)
{
	FBresult	  *result;
	printQueryOpt pqopt = fset.popt;
	const char *paramValues[2];
	//const char **paramValues;

	char intbuf[10];

	char *q1 = "SELECT * FROM language WHERE lang_id != CAST(? AS VARCHAR(%s))";
	char *cast_varchar = (char *)malloc(strlen(q1) + strlen(intbuf));

	const char *db_key_sql = "SELECT rdb$db_key FROM language WHERE lang_id='en'";
	char *db_key;
	FBresult *query_result;

	const int paramFormats[2] = { 0, -1 };

	query_result = FQexec(fset.conn, db_key_sql);
	FQlog(fset.conn, DEBUG1, "key %s", FQformatDbKey(query_result, 0, 0));
	db_key = FQformatDbKey(query_result, 0, 0);

	paramValues[0] = "en";

	paramValues[1] = db_key;

	result = FQexecParams(
		fset.conn,
		"SELECT * FROM language WHERE lang_id=? ",
		//cast_varchar,
		//"SELECT * FROM language WHERE name_english != ?",
		//"UPDATE language SET name_native=? WHERE lang_id='en'",
		//"UPDATE language SET name_native=? WHERE RDB$DB_KEY = ?",
		//"SELECT * FROM language WHERE rdb$db_key = ?",
		1,
		NULL,
		paramValues,
		NULL,
		NULL,//paramFormats,
		0
		);

	free(cast_varchar);

	if (FQresultStatus(result) == FBRES_FATAL_ERROR)
	{
		char *dump = FQresultErrorFieldsAsString(result, "-");

		printf("%s\n", FQresultErrorMessage(result));
		printf("%s\n", dump);
		free(dump);
	}
	printf("%i rows returned\n", FQntuples(result));

	printQuery(result, &pqopt);
	FQclear(result);
}

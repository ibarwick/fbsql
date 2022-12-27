/* ---------------------------------------------------------------------
 *
 * command_test.c
 *
 * Ad-hoc testing code triggered by \test* slash commands
 *
 * Keeping it separate to avoid polluting command.c with miscellaneous
 * changes.
 *
 * ---------------------------------------------------------------------
 */

#define _XOPEN_SOURCE
#include <time.h>

#include "libfq.h"

#include "fbsql.h"
#include "query.h"
#include "settings.h"

/* \test */
void
_command_test(const char *param)
{
	struct tm tm;

	if (param == NULL)
		return;

	if (strptime(param, "%Y-%m-%d_%H:%M:%S", &tm) == NULL)
		puts("error");
	else
		puts("OK");

	printf("year: %i; month: %i; day: %i;\n",
		tm.tm_year, tm.tm_mon, tm.tm_mday);
}


void
_command_test_ins()
{
	FBresult	  *result;
	const char *paramValues[2];
	const char *paramValues2[2];

	paramValues[0] = "99";
	paramValues[1] = "2041/01/01 01:14:33.1234";

	paramValues2[0] = "1";
	paramValues2[1] = "2022/02/22 22:22:22.2222";

/*	result = FQprepare(fset.conn,
							 "INSERT INTO ts_test (id, ts) VALUES(?,?)",
							 2);

	result = FQexecPrepared(fset.conn,
							result,
							2,
							NULL,
							paramValues,
							NULL,
							NULL,
							0);
	result = FQexecParamsPrepared(fset.conn,
								  result,
								  2,
								  NULL,
								  paramValues2,
								  NULL,
								  NULL,
								  0);*/
	/*
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
		0);*/

	FQclear(result);
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

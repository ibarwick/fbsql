#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "libfq.h"
#include "fbsql.h"
#include "fbsqlscan.h"
#include "settings.h"
#include "common.h"
/*
 * Global fbsql options
 */
fbsqlSettings fset;

int
main(int argc, char *argv[])
{
	FbsqlScanState scan_state;	/* lexer working state */
	FbsqlScanResult scan_result;

	char prompt_tmp[100];
	volatile FQExpBuffer query_buf;
	char *stmt = "BEGIN;";
	
	query_buf = createFQExpBuffer();
	init_settings();

	scan_state = fbsql_scan_create();
	fbsql_scan_setup(scan_state, stmt, strlen(stmt));

	printf("hello world\n");
	// Here we need the scanner/parser to return some kind of status
	// which indicates pseudo-SQL command
	scan_result = fbsql_scan(scan_state, query_buf, &prompt_tmp);
	printf("goodbye world\n");


	fbsql_scan_finish(scan_state);
}

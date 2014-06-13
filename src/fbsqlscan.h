#ifndef FBSQLSCAN_H
#define FBSQLSCAN_H


#include "libfq.h"

/* Abstract type for lexer's internal state */
typedef struct FbsqlScanStateData *FbsqlScanState;

/* Termination states for fbsql_scan() */
typedef enum
{
	FSCAN_SEMICOLON,			/* found command-ending semicolon */
	FSCAN_BACKSLASH,			/* found backslash command */
	FSCAN_INCOMPLETE,			/* end of line, SQL statement incomplete */
	FSCAN_EOL					/* end of line, SQL possibly complete */
} FbsqlScanResult;

/* Different ways for scan_slash_option to handle parameter words */
enum slash_option_type
{
	OT_NORMAL,					/* normal case */
	OT_SQLID,					/* treat as SQL identifier */
	OT_SQLIDHACK,				/* SQL identifier, but don't downcase */
	OT_FILEPIPE,				/* it's a filename or pipe */
	OT_WHOLE_LINE,				/* just snarf the rest of the line */
	OT_NO_EVAL					/* no expansion of backticks or variables */
};

extern FbsqlScanState fbsql_scan_create(char *term);
extern void fbsql_scan_destroy(FbsqlScanState state);

extern void fbsql_scan_setup(FbsqlScanState state,
				const char *line, int line_len);
extern void fbsql_scan_finish(FbsqlScanState state);

extern FbsqlScanResult fbsql_scan(FbsqlScanState state,
		  FQExpBuffer query_buf,
		  char *prompt);

extern void fbsql_scan_reset(FbsqlScanState state);

extern bool fbsql_scan_in_quote(FbsqlScanState state);

extern char *fbsql_scan_slash_command(FbsqlScanState state);

extern char *fbsql_scan_slash_option(FbsqlScanState state,
					   enum slash_option_type type,
					   char *quote,
					   bool semicolon);

extern void fbsql_scan_slash_command_end(FbsqlScanState state);



#endif   /* FBSQLSCAN_H */

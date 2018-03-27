#ifndef COMMAND_H
#define COMMAND_H

#include "settings.h"
#include "fbsqlscan.h"

typedef enum _backslashResult
{
	FBSQL_CMD_UNKNOWN = 0,	  /* internal only status implying parsing incomplete */
	FBSQL_CMD_SEND,			  /* query complete; send off */
	FBSQL_CMD_SKIP_LINE,	  /* keep building query */
	FBSQL_CMD_TERMINATE,	  /* quit program */
	FBSQL_CMD_NEWEDIT,		  /* query buffer was changed (e.g., via \e) */
	FBSQL_CMD_ERROR			  /* the execution of the backslash command
							   * resulted in an error */
} backslashResult;

extern backslashResult HandleSlashCmds(FbsqlScanState scan_state,
									   FQExpBuffer query_buf);

static backslashResult
execSlashCommand(const char *cmd,
				 FbsqlScanState scan_state,
				 FQExpBuffer query_buf);

extern bool
do_format(const char *param,
		  const char *value,
		  printQueryOpt *popt,
		  bool quiet);

#endif   /* COMMAND_H */

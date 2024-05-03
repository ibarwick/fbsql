#ifndef SETTINGS_H
#define SETTINGS_H

#define FBSQL_HISTORY ".fbsql_history"
#include "libfq.h"

enum printFormat
{
	PRINT_NOTHING = 0,
	PRINT_UNALIGNED,
	PRINT_ALIGNED,
	PRINT_WRAPPED,
	PRINT_HTML
};

enum borderFormat
{
	BORDER_MINIMAL = 0,	 /* "psql" style */
	BORDER_BOX,			 /* "mysql" style */
	BORDER_CLASSIC		 /* "isql/sqlplus" style */
};

enum planDisplayOption
{
	PLAN_DISPLAY_OFF = 0,
	PLAN_DISPLAY_ON,
	PLAN_DISPLAY_ONLY
};

typedef struct printTextFormat
{
	/* A complete line style */
	const char *name;			/* for display purposes */
	const char *divider;
	const char *junction;
	const char *header_underline;
	bool padding;
} printTextFormat;


typedef struct printTableOpt
{
	enum printFormat format;
	enum borderFormat border;
	const printTextFormat *border_format;
} printTableOpt;


typedef struct printQueryOpt
{
	printTableOpt topt;			/* the options above */
	char		  *nullPrint;	/* how to print null entities */
	char		  *header;		/* optional table header */
} printQueryOpt;


typedef enum
{
	hctl_none = 0,
	hctl_ignorespace = 1,
	hctl_ignoredups = 2,
	hctl_ignoreboth = hctl_ignorespace | hctl_ignoredups
} HistControl;


typedef struct _fbsqlSettings
{
	FBconn			 *conn;
	char			 *sversion;
	char			 *dbpath;
	char			 *username;
	char			 *password;
	char			 *client_encoding;
	int				  client_encoding_id; /* corresponds to MON$ATTACHMENTS.MON$CHARACTER_SET_ID */
	bool			  time_zone_names;    /* instructs libfq to display time zone names if available */
	char			 *home_path;
	char			 *fbsql_history;
	printQueryOpt	  popt;
	bool			  timing;			  /* toggle timing display */
	bool			  quiet;
	bool			  lc_fold;			  /* fold column headings to lower-case? */
	bool			  echo_hidden;
	bool			  autocommit;
	short			  plan_display;		  /* display query plan? */
	HistControl		  histcontrol;
} fbsqlSettings;

extern fbsqlSettings fset;



#endif /* SETTINGS_H */

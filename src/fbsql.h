#ifndef FBSQL_H
#define FBSQL_H

#include <setjmp.h>

#define FBSQL_VERSION "0.2.0"



#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#define EXIT_BADCONN 2

#define EXIT_USER 3
typedef enum _promptStatus
{
	PROMPT_READY,
	PROMPT_CONTINUE,
	PROMPT_COMMENT,
	PROMPT_SINGLEQUOTE,
	PROMPT_DOUBLEQUOTE,
	PROMPT_DOLLARQUOTE,
	PROMPT_PAREN,
	PROMPT_COPY
} promptStatus_t;

#define DEVNULL "/dev/null"

#endif /* FBSQL_H */



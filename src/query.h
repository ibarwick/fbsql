#ifndef QUERY_H
#define QUERY_H

#include "fbsql.h"
#include "settings.h"

typedef struct timeval query_time;

extern bool
SendQuery(const char *query);

extern void
printQuery(const FBresult *query_result, const printQueryOpt *pqopt);

#define INSTR_TIME_SUBTRACT(x,y) \
	do { \
		(x).tv_sec -= (y).tv_sec; \
		(x).tv_usec -= (y).tv_usec; \
		/* Normalize */ \
		while ((x).tv_usec < 0) \
		{ \
			(x).tv_usec += 1000000; \
			(x).tv_sec--; \
		} \
	} while (0)


#define INSTR_TIME_GET_MILLISEC(t) \
	(((double) (t).tv_sec * 1000.0) + ((double) (t).tv_usec) / 1000.0)

#endif   /* QUERY_H */

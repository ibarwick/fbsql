/* ---------------------------------------------------------------------
 *
 * tab-complete.c
 *
 * Handles libreadline tab-completion
 *
 * ---------------------------------------------------------------------
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <readline/readline.h>

#include "libfq.h"
#include "fbsql.h"
#include "common.h"
#include "settings.h"
#include "port.h"


static char **fbsql_completion(char *text, int start, int end);

static char *complete_from_list(const char *text, int state);
static char *complete_from_const(const char *text, int state);
static char *complete_from_query(const char *text, int state);

static char *alter_keyword_generator(const char *text, int state);
static char *create_or_drop_keyword_generator(const char *text, int state);

static void get_previous_words(int point, char **previous_words, int nwords);
static char *fb_strdup_keyword_case(const char *s, const char *ref);

#define Query_for_list_of_attributes \
"   SELECT TRIM(LOWER(rdb$field_name)) "\
"     FROM rdb$relation_fields "\
"    WHERE SUBSTRING(LOWER(rdb$field_name) FROM 1 FOR %i) = '%s' "\
"      AND LOWER(rdb$relation_name) = '%s' "\
" ORDER BY 1"

#define Query_for_list_of_functions \
"   SELECT TRIM(LOWER(rdb$function_name)) "\
"     FROM rdb$functions "\
"    WHERE SUBSTRING(LOWER(rdb$function_name) FROM 1 FOR %i) = '%s' "\
" ORDER BY 1"

#define Query_for_list_of_indexes \
"   SELECT TRIM(LOWER(rdb$index_name)) "\
"     FROM rdb$indices "\
"    WHERE rdb$index_name NOT LIKE '%%$%%' "\
"      AND SUBSTRING(LOWER(rdb$index_name) FROM 1 FOR %i) = '%s' "\
" ORDER BY 1"

#define Query_for_list_of_insertables \
"   SELECT TRIM(LOWER(rdb$relation_name)) "\
"     FROM rdb$relations "\
"    WHERE rdb$relation_name NOT LIKE '%%$%%' "\
"      AND SUBSTRING(LOWER(rdb$relation_name) FROM 1 FOR %i) = '%s' "\
" ORDER BY 1"

#define Query_for_list_of_procedures \
"   SELECT TRIM(LOWER(rdb$procedure_name)) "\
"     FROM rdb$procedures  \n"\
"    WHERE rdb$system_flag = 0 \n"\
"       AND SUBSTRING(LOWER(rdb$procedure_name) FROM 1 FOR %i) = '%s' "\
" ORDER BY 1"

/* Objects which can be selected from (tables and views?) */
#define Query_for_list_of_selectables \
"   SELECT TRIM(LOWER(rdb$relation_name)) "\
"     FROM rdb$relations "\
"    WHERE SUBSTRING(LOWER(rdb$relation_name) FROM 1 FOR %i) = '%s' "\
" ORDER BY 1"

#define Query_for_list_of_sequences \
"   SELECT TRIM(LOWER(rdb$generator_name)) \n" \
"     FROM rdb$generators \n" \
"    WHERE rdb$system_flag = 0 \n"\
"      AND SUBSTRING(LOWER(rdb$generator_name) FROM 1 FOR %i) = '%s' \n"\
" ORDER BY 1"


/* http://www.firebirdfaq.org/faq174/ */
#define Query_for_list_of_tables \
"   SELECT TRIM(LOWER(rdb$relation_name)) "\
"     FROM rdb$relations "\
"    WHERE rdb$view_blr IS NULL "\
"      AND (rdb$system_flag IS NULL OR rdb$system_flag = 0) "\
" ORDER BY 1"

#define Query_for_list_of_views \
"   SELECT TRIM(LOWER(rdb$relation_name)) "\
"     FROM rdb$relations "\
"    WHERE rdb$view_blr IS NOT NULL "\
"      AND (rdb$system_flag IS NULL OR rdb$system_flag = 0) "\
" ORDER BY 1"

/*
 * List of keywords and optionally queries to generate a list of appropriate
 * objects which can appear after CREATE, DROP and ALTER.
 */
typedef struct
{
    const char   *name;
    const char   *query;         /* simple query, or NULL */
    const bits32  flags;         /* visibility flags, see below */
} create_alter_drop_item_t;

#define THING_NO_CREATE     (1 << 0)    /* should not show up after CREATE */
#define THING_NO_DROP       (1 << 1)    /* should not show up after DROP */
#define THING_NO_SHOW       (THING_NO_CREATE | THING_NO_DROP)

static const create_alter_drop_item_t words_after_create[] = {
	{"DATABASE",  NULL},
	{"DOMAIN",	  NULL},
	{"EXCEPTION", NULL},
	{"GENERATOR", NULL},
	{"INDEX",	  NULL},
	{"PROCEDURE", NULL},
	{"SEQUENCE",  NULL},
	{"TABLE",	  Query_for_list_of_tables, THING_NO_CREATE},
	{"TRIGGER",	  NULL},
	{"TYPE",	  NULL},
	{"USER",	  NULL},
	{"VIEW",	  NULL},
	{NULL}						/* end of list */
};

static const create_alter_drop_item_t words_after_alter[] = {
	{"DATABASE",  NULL},
	{"DOMAIN",	  NULL},
	{"EXTERNAL FUNCTION", NULL},
	{"GENERATOR", NULL},
	{"PROCEDURE", NULL},
	{"TABLE",	  Query_for_list_of_tables, THING_NO_CREATE},
	{"TRIGGER",	  NULL},
	{NULL}						/* end of list */
};



/* Maximum number of records to be returned by database queries
 * (implemented via SELECT ... ROWS xx).
 * TODO: not currently implemented
 */
static int  completion_max_records;


/*
 * Communication variables set by COMPLETE_WITH_FOO macros and then used by
 * the completion callback functions.  Ugly but there is no better way.
 */
static const char *completion_charp;    /* to pass a string */
static const char *const * completion_charpp;   /* to pass a list of strings */
static const char *completion_info_charp;       /* to pass a second string */
static const char *completion_info_charp2;      /* to pass a third string */
static bool completion_case_sensitive;  /* completion is case sensitive */

/* function in older rl version is completion_matches()  */
#define COMPLETION_MATCHES(text, complete_func) rl_completion_matches(text, complete_func)


#define COMPLETE_WITH_CONST(string) \
do { \
	completion_charp = string; \
	completion_case_sensitive = false; \
	matches = COMPLETION_MATCHES(text, complete_from_const); \
} while (0)

#define COMPLETE_WITH_LIST(list) \
do { \
	completion_charpp = list; \
	completion_case_sensitive = false; \
	matches = COMPLETION_MATCHES(text, complete_from_list); \
} while (0)

#define COMPLETE_WITH_LIST_CS(list) \
do { \
    completion_charpp = list; \
    completion_case_sensitive = true; \
    matches = COMPLETION_MATCHES(text, complete_from_list); \
} while (0)

#define COMPLETE_WITH_QUERY(query) \
do { \
	completion_charp = query; \
	matches = COMPLETION_MATCHES(text, complete_from_query); \
} while (0)

#define COMPLETE_WITH_ATTR(relation) \
do { \
	completion_info_charp = relation; \
	completion_charp = Query_for_list_of_attributes; \
	matches = COMPLETION_MATCHES(text, complete_from_query); \
} while (0)

/* word break characters */
#define WORD_BREAKS     "\t\n@$><=;|&{() "


/**
 * initialize_tabcomplete()
 *
 * Initialize libreadline and nominate the completion function
 */
void
initialize_tabcomplete(void)
{
	rl_attempted_completion_function = (void *) fbsql_completion;

	rl_basic_word_break_characters = WORD_BREAKS;

	completion_max_records = 1000;
}


/**
 * fbsql_completion()
 *
 * Main tab completion handler function
 */
static char **
fbsql_completion(char *text, int start, int end)
{
	/* This is the variable we'll return. */
	char	  **matches = NULL;

	/* This array will contain some scannage of the input line. */
	char	   *previous_words[6];


	/* For compactness, we use these macros to reference previous_words[]. */
#define prev_wd   (previous_words[0])
#define prev2_wd  (previous_words[1])
#define prev3_wd  (previous_words[2])
#define prev4_wd  (previous_words[3])
#define prev5_wd  (previous_words[4])
#define prev6_wd  (previous_words[5])

	static const char *const backslash_commands[] = {
		"\\a", "\\activity", "\\autocommit",
		"\\conninfo", "\\copyright",
		"\\d", "\\df", "\\di", "\\dp", "\\ds", "\\dt", "\\du", "\\dv",
		"\\format",
		"\\l",
		"\\plan",
		"\\q",
		"\\set",
		"\\timing",
		"\\util",
		NULL
	};

	static const char *const sql_commands[] = {
		"ALTER",
		"BEGIN",
		"COMMENT", "COMMIT", "CREATE",
		"DELETE",
		"DROP",
		"INSERT",
		"ROLLBACK",
		"SELECT", "SET", "SHOW", "START",
		"UPDATE",
		NULL
	};

	(void) end;					/* not used */

	/* Clear a few things. */
	completion_charp = NULL;
	completion_charpp = NULL;
	completion_info_charp = NULL;
	completion_info_charp2 = NULL;

	/*
	 * Scan the input line before our current position for the last few words.
	 * According to those we'll make some smart decisions on what the user is
	 * probably intending to type.
	 */
	get_previous_words(start, previous_words, lengthof(previous_words));

	/* If a backslash command was started, continue */
	if (text[0] == '\\')
	{
		COMPLETE_WITH_LIST_CS(backslash_commands);
	}
	else if (prev_wd[0] == '\0') {
		COMPLETE_WITH_LIST(sql_commands);
	}

	else if (strcmp(prev_wd, "\\format") == 0)
	{
		static const char *const list_bs_format[] =
			{"alignment", "border", "null",
			 NULL
			};
		COMPLETE_WITH_LIST_CS(list_bs_format);
	}
	else if (strcmp(prev2_wd, "\\format") == 0 &&
			 strcmp(prev_wd, "alignment") == 0)
	{
		static const char *const list_alignment[] =
			{"aligned", "unaligned",
			 NULL
			};
		COMPLETE_WITH_LIST_CS(list_alignment);

	}
	else if (strcmp(prev2_wd, "\\format") == 0 &&
			 strcmp(prev_wd, "border") == 0)
	{
		static const char *const list_border[] =
			{"classic", "minimal",
			 NULL
			};
		COMPLETE_WITH_LIST_CS(list_border);
	}


/* ALTER */
	else if (pg_strcasecmp(prev_wd, "ALTER") == 0)
		matches = COMPLETION_MATCHES(text, alter_keyword_generator);

/* COMMENT */
	else if (pg_strcasecmp(prev_wd, "COMMENT") == 0)
	{
		COMPLETE_WITH_CONST("ON");
	}
	else if (pg_strcasecmp(prev2_wd, "COMMENT") == 0 &&
			 pg_strcasecmp(prev_wd, "ON") == 0)
	{
/* http://www.firebirdsql.org/refdocs/langrefupd25-ddl-comment.html */
		static const char *const list_COMMENT[] =
			{"DATABASE", "CHARACTER SET", "COLLATION", "COLUMN", "DOMAIN",
			 "EXCEPTION", "EXTERNAL FUNCTION", "FILTER", "GENERATOR", "INDEX",
			 "PARAMETER", "PROCEDURE", "ROLE", "SEQUENCE", "TABLE",
			 "TRIGGER", "VIEW",
			 NULL
			};

		COMPLETE_WITH_LIST(list_COMMENT);
	}
	else if (pg_strcasecmp(prev3_wd, "COMMENT") == 0 &&
			 pg_strcasecmp(prev2_wd, "ON") == 0 &&
			 pg_strcasecmp(prev_wd, "DATABASE") == 0)
		COMPLETE_WITH_CONST("IS");
	else if (pg_strcasecmp(prev4_wd, "COMMENT") == 0 &&
			 pg_strcasecmp(prev3_wd, "ON") == 0 &&
			 pg_strcasecmp(prev2_wd, "DATABASE") != 0)
		COMPLETE_WITH_CONST("IS");

/* CREATE */
	else if (pg_strcasecmp(prev_wd, "CREATE") == 0)
		matches = COMPLETION_MATCHES(text, create_or_drop_keyword_generator);

/* DELETE */
	else if (pg_strcasecmp(prev_wd, "DELETE") == 0)
		COMPLETE_WITH_CONST("FROM");
/* DROP */
	/* ensure this only matches when at the start of a command */
	else if (pg_strcasecmp(prev_wd, "DROP") == 0 &&
			 prev2_wd[0] == '\0')
		matches = COMPLETION_MATCHES(text, create_or_drop_keyword_generator);

/* INSERT */
	/* Complete INSERT with "INTO" */
	else if (pg_strcasecmp(prev_wd, "INSERT") == 0)
		COMPLETE_WITH_CONST("INTO");
	/* Complete INSERT INTO with table names */
	else if (pg_strcasecmp(prev2_wd, "INSERT") == 0 &&
			 pg_strcasecmp(prev_wd, "INTO") == 0)
		COMPLETE_WITH_QUERY(Query_for_list_of_insertables);
	/* Complete "INSERT INTO <table> (" with attribute names */
	else if (pg_strcasecmp(prev4_wd, "INSERT") == 0 &&
			 pg_strcasecmp(prev3_wd, "INTO") == 0 &&
			 pg_strcasecmp(prev_wd, "(") == 0)
			 COMPLETE_WITH_ATTR(prev2_wd);

	/*
	 * Complete INSERT INTO <table> with "(" or "VALUES" or "SELECT" or
	 * "TABLE" or "DEFAULT VALUES"
	 */
	else if (pg_strcasecmp(prev3_wd, "INSERT") == 0 &&
			 pg_strcasecmp(prev2_wd, "INTO") == 0)
	{
		static const char *const list_INSERT[] =
		{"(", "DEFAULT VALUES", "SELECT",  "VALUES", NULL};

		COMPLETE_WITH_LIST(list_INSERT);
	}
/* SET */
/* XXX check if other SET syntax supported */
/* SET GENERATOR
  http://www.firebirdsql.org/refdocs/langrefupd20-set-generator.html
*/
	else if (pg_strcasecmp(prev_wd, "SET") == 0)
	{
		COMPLETE_WITH_CONST("TRANSACTION");
	}

/* ... FROM ... */
	else if (pg_strcasecmp(prev_wd, "FROM") == 0)
		COMPLETE_WITH_QUERY(Query_for_list_of_selectables);
/* \df */
	else if (pg_strcasecmp(prev_wd, "\\df") == 0)
		COMPLETE_WITH_QUERY(Query_for_list_of_functions);
/* \di */
	else if (pg_strcasecmp(prev_wd, "\\di") == 0)
		COMPLETE_WITH_QUERY(Query_for_list_of_indexes);
/* \dp */
	else if (pg_strcasecmp(prev_wd, "\\dp") == 0)
		COMPLETE_WITH_QUERY(Query_for_list_of_procedures);
/* \ds */
	else if (pg_strcasecmp(prev_wd, "\\ds") == 0)
		COMPLETE_WITH_QUERY(Query_for_list_of_sequences);
/* \dt */
	else if (pg_strcasecmp(prev_wd, "\\dt") == 0)
		COMPLETE_WITH_QUERY(Query_for_list_of_tables);
/* \dv */
	else if (pg_strcasecmp(prev_wd, "\\dv") == 0)
		COMPLETE_WITH_QUERY(Query_for_list_of_views);
/* \d */
	else if (pg_strcasecmp(prev_wd, "\\d") == 0)
		COMPLETE_WITH_QUERY(Query_for_list_of_selectables);

/* \plan */
	else if (pg_strcasecmp(prev_wd, "\\plan") == 0)
	{
		static const char *const list_PLAN[] =
		{"on", "only", "off", NULL};

		COMPLETE_WITH_LIST_CS(list_PLAN);
	}

/* \util */
	else if (pg_strcasecmp(prev_wd, "\\util") == 0)
	{
		static const char *const list_UTIL[] =
		{"set_index_statistics", NULL};

		COMPLETE_WITH_LIST_CS(list_UTIL);
	}

	/*
	 * Finally, we look through the list of "things", such as TABLE, INDEX and
	 * check if that was the previous word. If so, execute the query to get a
	 * list of them.
	 */
	else
	{
		int i;

		for (i = 0; words_after_create[i].name; i++)
		{
			if (pg_strcasecmp(prev_wd, words_after_create[i].name) == 0)
			{
				if (words_after_create[i].query)
				{
					COMPLETE_WITH_QUERY(words_after_create[i].query);
				}
				break;
			}
		}
	}

	/*
	 * Return zero-length string to prevent readline automatically
	 * attempting filename completion
	 */
	if (matches == NULL)
	{
		COMPLETE_WITH_CONST("");
		rl_completion_append_character = '\0';
	}

	/* free storage */
	{
		int i;

		for (i = 0; i < lengthof(previous_words); i++)
			free(previous_words[i]);
	}

	return matches;
}


/**
 * alter_keyword_generator()
 *
 */
static char *
alter_keyword_generator(const char *text, int state)
{
	static int	list_index,
				string_length;
	const char *name;

	/* If this is the first time for this completion, init some values */
	if (state == 0)
	{
		list_index = 0;
		string_length = strlen(text);
	}

	/* find something that matches */
	while ((name = words_after_alter[list_index++].name))
	{
		if (pg_strncasecmp(name, text, string_length) == 0)
			return fb_strdup_keyword_case(name, text);
	}
	/* if nothing matches, return NULL */
	return NULL;
}


/**
 * create_or_drop_keyword_generator()
 *
 * List of keywords which can follow CREATE or DROP
 */
static char *
create_or_drop_keyword_generator(const char *text, int state)
{
	static int	list_index,
				string_length;
	const char *name;

	/* If this is the first time for this completion, init some values */
	if (state == 0)
	{
		list_index = 0;
		string_length = strlen(text);
	}

	/* find something that matches */
	while ((name = words_after_create[list_index++].name))
	{
		if (pg_strncasecmp(name, text, string_length) == 0)
			return fb_strdup_keyword_case(name, text);
	}
	/* if nothing matches, return NULL */
	return NULL;
}


/**
 * complete_from_list()
 *
 * Returns one of a fixed, NULL pointer terminated list of strings (if matching)
 * in their defined order. This can be used if there are only a fixed number of
 * SQL words that can appear at certain spot.
 */
static char *
complete_from_list(const char *text, int state)
{
	static int	string_length,
				list_index,
				matches;
	static bool casesensitive;
	const char *item;

	/* Initialization */
	if (state == 0)
	{
		list_index = 0;
		string_length = strlen(text);
		casesensitive = completion_case_sensitive;
		matches = 0;
	}

	while ((item = completion_charpp[list_index++]))
	{
		/* First pass is case sensitive */
		if (casesensitive && strncmp(text, item, string_length) == 0)
		{
			matches++;
			return strdup(item);
		}

		/* Second pass is case insensitive, don't bother counting matches */
		if (!casesensitive && pg_strncasecmp(text, item, string_length) == 0)
		{
			if (completion_case_sensitive)
				return strdup(item);
			else

				/*
				 * If case insensitive matching was requested initially,
				 * adjust the case according to setting.
				 */
				return fb_strdup_keyword_case(item, text);

		}
	}

	/*
	 * No matches found. If we're not case insensitive already, lets switch to
	 * being case insensitive and try again
	 */
	if (casesensitive && matches == 0)
	{
		casesensitive = false;
		list_index = 0;
		state++;
		return complete_from_list(text, state);
	}

	/* If no more matches, return null. */
	return NULL;
}

/**
 * complete_from_const()
 *
 * This function returns one fixed string the first time even if it doesn't
 * match what's there, and nothing the second time. This should be used if
 * there is only one possibility that can appear at a certain spot, so
 * misspellings will be overwritten.  The string to be passed must be in
 * completion_charp.
 */
static char *
complete_from_const(const char *text, int state)
{
	if (state == 0)
	{

		if (completion_case_sensitive)
			return strdup(completion_charp);
		else

			/*
			 * If case insensitive matching was requested initially, adjust
			 * the case according to setting.
			 */
			return fb_strdup_keyword_case(completion_charp, text);
	}
	else
	{
		return NULL;
	}
}


/**
 * complete_from_query()
 *
 * Dynamically generate tab completion candidates from the specified query
 */
static char *
complete_from_query(const char *text, int state)
{
	static int	list_index,
				string_length;
	static FQresult *result = NULL;
	char			*query;

	if (state == 0)
	{
		string_length = strlen(text);
		list_index = 0;

		query = malloc(strlen(completion_charp) * 2 + 1);

		if(completion_info_charp != NULL)
		{
			sprintf(query, completion_charp, string_length, text, completion_info_charp);
		}
		else
		{
			sprintf(query, completion_charp, string_length, text);
		}

		result = FQexecTransaction(fset.conn, query);

		free(query);
	}

	/* Find something that matches */
	if (result && FQresultStatus(result) == FBRES_TUPLES_OK)
	{
		const char *item;
		while (list_index < FQntuples(result) &&
			   (item = FQgetvalue(result, list_index++, 0)))
		{
			if (pg_strncasecmp(text, item, string_length) == 0)
				return strdup(item);
		}
	}

	FQclear(result);
	result = NULL;
	return NULL;
}


/**
 * get_previous_words()
 *
 * Return the nwords word(s) before point.  Words are returned right to left,
 * that is, previous_words[0] gets the last word before point.
 * If we run out of words, remaining array elements are set to empty strings.
 * Each array element is filled with a malloc'd string.
 */
static void
get_previous_words(int point, char **previous_words, int nwords)
{
	const char *buf = rl_line_buffer;	/* alias */
	int			i;

	/* first we look for a non-word char before the current point */
	for (i = point - 1; i >= 0; i--)
		if (strchr(WORD_BREAKS, buf[i]))
			break;
	point = i;

	while (nwords-- > 0)
	{
		int			start,
					end;
		char	   *s;

		/* now find the first non-space which then constitutes the end */
		end = -1;
		for (i = point; i >= 0; i--)
		{
			if (!isspace((unsigned char) buf[i]))
			{
				end = i;
				break;
			}
		}

		/*
		 * If no end found we return an empty string, because there is no word
		 * before the point
		 */
		if (end < 0)
		{
			point = end;
			s = strdup("");
		}
		else
		{
			/*
			 * Otherwise we now look for the start. The start is either the
			 * last character before any word-break character going backwards
			 * from the end, or it's simply character 0. We also handle open
			 * quotes and parentheses.
			 */
			bool		inquotes = false;
			int			parentheses = 0;

			for (start = end; start > 0; start--)
			{
				if (buf[start] == '"')
					inquotes = !inquotes;
				if (!inquotes)
				{
					if (buf[start] == ')')
						parentheses++;
					else if (buf[start] == '(')
					{
						if (--parentheses <= 0)
							break;
					}
					else if (parentheses == 0 &&
							 strchr(WORD_BREAKS, buf[start - 1]))
						break;
				}
			}

			point = start - 1;

			/* make a copy of chars from start to end inclusive */
			s = malloc(end - start + 2);
			strlcpy(s, &buf[start], end - start + 2);
		}

		*previous_words++ = s;
	}
}

/* HELPER FUNCTIONS */


/**
 * fb_strdup_keyword_case()
 *
 * Make a strdup copy of s and convert the case according to
 * COMP_KEYWORD_CASE variable, using ref as the text that was already entered.
 */
static char *
fb_strdup_keyword_case(const char *s, const char *ref)
{
	char	   *ret,
			   *p;
	unsigned char first = ref[0];
	int			tocase = 2;

	ret = strdup(s);

	if (tocase == -2
		|| ((tocase == -1 || tocase == +1) && islower(first))
		|| (tocase == -1 && !isalpha(first))
		)
		for (p = ret; *p; p++)
			*p = tolower((unsigned char) *p);
	else
		for (p = ret; *p; p++)
			*p = toupper((unsigned char) *p);

	return ret;
}

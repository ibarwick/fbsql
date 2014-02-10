/* ---------------------------------------------------------------------
 *
 * query.c
 *
 * Execute a query and display the results
 *
 * ---------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <ctype.h>

#include "libfq.h"
#include "fbsql.h"
#include "query.h"
#include "settings.h"




static char *
_formatColumn(const FQresult *query_result, int row, int column, char *value, bool for_header);

static char *
_formatValue(const FQresult *query_result, int row, int column, const char *value, bool for_header);

static char *
_formatColumnHeaderUnderline(const FQresult *query_result, int column);

static int
_getColumnMaxWidth(const FQresult *query_result, int column);

static void
_printTableHeader(const FQresult *query_result, const printQueryOpt *pqopt);


/**
 * SendQuery()
 *
 * Send query and generate output, including timing and error messages.
 * (Note that INFO/WARNING message generation is handled by libfq).
 */
bool
SendQuery(const char *query)
{
    FQresult   *query_result;
    query_time  before, after;
    double      elapsed_msec = 0;

    char *expbuffer;

    if(fset.timing)
        gettimeofday(&before, NULL);

    query_result = FQexec(fset.conn, query);

    switch(FQresultStatus(query_result))
    {
        case FBRES_EMPTY_QUERY:
        case FBRES_BAD_RESPONSE:
        case FBRES_NONFATAL_ERROR:
        case FBRES_FATAL_ERROR:
            printf("%s\n", FQresultErrorMessage(query_result));
            /* XXX TODO: make nicer */

            FQresultDumpErrorFields(fset.conn, query_result);
            FQclear(query_result);
            return false;

        case FBRES_TUPLES_OK:
            if(fset.plan_display != PLAN_DISPLAY_ONLY)
            {
                printQuery(query_result, &fset.popt);
                printf("(%i rows)\n", FQntuples(query_result));
            }

            if(fset.plan_display != PLAN_DISPLAY_OFF)
            {
                expbuffer = FQexplainStatement(fset.conn, query);
                if(expbuffer != NULL)
                {
                    puts(expbuffer);
                    free(expbuffer);
                }
            }
            break;

        case FBRES_COMMAND_OK:
            puts("");
            break;
        case FBRES_TRANSACTION_START:
            puts("START");
            break;
        case FBRES_TRANSACTION_COMMIT:
            puts("COMMIT");
            break;
        case FBRES_TRANSACTION_ROLLBACK:
            puts("ROLLBACK");
            break;
        default:
            /* should never reach here */
            puts("Unexpected result code");
    }

    /*if(fset.conn->trans == 0L)
        puts("no transaction");
    else
    puts("in transaction");*/


    FQclear(query_result);

    if(fset.timing)
    {
        gettimeofday(&after, NULL);
        INSTR_TIME_SUBTRACT(after, before);
        elapsed_msec = INSTR_TIME_GET_MILLISEC(after);
        printf("Time: %.3f ms\n", elapsed_msec);
    }

    return true;
}


/**
 * printQuery()
 *
 * Display the returned query data according to the selected
 * formatting options.
 */
void
printQuery(const FQresult *query_result, const printQueryOpt *pqopt)
{
    int i, nfields, ntuples;

    _printTableHeader(query_result, pqopt);

    /* Print data rows */
    ntuples = FQntuples(query_result);
    nfields = FQnfields(query_result);

    for(i = 0; i < ntuples; i++)
    {
        int j;

        for(j = 0; j < nfields; j++)
        {

            if(j)
                printf("%s", pqopt->topt.border_format->divider);

            if(FQgetisnull(query_result, i, j))
            {
                printf("%s", _formatColumn(query_result, i, j, pqopt->nullPrint, false));
            }
            else
            {
                char *value = FQgetvalue(query_result, i, j);
                printf("%s", _formatColumn(query_result, i, j, value, false));
            }
        }
        puts("");
    }
}


/**
 * _formatColumn()
 *
 * Format column for display - padding and column value
 */
static char *
_formatColumn(const FQresult *query_result, int row, int column, char *value, bool for_header)
{
    int value_len, column_max_len;
    char *result;
    char *formatted_value;
    char format[32];

    column_max_len = _getColumnMaxWidth(query_result, column);

    if(fset.popt.topt.format == PRINT_ALIGNED)
    {
        switch(FQftype(query_result, column))
        {
            case SQL_SHORT:
            case SQL_LONG:
            case SQL_INT64:
            case SQL_FLOAT:
            case SQL_DOUBLE:
                /* right-justify numbers */
                sprintf(format,"%s%%%is%s",
                        fset.popt.topt.border_format->padding ? " " : "",
                        column_max_len,
                        fset.popt.topt.border_format->padding ? " " : "");
                break;

            default:
                sprintf(format,"%s%%-%is%s",
                        fset.popt.topt.border_format->padding ? " " : "",
                        column_max_len,
                        fset.popt.topt.border_format->padding ? " " : "");
        }
    }
    else
    {
        sprintf(format,"%%s");
    }

    formatted_value = _formatValue(query_result, row, column, value, for_header);

    if(value == NULL)
        value_len = 0;
    else
        value_len = strlen(formatted_value);

    result = (char *)malloc(column_max_len + (fset.popt.topt.border_format->padding ? 2 : 0) + 1);

    sprintf(result, format, formatted_value, column_max_len - value_len);

    if(formatted_value != NULL)
        free(formatted_value);

    return result;
}


/**
 * _formatValue()
 *
 * Format column value for display
 */
static char *
_formatValue(const FQresult *query_result, int row, int column, const char *value, bool for_header)
{
    char *formatted_value;

    if(for_header == false && FQftype(query_result, column) == SQL_DB_KEY)
    {
        char *src = FQformatDbKey(query_result, row, column);
        formatted_value = (char *)malloc(strlen(src) + 1);
        sprintf(formatted_value, "%s", src);
        free(src);
    }
    else {
        formatted_value = (char *)malloc(strlen(value) + 1);
        sprintf(formatted_value, "%s", value);
    }

    return formatted_value;
}


/**
 * _formatColumnHeaderUnderline()
 *
 * Generate underline bar for a column header
 */
static char *
_formatColumnHeaderUnderline(const FQresult *query_result, int column)
{
    int column_max_len, i;
    char *underline;

    column_max_len = _getColumnMaxWidth(query_result, column)
        + (fset.popt.topt.border_format->padding ? 2 : 0);

    underline = malloc(column_max_len + 1);

    for(i = 0; i < column_max_len; i++)
    {
        underline[i] = fset.popt.topt.border_format->header_underline[0];
    }

    underline[i] = '\0';

    return underline;
}


/**
 * _getColumnMaxWidth()
 *
 * Get the maximum possible width of a column from libfq (widest value or
 * the header width if wider), then check against the width of the NULL
 * identifier if it has NULL values.
 */
static int
_getColumnMaxWidth(const FQresult *query_result, int column)
{
    int max_width;

    /* Columns containing the RDB$DB_KEY value will always be fixed-width */
    if(FQftype(query_result, column) == SQL_DB_KEY)
    {
        return FB_DB_KEY_LEN;
    }

    max_width = FQfmaxwidth(query_result, column);

    /* check if column has null values, and if so check whether the
       null display value is wider
     */
    if(FQfhasNull(query_result, column) == true)
    {
        int null_width = strlen(fset.popt.nullPrint);
        if(null_width > max_width)
            max_width = null_width;
    }

    return max_width;
}


static void
_printTableHeader(const FQresult *query_result, const printQueryOpt *pqopt)
{
    int i;

    char *p;
    char *column_name;
    char *column_name_lc;

    /* No tuples returned - no header info available :(
       Not sure if there is a work around to get the header info
       in this case */
    if(FQntuples(query_result) == 0)
        return;

    if(pqopt->header != NULL)
    {
        if(pqopt->topt.format == PRINT_ALIGNED)
        {
            int width = 0;
            char format[32];
            /* Calculate max width */
            for(i = 0; i < FQnfields(query_result); i++)
            {
                width += _getColumnMaxWidth(query_result, i);
            }

            /* Add padding and border column width */
            width += (FQnfields(query_result) * 3);

            sprintf(format,"%%%is\n",
                    width - ((width - (int)strlen(pqopt->header)) / 2)
                );
            printf(format, pqopt->header);
        }
        else {
            printf("%s\n", pqopt->header);
        }
    }

    /* Print column headers */

    for(i = 0; i < FQnfields(query_result); i++)
    {
        if(i)
            printf("%s", pqopt->topt.border_format->divider);

        column_name = FQfname(query_result, i);
        column_name_lc = NULL;

        /* Fold columns to lower case. This will not fold mixed-case columns
           which were explicitly quoted, but any upper-case columns quoted
           on creation will be converted to lower-case. Not sure if the
           API provides any way of detecting whether column names were quoted.
         */
        if(fset.lc_fold == true)
        {
            column_name_lc = (char *)malloc(strlen(column_name) + 1);
            strcpy(column_name_lc, column_name);

            for(p = column_name_lc; *p; p++)
            {
                *p = toupper((unsigned char) *p);
            }

            if(strncmp(column_name_lc, column_name, strlen(column_name)) == 0)
            {
                for(p = column_name_lc; *p; p++)
                {
                    *p = tolower((unsigned char) *p);
                }
                column_name = column_name_lc;
            }
        }

        printf("%s", _formatColumn(query_result, 0, i, column_name, true));

        if(fset.lc_fold == true)
            free(column_name_lc);
    }

    puts("");

    if(pqopt->topt.format == PRINT_ALIGNED)
    {
        /* print column header underline */
        for(i = 0; i < FQnfields(query_result); i++)
        {
            if(i)
                printf("%s", pqopt->topt.border_format->junction);

            printf("%s", _formatColumnHeaderUnderline(query_result, i));
        }

        puts("");
    }
}

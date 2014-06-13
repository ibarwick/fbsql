/* ---------------------------------------------------------------------
 *
 * inputloop.c
 *
 * Main input processing loop
 *
 * ---------------------------------------------------------------------
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>


#include <signal.h>

#include "libfq.h"
#include "inputloop.h"
#include "fbsql.h"
#include "input.h"
#include "common.h"
#include "command.h"
#include "query.h"
#include "fbsqlscan.h"


char *
_formatPrompt(void);

/*
 * Main loop for processing input
 *
 * Currently accepts input from console only, however it should be possible
 * to modify this to accept input from a FILE pointer.
 */

int
InputLoop(FILE *source)
{
    FbsqlScanState scan_state;  /* lexer working state */


    volatile FQExpBuffer query_buf;     /* buffer for query being accumulated */
    volatile FQExpBuffer previous_buf;  /* if there isn't anything in the new
                                         * buffer yet, use this one for \e,
                                         * etc. */
    FQExpBuffer history_buf;    /* earlier lines of a multi-line command, not
                                 * yet saved to readline history */


    volatile int successResult = EXIT_SUCCESS;
    volatile backslashResult slashCmdStatus = FBSQL_CMD_UNKNOWN;
    char       *line;             /* current input line */


    bool        success;
    int         added_nl_pos;
    bool        line_saved_in_history;

    volatile bool die_on_error = false;

    if (signal(SIGINT, handle_signals) == SIG_ERR) {
        printf("failed to register interrupts with kernel\n");
        return EXIT_FAILURE;
    }

    while ( sigsetjmp( sigint_interrupt_jmp, 1 ) != 0 );

    query_buf = createFQExpBuffer();
    previous_buf = createFQExpBuffer();
    history_buf = createFQExpBuffer();

    if (FQExpBufferBroken(query_buf) ||
        FQExpBufferBroken(previous_buf) ||
        FQExpBufferBroken(history_buf))
    {
        fbsql_error("out of memory\n");
        exit(EXIT_FAILURE);
    }
    /* Create working state */
    scan_state = fbsql_scan_create(";");

    while (successResult == EXIT_SUCCESS)
    {
        char *current_prompt = _formatPrompt();
        line = gets_interactive((char *) current_prompt);

        if (line == NULL)
        {
            puts("\\q");
            break;
        }

        if( query_buf->len == 0 &&  strncasecmp(line, "help", 4) == 0)
        {
            free(line);
            puts("This is fbsql, a command-line interface to Firebird.");
            printf("Type:  \\copyright for distribution terms\n"
                   /*"       \\h for help with SQL commands\n" */
                   "       \\? for help with fbsql commands\n"
                   "       \\g or terminate with semicolon to execute query\n"
                   "       \\q to quit\n");

            fflush(stdout);
            continue;
        }

        /* insert newlines into query buffer between source lines */
        if (query_buf->len > 0)
        {
            appendFQExpBufferChar(query_buf, '\n');
            added_nl_pos = query_buf->len;
        }
        else
            added_nl_pos = -1;  /* flag we didn't add one */

        /* from around here we need to start setting up the scanner */

        /* crude check for slash commands at the start of a line
               (we will need to use scanner for these too) */

        /*
         * Parse line, looking for command separators.
         */
        fbsql_scan_setup(scan_state, line, strlen(line));
        success = true;
        line_saved_in_history = false;

        while(success || !die_on_error)
        {
            FbsqlScanResult scan_result;
            char prompt_tmp[100];
            scan_result = fbsql_scan(scan_state, query_buf, prompt_tmp);

            if (scan_result == FSCAN_SEMICOLON)
            {

                /*
                 * Save query in history.  We use history_buf to accumulate
                 * multi-line queries into a single history entry.
                 */
                if (!line_saved_in_history)
                {
                    fb_append_history(line, history_buf);
                    send_history(history_buf);
                    line_saved_in_history = true;
                }

                /* execute query */
                success = SendQuery(query_buf->data);

                /* transfer query to previous_buf by pointer-swapping */
                {
                    FQExpBuffer swap_buf = previous_buf;

                    previous_buf = query_buf;
                    query_buf = swap_buf;
                }
                resetFQExpBuffer(query_buf);
                added_nl_pos = -1;
            }
            else if (scan_result == FSCAN_BACKSLASH)
            {
                /*
                 * If we added a newline to query_buf, and nothing else has
                 * been inserted in query_buf by the lexer, then strip off the
                 * newline again.  This avoids any change to query_buf when a
                 * line contains only a backslash command.  Also, in this
                 * situation we force out any previous lines as a separate
                 * history entry; we don't want SQL and backslash commands
                 * intermixed in history if at all possible.
                 */
                if (query_buf->len == added_nl_pos)
                {
                    query_buf->data[--query_buf->len] = '\0';
                    send_history(history_buf);
                }
                added_nl_pos = -1;

                /* save backslash command in history */
                if (!line_saved_in_history)
                {
                    fb_append_history(line, history_buf);
                    send_history(history_buf);
                    line_saved_in_history = true;
                }


                /* execute backslash command */
                slashCmdStatus = HandleSlashCmds(scan_state,
                                                 query_buf->len > 0 ?
                                                 query_buf : previous_buf);

                if (slashCmdStatus == FBSQL_CMD_SEND)
                {
                    success = SendQuery(query_buf->data);

                    /* transfer query to previous_buf by pointer-swapping */
                    {
                        FQExpBuffer swap_buf = previous_buf;

                        previous_buf = query_buf;
                        query_buf = swap_buf;
                    }
                    resetFQExpBuffer(query_buf);

                    /* flush any paren nesting info after forced send */
                    fbsql_scan_reset(scan_state);
                }
                else if (slashCmdStatus == FBSQL_CMD_TERMINATE)
                    break;
                else if(slashCmdStatus == FBSQL_CMD_ERROR)
                    printf("Invalid slash command \"%s\". Show help with \\? \n", line);
            }

            /* fall out of loop if lexer reached EOL */
            if (scan_result == FSCAN_INCOMPLETE ||
                scan_result == FSCAN_EOL)
                break;
        }

        if (!line_saved_in_history)
            fb_append_history(line, history_buf);

        fbsql_scan_finish(scan_state);
        free(line);
        line = NULL;

        if (slashCmdStatus == FBSQL_CMD_TERMINATE)
        {
            successResult = EXIT_SUCCESS;
            break;
        }


        slashCmdStatus = FBSQL_CMD_UNKNOWN;
    } /* while (successResult == EXIT_SUCCESS) */

    destroyFQExpBuffer(query_buf);

    return successResult;
}


/**
 * _formatPrompt()
 *
 * Rudimentary prompt formatting.
 *
 * Currently generates an isql-style "SQL>" prompt, but adds an asterisk
 * if we're in a transaction.
 */
char *
_formatPrompt(void)
{
    char *prompt[128];
    snprintf(prompt, 127, "SQL%s> ", FQisActiveTransaction(fset.conn) ? "*" : "");

    return prompt;
}

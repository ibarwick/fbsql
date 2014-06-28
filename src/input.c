/* ---------------------------------------------------------------------
 *
 * input.c
 *
 * Interfaces with libreadline to handle keyboard input and command line
 * history
 *
 * ---------------------------------------------------------------------
 */

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "fbsql.h"
#include "common.h"
#include "input.h"
#include "tab-complete.h"

void
init_readline(void)
{
    rl_readline_name = "fbsql";
    using_history();
    fetch_history(fset.fbsql_history);

    initialize_tabcomplete();
}


/**
 * gets_interactive()
 *
 * Gets a line of interactive input, using readline if desired.
 * The result is a malloc'd string.
 *
 * Caller *must* have set up sigint_interrupt_jmp before calling.
 */
char *
gets_interactive(char *prompt)
{
    bool useReadline = true;
    if (useReadline)
    {
        char       *result;

        /* Enable SIGINT to longjmp to sigint_interrupt_jmp */
        sigint_interrupt_enabled = true;

        result = readline((char *)prompt);

        /* Disable SIGINT again */
        sigint_interrupt_enabled = false;

        return result;
    }

    return (char *)NULL;
}


/**
 * fb_append_history()
 *
 * Append a line to internal history buffer
 */
void
fb_append_history(const char *line, FQExpBuffer history_buf)
{
    appendFQExpBufferStr(history_buf, line);
    if (!line[0] || line[strlen(line) - 1] != '\n')
        appendFQExpBufferChar(history_buf, '\n');
}


/**
 * send_history()
 *
 * Add history buffer contents to readline
 */
void
send_history(FQExpBuffer history_buf)
{
    static char *prev_hist = NULL;

    char       *s = history_buf->data;
    int         i;

    /* Trim any trailing \n's (OK to scribble on history_buf) */
    for (i = strlen(s) - 1; i >= 0 && s[i] == '\n'; i--)
        ;
    s[i + 1] = '\0';

    if (s[0])
    {
        if (((fset.histcontrol & hctl_ignorespace) &&
             s[0] == ' ') ||
            ((fset.histcontrol & hctl_ignoredups) &&
             prev_hist && strcmp(s, prev_hist) == 0))
        {
            /* Ignore this line as far as history is concerned */
        }
        else
        {
            /* Save each previous line for ignoredups processing */
            if (prev_hist)
                free(prev_hist);

            prev_hist = strdup(s);
            add_history(history_buf->data);
        }
    }
    resetFQExpBuffer(history_buf);
}


/**
 * fetch_history()
 *
 * Read history from file
 */
bool
fetch_history(char *fname)
{
    if (fname && strcmp(fname, DEVNULL) != 0)
    {
        errno = 0;

        (void) read_history(fname);

        if (errno == 0)
            return true;
    }

    return false;
}


/**
 * save_history()
 *
 * Write history to file
 */
bool
save_history(char *fname)
{

    if (fname && strcmp(fname, DEVNULL) != 0)
    {
        errno = 0;

        (void) write_history(fname);

        if (errno == 0)
            return true;
    }

    return false;
}


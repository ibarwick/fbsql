/* ---------------------------------------------------------------------
 *
 * common.c
 *
 * Miscellaneous common functions
 *
 * ---------------------------------------------------------------------
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#include "fbsql.h"
#include "common.h"
#include "settings.h"

volatile bool sigint_interrupt_enabled = false;
sigjmp_buf sigint_interrupt_jmp;
volatile bool cancel_pressed = false;


/* Line style control structures */
const printTextFormat border_minimal =
{
	"minimal",
	"|",
	"+",
	"-",
	true
};

const printTextFormat border_classic =
{
	"classic",
	" ",
	" ",
	"=",
	false
};



const printTextFormat *
_getBorderFormat(void);


/**
 * handle_signals()
 *
 * Rudimentary signal handling
 */
void
handle_signals(int signo)
{
	/*int			save_errno = errno;
	int			rc;
	char		errbuf[256];*/

	/* if we are waiting for input, longjmp out of it */
	if (sigint_interrupt_enabled)
	{
		sigint_interrupt_enabled = false;
        printf("\n");
		siglongjmp(sigint_interrupt_jmp, 1);
	}

	/* else, set cancel flag to stop any long-running loops */
	//cancel_pressed = true;

}


/**
 * get_home_path()
 *
 * Utility function to generate the full path of the user's home directory
 */
char *
get_home_path()
{
	char          pwdbuf[1024];
	struct passwd pwdstr;
	struct passwd *pwd = NULL;
	char *home_path;

	if (getpwuid_r(geteuid(), &pwdstr, pwdbuf, sizeof(pwdbuf), &pwd) != 0)
		return NULL;

	home_path = (char *)malloc(strlen(pwd->pw_dir) + 1);
	strncpy(home_path, pwd->pw_dir, strlen(pwd->pw_dir) + 1);
	return home_path;
}


/**
 * fb_malloc0()
 *
 * malloc() memory and initialize with NUL bytes.
 */
void *
fb_malloc0(size_t size)
{
	void       *tmp;

	tmp = malloc(size);
    if(tmp != NULL)
        memset(tmp, 0, size);
	return tmp;
}


/**
 * fbsql_error()
 *
 * Error reporting for scripts. Errors should look like
 *	 fbsql:filename:lineno: message
 */
void
fbsql_error(const char *fmt,...)
{
	va_list		ap;
	fflush(stdout);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}


/**
 * init_settings()
 *
 * Initialise the user-definable settings.
 */
void
init_settings(void) {
	fset.timing = true;
	fset.quiet = false;
	fset.lc_fold = true;
	fset.autocommit = true;

/* provisional sane default value */
    fset.client_encoding = "UTF-8";
	fset.popt.nullPrint = strdup("NULL");
	fset.popt.header = NULL;

	fset.popt.topt.format = PRINT_ALIGNED;
	fset.popt.topt.border = BORDER_MINIMAL;
	fset.popt.topt.border_format = _getBorderFormat();

	/* TODO: add sanity checking and fallbacks */
	fset.home_path = get_home_path();

	if(fset.home_path != NULL)
	{
		fset.fbsql_history = (char *)malloc(strlen(fset.home_path) + 1 + strlen(FBSQL_HISTORY) + 1);
		sprintf(fset.fbsql_history, "%s/%s", fset.home_path, FBSQL_HISTORY);
	}
	else {
		puts("init_settings(): unable to get home directory");
	}

	fset.histcontrol = hctl_ignoreboth;
	fset.echo_hidden = false;
	fset.plan_display = PLAN_DISPLAY_OFF;
}


const printTextFormat *
_getBorderFormat()
{
	const printTextFormat *border_format;
	switch(fset.popt.topt.border)
	{
		case BORDER_CLASSIC:
			border_format = &border_classic;
			break;
		case BORDER_MINIMAL:
		default:
			border_format = &border_minimal;
			break;
	}

	return border_format;
}

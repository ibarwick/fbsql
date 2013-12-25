#ifndef INPUT_H
#define INPUT_H

#include "fbsql.h"
#include "settings.h"


void
init_readline(void);

extern char*
gets_interactive(char * prompt);

extern void
fb_append_history(const char *line, FQExpBuffer history_buf);

extern void
send_history(FQExpBuffer history_buf);

extern bool
fetch_history(char *fname);

extern bool
save_history(char *fname);

#endif   /* INPUT_H */

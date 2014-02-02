#ifndef COMMON_H
#define COMMON_H
#define MAXPATH 1024

#include <setjmp.h>
#include "settings.h"

extern volatile bool sigint_interrupt_enabled;

extern sigjmp_buf sigint_interrupt_jmp;

extern volatile bool cancel_pressed;

extern void setup_cancel_handler(void);
extern void handle_signals(int signo);
extern char *get_home_path(void);

extern void *fb_malloc0(size_t size);

extern void fbsql_error(const char *fmt,...);

extern void init_settings(void);

extern const printTextFormat *
_getBorderFormat(void);


#endif   /* COMMON_H */

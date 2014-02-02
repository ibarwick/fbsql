#ifndef PORT_H
#define PORT_H

#include <stdlib.h>

extern size_t strlcpy(char *dst, const char *src, size_t siz);

/* Portable SQL-like case-independent comparisons and conversions */
extern int	pg_strcasecmp(const char *s1, const char *s2);
extern int	pg_strncasecmp(const char *s1, const char *s2, size_t n);
extern unsigned char pg_toupper(unsigned char ch);
extern unsigned char pg_tolower(unsigned char ch);
extern unsigned char pg_ascii_toupper(unsigned char ch);
extern unsigned char pg_ascii_tolower(unsigned char ch);


#endif   /* PORT_H */

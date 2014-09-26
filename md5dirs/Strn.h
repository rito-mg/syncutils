/* Strn.h */

#ifndef _Strn_h_
#define _Strn_h_ 1

/* You should define this from the Makefile. */
#ifndef STRN_ZERO_PAD
#	define STRN_ZERO_PAD 1
#endif

/* You should define this from the Makefile. */
#ifndef STRNP_ZERO_PAD
#	define STRNP_ZERO_PAD 0
#endif

/* Strncat.c */
char *Strncat(char *const, const char *const, const size_t);

/* Strncpy.c */
char *Strncpy(char *const, const char *const, const size_t);

/* Strnpcat.c */
char *Strnpcat(char *, char *, size_t);

/* Strnpcpy.c */
char *Strnpcpy(char *, char *, size_t);

/* Strntok.c */
char *Strtok(char *, const char *);
int Strntok(char *, size_t, char *, const char *);

/* strtokc.c */
char *strtokc(char *, const char *, char **);
int strntokc(char *, size_t, char *, const char *, char **);

#define STRNCPY(d,s) Strncpy((d), (s), (size_t) sizeof(d))
#define STRNCAT(d,s) Strncat((d), (s), (size_t) sizeof(d))

#endif	/* _Strn_h_ */

/* eof Strn.h */

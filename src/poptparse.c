/* (C) 1998 Red Hat Software, Inc. -- Licensing details are in the COPYING
   file accompanying popt source distributions, available from 
   ftp://ftp.redhat.com/pub/code/popt */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* AIX requires this to be the first thing in the file.  */
#ifndef __GNUC__
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifdef _AIX
#pragma alloca
#  else
#   ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#   endif
#  endif
# endif
#elif defined(__GNUC__) && defined(__STRICT_ANSI__)
#define alloca __builtin_alloca
#endif

#include "popt.h"

#define POPT_ARGV_ARRAY_GROW_DELTA 5

int poptParseArgvString(const char * s, int * argcPtr, char *** argvPtr) {
    char * buf, * bufStart, * dst;
    const char * src;
    char quote = '\0';
    int argvAlloced = POPT_ARGV_ARRAY_GROW_DELTA;
    char ** argv = malloc(sizeof(*argv) * argvAlloced);
    char ** argv2;
    int argc = 0;
    int i, buflen;

    buflen = strlen(s) + 1;
    bufStart = buf = alloca(buflen);
    memset(buf, '\0', buflen);

    src = s;
    argv[argc] = buf;

    while (*src) {
	if (quote == *src) {
	    quote = '\0';
	} else if (quote) {
	    if (*src == '\\') {
		src++;
		if (!*src) {
		    free(argv);
		    return POPT_ERROR_BADQUOTE;
		}
		if (*src != quote) *buf++ = '\\';
	    }
	    *buf++ = *src;
	} else if (isspace(*src)) {
	    if (*argv[argc]) {
		buf++, argc++;
		if (argc == argvAlloced) {
		    argvAlloced += POPT_ARGV_ARRAY_GROW_DELTA;
		    argv = realloc(argv, sizeof(*argv) * argvAlloced);
		}
		argv[argc] = buf;
	    }
	} else switch (*src) {
	  case '"':
	  case '\'':
	    quote = *src;
	    break;
	  case '\\':
	    src++;
	    if (!*src) {
		free(argv);
		return POPT_ERROR_BADQUOTE;
	    }
	    /* fallthrough */
	  default:
	    *buf++ = *src;
	}

	src++;
    }

    if (strlen(argv[argc])) {
	argc++, buf++;
    }

    dst = malloc(argc * sizeof(*argv) + (buf - bufStart));
    argv2 = (void *) dst;
    dst += argc * sizeof(*argv);
    memcpy(argv2, argv, argc * sizeof(*argv));
    memcpy(dst, bufStart, buf - bufStart);

    for (i = 0; i < argc; i++) {
	argv2[i] = dst + (argv[i] - bufStart);
    }

    free(argv);

    *argvPtr = argv2;
    *argcPtr = argc;

    return 0;
}

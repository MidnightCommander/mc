/*
 * This file should be included after all system includes and before
 * all local includes.
 */

#ifndef __GLOBAL_H
#define __GLOBAL_H

#include <stdlib.h>	/* for free() and other usefull routins */

#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif

#ifdef HAVE_SYS_TIMEB_H
#  include <sys/timeb.h>
#endif

#ifdef TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
#else
#  ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#  else
#    include <time.h>
#  endif
#endif

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif

#include <glib.h>

#include "fs.h"
#include "mem.h"
#include "util.h"
#include "mad.h"

#include "textconf.h"

extern char *home_dir;

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#define min(x, y)	((x) > (y) ? (y) : (x))
#define max(x, y)	((x) > (y) ? (x) : (y))

/* Just for keeping Your's brains from invention a proper size of the buffer :-) */
#define BUF_10K		10240L
#define BUF_8K		8192L
#define BUF_4K		4096L
#define BUF_1K		1024L

#define BUF_LARGE	BUF_1K
#define BUF_MEDIUM	512
#define BUF_SMALL	128
#define BUF_TINY	64

void refresh_screen (void *);

/* AIX compiler doesn't understand '\e' */
#define ESC_CHAR '\033'
#define ESC_STR  "\033"

#define xgetch getch

#endif

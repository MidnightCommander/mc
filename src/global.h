/*
 * This file should be included after all system includes and before
 * all local includes.
 */

#ifndef __MC_GLOBAL_H
#define __MC_GLOBAL_H

#include <stdlib.h>	/* for free() and other useful routins */

#if defined(STDC_HEADERS) || defined(HAVE_STRING_H)
#  include <string.h>
   /* An ANSI string.h and pre-ANSI memory.h might conflict */
#  if !defined(STDC_HEADERS) && defined(HAVE_MEMORY_H)
#    include <memory.h>
#  endif /* !STDC_HEADERS & HAVE_MEMORY_H */

#else /* !STDC_HEADERS & !HAVE_STRING_H */
#  include <strings.h>
    /* memory and strings.h conflict on other systems */
#endif /* !STDC_HEADERS & !HAVE_STRING_H */

#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif

#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif

/* The O_BINARY definition was taken from gettext */
#if !defined O_BINARY && defined _O_BINARY
  /* For MSC-compatible compilers.  */
#  define O_BINARY _O_BINARY
#endif
#ifdef __BEOS__
  /* BeOS 5 has O_BINARY, but is has no effect.  */
#  undef O_BINARY
#endif
/* On reasonable systems, binary I/O is the default.  */
#ifndef O_BINARY
#  define O_BINARY 0
#endif


/* Replacement for O_NONBLOCK */
#ifndef O_NONBLOCK 
#ifdef O_NDELAY /* SYSV */ 
#define O_NONBLOCK O_NDELAY 
#else /* BSD */ 
#define O_NONBLOCK FNDELAY 
#endif /* !O_NDELAY */ 
#endif /* !O_NONBLOCK */ 


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

#if !defined(HAVE_SYS_TIME_H)
struct timeval {
    long int tv_sec;  /* seconds */
    long int tv_usec; /* microseconds */
};
#endif /* !HAVE_SYS_TIME_H */

#ifdef HAVE_UTIME_H
#  include <utime.h>
#elif defined(HAVE_SYS_UTIME_H)
#  include <sys/utime.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif

#ifdef HAVE_GRP_H
#  include <grp.h>
#endif

#ifdef HAVE_PWD_H
#  include <pwd.h>
#endif

#if defined(__QNX__) && !defined(__QNXNTO__)
/* exec*() from <process.h> */
#  include <unix.h>
#endif

#include <glib.h>

#if defined(HAVE_RX_H) && defined(HAVE_REGCOMP)
#include <rx.h>
#else
#include "eregex.h"
#endif

#ifndef __GNUC__
#  define __attribute__(x)
#endif

#ifndef HAVE_GETUID
#  define getuid() 0
#endif
#ifndef HAVE_GETGID
#  define getgid() 0
#endif
#ifndef HAVE_GETEUID
#  define geteuid() getuid()
#endif
#ifndef HAVE_GETEGID
#  define getegid() getgid()
#endif

#include "fs.h"
#include "util.h"

#include "textconf.h"
#include "../vfs/vfs.h"

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

#endif /* !__MC_GLOBAL_H */

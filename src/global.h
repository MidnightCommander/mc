
/** \file global.h
 *  \brief Header: %global definitions for compatibility
 *
 *  This file should be included after all system includes and before all local includes.
 */


#ifndef MC_GLOBAL_H
#define MC_GLOBAL_H

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

#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif

#if defined(__QNX__) && !defined(__QNXNTO__)
/* exec*() from <process.h> */
#  include <unix.h>
#endif

#include <glib.h>
#include "glibcompat.h"

#ifndef __GNUC__
#  define __attribute__(x)
#endif

#ifdef ENABLE_NLS
#   include <libintl.h>
#   define _(String) gettext (String)
#   ifdef gettext_noop
#	define N_(String) gettext_noop (String)
#   else
#	define N_(String) (String)
#   endif
#else /* Stubs that do something close enough.  */
#   define textdomain(String)
#   define gettext(String) (String)
#   define ngettext(String1,String2,Num) (((Num) == 1) ? (String1) : (String2))
#   define dgettext(Domain,Message) (Message)
#   define dcgettext(Domain,Message,Type) (Message)
#   define bindtextdomain(Domain,Directory)
#   define _(String) (String)
#   define N_(String) (String)
#endif /* !ENABLE_NLS */

#include "fs.h"
#include "util.h"

#ifdef USE_MAINTAINER_MODE
#include "src/logging.h"
#endif

extern const char *home_dir;

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

/* C++ style type casts */
#define const_cast(m_type, m_expr) ((m_type) (m_expr))

#if 0
#ifdef MC_ENABLE_DEBUGGING_CODE
# undef NDEBUG
#else
# define NDEBUG
#endif
#include <assert.h>
#endif

#define MC_ERROR mc_main_error_quark ()
GQuark mc_main_error_quark (void);

#endif

/** \file unixcompat.h
 *  \brief Header: collects differences between the various Unix
 *
 *  This header file collects differences between the various Unix
 *  variants that are supported by the Midnight Commander and provides
 *  replacement routines if they are not natively available.
 *  The major/minor macros are not specified in SUSv3, so we can only hope
 *  they are provided by the operating system or emulate it.
 */

#ifndef MC_UNIXCOMPAT_H
#define MC_UNIXCOMPAT_H

#include <fcntl.h>              /* O_* macros */
#include <signal.h>             /* sig_atomic_t */
#include <unistd.h>

#include <sys/types.h>          /* BSD */

#ifdef MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#elif defined MAJOR_IN_SYSMACROS
#include <sys/sysmacros.h>
#endif

#if defined(HAVE_STRING_H)
#include <string.h>
   /* An ANSI string.h and pre-ANSI memory.h might conflict */
#elif defined(HAVE_MEMORY_H)
#include <memory.h>
#else
#include <strings.h>
    /* memory and strings.h conflict on other systems */
#endif /* !STDC_HEADERS & !HAVE_STRING_H */

#if defined(__QNX__) && !defined(__QNXNTO__)
/* exec*() from <process.h> */
#include <unix.h>
#endif

/*** typedefs(not structures) and defined constants **********************************************/

#ifndef major
#warning major() is undefined. Device numbers will not be shown correctly.
#define major(devnum) (((devnum) >> 8) & 0xff)
#endif

#ifndef minor
#warning minor() is undefined. Device numbers will not be shown correctly.
#define minor(devnum) (((devnum) & 0xff))
#endif

#ifndef makedev
#warning makedev() is undefined. Device numbers will not be shown correctly.
#define makedev(major,minor) ((((major) & 0xff) << 8) | ((minor) & 0xff))
#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

/* The O_BINARY definition was taken from gettext */
#if !defined O_BINARY && defined _O_BINARY
  /* For MSC-compatible compilers.  */
#define O_BINARY _O_BINARY
#endif
#ifdef __BEOS__
  /* BeOS 5 has O_BINARY, but it has no effect.  */
#undef O_BINARY
#endif
/* On reasonable systems, binary I/O is the default.  */
#ifndef O_BINARY
#define O_BINARY 0
#endif

/* Replacement for O_NONBLOCK */
#ifndef O_NONBLOCK
#ifdef O_NDELAY                 /* SYSV */
#define O_NONBLOCK O_NDELAY
#else /* BSD */
#define O_NONBLOCK FNDELAY
#endif /* !O_NDELAY */
#endif /* !O_NONBLOCK */

/* Solaris9 doesn't have PRIXMAX */
#ifndef PRIXMAX
#define PRIXMAX PRIxMAX
#endif

/* ESC_CHAR is defined in /usr/include/langinfo.h in some systems */
#ifdef ESC_CHAR
#undef ESC_CHAR
#endif
/* AIX compiler doesn't understand '\e' */
#define ESC_CHAR '\033'
#define ESC_STR  "\033"

/* OS specific defines */
#define PATH_SEP '/'
#define PATH_SEP_STR "/"
#define IS_PATH_SEP(c) ((c) == PATH_SEP)
#define PATH_ENV_SEP ':'
#define TMPDIR_DEFAULT "/tmp"
#define SCRIPT_SUFFIX ""
#define get_default_editor() "vi"
#define OS_SORT_CASE_SENSITIVE_DEFAULT TRUE

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/*** inline functions ****************************************************************************/

#endif

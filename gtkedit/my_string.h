/* my_string.h - compatability for any system
   Copyright (C) 1996, 1997 Paul Sheer

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
 */

#ifndef _MY_STRING_H
#define _MY_STRING_H

#include "global.h"
#include <config.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdarg.h>

#include <sys/stat.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#include "libgettext.h"

#define _(String) gettext (String)

#ifdef gettext_noop
# define N_(String) gettext_noop (String)
#else
# define N_(String) (String)
#endif


#define MAX_PATH_LEN 1024

/* string include, hopefully works across all unixes */

#ifndef INHIBIT_STRING_HEADER
# if defined (HAVE_STRING_H) || defined (STDC_HEADERS) || defined (_LIBC)
#  include <string.h>
# else
#  include <strings.h>
# endif
#endif

#if defined(__STRICT_ANSI__) && defined(__GNUC__)
 int strcasecmp (const char *p1, const char *p2);
 int strncasecmp (const char *p1, const char *p2, size_t n);
 char *strdup (const char *s);
 char *getwd (char *buf);
 char *tempnam (const char *dir, const char *pfx);
 int lstat(const char *file_name, struct stat *buf);
 int kill(pid_t pid, int sig);
#endif

#ifndef STDC_HEADERS
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif

    size_t strnlen (const char *s, size_t count);

# ifndef HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
# endif
# ifndef HAVE_MEMCMP
    int memcmp (const void *cs, const void *ct, size_t count);
# endif
# ifndef HAVE_MEMCHR
    void *memchr (const void *s, int c, size_t n);
# endif
#ifndef HAVE_STRCASECMP
#  ifndef __STRICT_ANSI__
    int strcasecmp (const char *p1, const char *p2);
#  endif
#endif
#ifndef HAVE_STRNCASECMP
#  ifndef __STRICT_ANSI__
    int strncasecmp (const char *p1, const char *p2, size_t n);
#  endif
#endif
# ifndef HAVE_STRDUP
#  ifndef __STRICT_ANSI__
    char *strdup (const char *s);
#  endif
# endif
#ifndef HAVE_MEMMOVE
    void *memmove (void *dest, const void *src, size_t n);
# endif
# ifndef HAVE_MEMSET
    void *memset (void *dest, int c, size_t n);
# endif
# ifndef HAVE_STRSPN
    size_t strspn (const char *s, const char *accept);
# endif
# ifndef HAVE_STRSTR
    char *strstr (const char *s1, const char *s2);
# endif
# ifndef HAVE_VPRINTF
    int vsprintf (char *buf, const char *fmt, va_list args);
# endif
#endif

#ifndef S_IFMT
#define	S_IFMT	0170000
#endif
#ifndef S_IFDIR
#define	S_IFDIR	0040000
#endif
#ifndef S_IFCHR
#define	S_IFCHR	0020000
#endif
#ifndef S_IFBLK
#define	S_IFBLK	0060000
#endif
#ifndef S_IFREG
#define	S_IFREG	0100000
#endif
#ifndef S_IFIFO
#define	S_IFIFO	0010000
#endif
#ifndef S_IFLNK
#define	S_IFLNK		0120000
#endif
#ifndef S_IFSOCK
#define	S_IFSOCK	0140000
#endif
#ifndef S_ISUID
#define	S_ISUID		04000
#endif
#ifndef S_ISGID
#define	S_ISGID		02000
#endif
#ifndef S_ISVTX
#define	S_ISVTX		01000
#endif
#ifndef S_IREAD
#define	S_IREAD		0400
#endif
#ifndef S_IWRITE
#define	S_IWRITE	0200
#endif
#ifndef S_IEXEC
#define	S_IEXEC		0100
#endif
#ifndef S_ISTYPE
#define	S_ISTYPE(mode, mask)	(((mode) & S_IFMT) == (mask))
#endif
#ifndef S_ISDIR
#define	S_ISDIR(mode)	S_ISTYPE((mode), S_IFDIR)
#endif
#ifndef S_ISCHR
#define	S_ISCHR(mode)	S_ISTYPE((mode), S_IFCHR)
#endif
#ifndef S_ISBLK
#define	S_ISBLK(mode)	S_ISTYPE((mode), S_IFBLK)
#endif
#ifndef S_ISREG
#define	S_ISREG(mode)	S_ISTYPE((mode), S_IFREG)
#endif
#ifndef S_ISFIFO
#define	S_ISFIFO(mode)	S_ISTYPE((mode), S_IFIFO)
#endif
#ifndef S_ISLNK
#define	S_ISLNK(mode)	S_ISTYPE((mode), S_IFLNK)
#endif
#ifndef S_ISSOCK
#define	S_ISSOCK(mode)	S_ISTYPE((mode), S_IFSOCK)
#endif
#ifndef S_IRWXU
#define	S_IRWXU	(__S_IREAD|__S_IWRITE|__S_IEXEC)
#endif

#endif				/*  _MY_STRING_H  */


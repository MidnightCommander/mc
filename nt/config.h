/****************************************************************************
 CONFIG.HPP -   Midnight Commander Configuration for Win32


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

 ----------------------------------------------------------------------------
 Changes:
        - Created 951204/jfg

 ----------------------------------------------------------------------------
 Contents:
        - Headers flags
        - Library flags
        - Typedefs
        - etc.
 ****************************************************************************/
#ifndef __CONFIG_HPP                    //Prevent multiple includes
#define __CONFIG_HPP

#include <../VERSION>

#ifndef WIN32
#   define WIN32
#endif

#ifndef __WIN32__
#   define __WIN32__
#endif

#ifndef MSWINDOWS
#   define MSWINDOWS
#endif

#ifndef pc_system
#   define pc_system
#endif

#define _OS_NT
#define OS2_NT
#define FLOAT_TYPE
#define MIDNIGHT

// ---------------------------------------------------------------------------
// Headers
#define STDC_HEADERS
#define HAVE_STDLIB_H
#define HAVE_STRING_H
#define HAVE_DIRENT_H
#define HAVE_LIMITS_H
#define HAVE_FCNTL_H
#define NO_UNISTD_H
#pragma include_alias(<utime.h>, <sys/utime.h>)			/* Only works for MSVC */

// ---------------------------------------------------------------------------
// "Standard" Library
#define HAVE_MEMSET
#define HAVE_MEMCHR
#define HAVE_MEMCPY
#define HAVE_MEMCMP
#define HAVE_MEMMOVE
#define HAVE_STRDUP
#define HAVE_STRERROR
#define HAVE_TRUNCATE

#define REGEX_MALLOC

#define NO_TERM
#define NO_INFOMOUNT

// ---------------------------------------------------------------------------
// Windowing library
#if !defined(HAVE_SLANG) && !defined (USE_NCURSES)
#define HAVE_SLANG
#endif

#ifdef USE_NCURSES
#define RENAMED_NCURSES
#endif

// ---------------------------------------------------------------------------
// Typedefs (some useless under NT)
typedef int gid_t;                 // Not defined in <sys/types.h>
typedef int uid_t;
typedef int mode_t;
typedef int pid_t;
typedef unsigned int umode_t;
typedef unsigned int nlink_t;

#define INLINE
#define inline

// ---------------------------------------------------------------------------
// File attributes
#define S_ISLNK(x) 0

#ifdef _MSC_VER                                                 // Already defined in Watcom C headers
#define S_ISBLK( m )    0               /* Some of these are not actual values*/
#define S_IFBLK         0010000                         /* but don't worry, these are yet not possible on NT */
#define S_IFLNK         0010000

#define S_IRWXU         0000700
#define S_IRUSR         0000400
#define S_IWUSR         0000200
#define S_IXUSR         0000100

#define S_IRWXG         0000070
#define S_IRGRP         0000040
#define S_IWGRP         0000020
#define S_IXGRP         0000010

#define S_IRWXO         0000007
#define S_IROTH         0000004
#define S_IWOTH         0000002
#define S_IXOTH         0000001

#define S_ISUID         0004000
#define S_ISGID         0002000
#define S_ISVTX         0001000

#define S_IFIFO         _S_IFIFO         /* pipe */

#define S_ISCHR( m )    (((m) & S_IFMT) == S_IFCHR)
#define S_ISDIR( m )    (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG( m )    (((m) & S_IFMT) == S_IFREG)
#define S_ISFIFO( m )   (((m) & S_IFMT) == S_IFIFO)

/* Symbolic constants for the access() function */
#define R_OK    4       /*  Test for read permission    */
#define W_OK    2       /*  Test for write permission   */
#define X_OK    1       /*  Test for execute permission */
#define F_OK    0       /*  Test for existence of file  */


/* Missing Errno definitions */
#define	ELOOP		40	/* Too many symbolic links encountered */

/* Missing mask definition */
#define O_ACCMODE	  0003
#endif  //_MSC_VER


// ---------------------------------------------------------------------------
// Inline definitions

// Pipes
#define popen   _popen
#define pclose  _pclose
#define pipe(p)  _pipe(p, 4096, 0x8000 /*_O_BINARY*/)

#endif //__CONFIG_HPP

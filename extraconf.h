/*
This file is included directly from config.h.
Don't include it from any other files.

The only code that belongs here is preprocessor directives that:

1) change the configuration setting defined in config.h if they
are inappropriate for the edition being compiled.

2) define symbols fully dependendent on those in config.h to eliminate
the need to embed this logic into configure.in.
*/

#ifdef HAVE_LIBPT
#    define HAVE_GRANTPT
#endif

#if defined(HAVE_LIBCRYPT) || defined(HAVE_LIBCRYPT_I)
#    define HAVE_CRYPT
#endif

#if defined(HAVE_SIGADDSET) && defined(HAVE_SIGEMPTYSET)
# if defined(HAVE_SIGACTION) && defined(HAVE_SIGPROCMASK)
#  define SLANG_POSIX_SIGNALS
# endif
#endif

#ifdef __os2__
#    define OS2_NT 1
#    define S_ISFIFO(x) 0

#    define NEEDS_IO_H
#    define NEEDS_DRIVE_H
#    define NEEDS_FCNTL_H
#    define HAS_NO_GRP_PWD_H
#    define HAS_NO_TERMIOS_H
#    define HAS_NO_SYS_PARAM_H
#    define HAS_NO_SYS_IOCTL_H

#    define USE_O_TEXT
#    define HAS_ACS_AS_PCCHARS
#    define NEEDS_CR_LF_TRANSLATION
#endif

#ifdef _OS_NT
#    define OS2_NT 1
#endif

#ifndef OS2_NT
/* some Unices do not define this, and slang requires it: */
#ifndef unix
#    define unix
#endif
#endif

#ifdef HAVE_X
#    undef HAVE_SUBSHELL_SUPPORT
#    undef HAVE_SLANG
#    undef HAS_CURSES
#    undef USE_NCURSES
#    undef HAVE_CHARSET
#    undef HAVE_TEXTMODE_X11_SUPPORT
#endif

#ifndef USE_VFS
#    undef USE_NETCODE
#    undef USE_EXT2FSLIB
#endif

#if defined (HAVE_SOCKETPAIR) || !defined (HAVE_X)
#    define WITH_BACKGROUND
#endif

#if defined (__QNX__) && !defined(__QNXNTO__) && !defined (HAVE_INFOMOUNT_LIST)
#    define HAVE_INFOMOUNT_QNX
#endif

#if defined(HAVE_INFOMOUNT_LIST) || defined(HAVE_INFOMOUNT_QNX)
#    define HAVE_INFOMOUNT
#endif

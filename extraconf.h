/*
This file is included directly from config.h.
Don't include it from any other files.

The only code that belongs here is preprocessor directives that:

1) change the configuration setting defined in config.h if there is a
conflict between them.

2) define symbols that fully depend on those in config.h to eliminate
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

#ifndef USE_VFS
#    undef USE_NETCODE
#    undef USE_EXT2FSLIB
#endif

#if defined (__QNX__) && !defined(__QNXNTO__) && !defined (HAVE_INFOMOUNT_LIST)
#    define HAVE_INFOMOUNT_QNX
#endif

#if defined(HAVE_INFOMOUNT_LIST) || defined(HAVE_INFOMOUNT_QNX)
#    define HAVE_INFOMOUNT
#endif

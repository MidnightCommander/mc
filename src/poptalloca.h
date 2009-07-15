
/** \file poptalloca.h
 *  \brief Header: definitions for alloca for popt
 *
 *  Definitions for alloca (mostly extracted from AC_FUNC_ALLOCA). According
 *  to the autoconf manual in dome versions of AIX the declaration of alloca
 *  has to precede everything else execept comments and prepocessor directives,
 *  i.e. including this file has to preceed anything else.
 *
 *  NOTE: alloca is redefined as malloc on systems which fail to support alloca.
 *  Don't include this header if you frequently use alloca in order to avoid an
 *  unlimited amount of memory leaks.
 *  popt uses alloca only during program startup, i.e. the memory leaks caused
 *  by this redefinition are limited.
 */

#ifndef MC_POPTALLOCA_H
#define MC_POPTALLOCA_H

/* AIX requires this to be the first thing in the file.  */
#ifdef __GNUC__
#    define alloca __builtin_alloca
#else
#    ifdef _MSC_VER
#        include <malloc.h>
#        define alloca _alloca
#    elif __TINYC__
#        include <stddef.h>
#    else
#        if HAVE_ALLOCA_H
#            include <alloca.h>
#        else
#            ifdef _AIX
#pragma alloca
#            else
#                ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#                endif
#            endif
#        endif
#    endif
#endif

#ifndef HAVE_ALLOCA
#    include <stdlib.h>
#    if !defined(STDC_HEADERS) && defined(HAVE_MALLOC_H)
#        include <malloc.h>
#    endif
#    define alloca malloc
#endif

#endif

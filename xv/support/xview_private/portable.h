/*      @(#)portable.h 1.14 93/06/28 SMI      */

/*
 *      (c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *      pending in the U.S. and foreign countries. See LEGAL NOTICE
 *      file for terms of the license.
 */


#ifndef xview_portable_h_DEFINED
#define xview_portable_h_DEFINED

#include "mem.h"

#if defined(__SVR4) && !defined(SVR4)
#define SVR4
#endif

#include <xview/attr.h>

#if defined(__STDC__) || defined(__cplusplus) || defined(c_plusplus)
#include <stdarg.h>
#define ANSI_FUNC_PROTO
#define VA_START( ptr, param )  va_start( ptr, param )
#else
#include <varargs.h>
#define VA_START( ptr, param )  va_start( ptr )
#endif


EXTERN_FUNCTION (Attr_avlist copy_va_to_av, (va_list valist, Attr_avlist avlist, Attr_attribute attr1));

#ifdef NO_CAST_VATOAV
#define AVLIST_DECL  Attr_attribute avarray[ATTR_STANDARD_SIZE];  \
                     Attr_avlist    avlist = avarray

#define MAKE_AVLIST( valist, avlist ) copy_va_to_av( valist, avlist, 0 )

#else
#define AVLIST_DECL  Attr_avlist  avlist

#define MAKE_AVLIST( valist, avlist )   \
        if( *((Attr_avlist)(valist)) == (Attr_attribute) ATTR_LIST )  \
        {  \
           Attr_attribute avarray[ATTR_STANDARD_SIZE];  \
           avlist = avarray;  \
           copy_va_to_av( valist, avlist, 0 );  \
        }  \
        else  \
           (avlist) = (Attr_avlist)(valist);
#endif

#define XV_BCOPY(a,b,c) bcopy(a,b,c)
#define XV_BZERO(a,b) bzero(a,b)
#define XV_INDEX(a,b) index(a,b)
#define XV_RINDEX(a,b) rindex(a,b)

/*
 * Defines governing tty mode and pty behavior.  (These are relevant to the
 * ttysw code.)
 */
#ifdef __linux
#define	XV_USE_TERMIOS
#undef	XV_USE_SVR4_PTYS
#else
#ifdef	SVR4
#define	XV_USE_TERMIOS
#define	XV_USE_SVR4_PTYS
#else	/* SVR4 */
#undef	XV_USE_TERMIOS
#undef	XV_USE_SVR4_PTYS
#endif	/* SVR4 */
#endif  /* __linux */

#endif /* xview_portable_h_DEFINED */

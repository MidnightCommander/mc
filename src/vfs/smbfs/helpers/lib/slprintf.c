/*
   Unix SMB/Netbios implementation.
   Version 1.9.
   snprintf replacement

   Copyright (C) Andrew Tridgell 1998

   Copyright (C) 2011-2017
   Free Software Foundation, Inc.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"

extern int DEBUGLEVEL;


/* this is like vsnprintf but the 'n' limit does not include
   the terminating null. So if you have a 1024 byte buffer then
   pass 1023 for n */
int
vslprintf (char *str, int n, const char *format, va_list ap)
{
    int ret = vsnprintf (str, n, format, ap);
    if (ret > n || ret < 0)
    {
        str[n] = 0;
        return -1;
    }
    str[ret] = 0;
    return ret;
}

#ifdef HAVE_STDARG_H
int
slprintf (char *str, int n, const char *format, ...)
{
#else
int
slprintf (va_alist)
     va_dcl
{
    char *str, *format;
    int n;
#endif
    va_list ap;
    int ret;

#ifdef HAVE_STDARG_H
    va_start (ap, format);
#else
    va_start (ap);
    str = va_arg (ap, char *);
    n = va_arg (ap, int);
    format = va_arg (ap, char *);
#endif

    ret = vslprintf (str, n, format, ap);
    va_end (ap);
    return ret;
}

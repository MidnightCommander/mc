/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Following code was copied from glib to GNU Midnight Commander to
 * provide compatibility with older versions of glib.
 */

#include <config.h>
#include <glib.h>
#include "glibcompat.h"

#if GLIB_MAJOR_VERSION < 2

/* Functions g_strlcpy and g_strlcat were originally developed by
 * Todd C. Miller <Todd.Miller@courtesan.com> to simplify writing secure code.
 * See ftp://ftp.openbsd.org/pub/OpenBSD/src/lib/libc/string/strlcpy.3
 * for more information.
 */

#ifdef HAVE_STRLCPY
/* Use the native ones, if available; they might be implemented in assembly */
gsize
g_strlcpy (gchar       *dest,
	   const gchar *src,
	   gsize        dest_size)
{
  g_return_val_if_fail (dest != NULL, 0);
  g_return_val_if_fail (src  != NULL, 0);
  
  return strlcpy (dest, src, dest_size);
}

gsize
g_strlcat (gchar       *dest,
	   const gchar *src,
	   gsize        dest_size)
{
  g_return_val_if_fail (dest != NULL, 0);
  g_return_val_if_fail (src  != NULL, 0);
  
  return strlcat (dest, src, dest_size);
}

#else /* ! HAVE_STRLCPY */
/* g_strlcpy
 *
 * Copy string src to buffer dest (of buffer size dest_size).  At most
 * dest_size-1 characters will be copied.  Always NUL terminates
 * (unless dest_size == 0).  This function does NOT allocate memory.
 * Unlike strncpy, this function doesn't pad dest (so it's often faster).
 * Returns size of attempted result, strlen(src),
 * so if retval >= dest_size, truncation occurred.
 */
gsize
g_strlcpy (gchar       *dest,
           const gchar *src,
           gsize        dest_size)
{
  register gchar *d = dest;
  register const gchar *s = src;
  register gsize n = dest_size;
  
  g_return_val_if_fail (dest != NULL, 0);
  g_return_val_if_fail (src  != NULL, 0);
  
  /* Copy as many bytes as will fit */
  if (n != 0 && --n != 0)
    do
      {
	register gchar c = *s++;
	
	*d++ = c;
	if (c == 0)
	  break;
      }
    while (--n != 0);
  
  /* If not enough room in dest, add NUL and traverse rest of src */
  if (n == 0)
    {
      if (dest_size != 0)
	*d = 0;
      while (*s++)
	;
    }
  
  return s - src - 1;  /* count does not include NUL */
}
#endif /* ! HAVE_STRLCPY */

#endif				/* GLIB_MAJOR_VERSION < 2 */


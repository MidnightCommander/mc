/* glibcompat.h -- declarations for functions decared in glibcompat.c,
   i.e. functions present only in recent versions of glib.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef __MC_GLIBCOMPAT_H
#define __MC_GLIBCOMPAT_H

#if GLIB_MAJOR_VERSION < 2
gsize g_strlcpy (gchar *dest, const gchar *src, gsize dest_size);
#define g_try_malloc(x) malloc(x)
#define g_try_realloc(x) realloc(x)
#endif				/* GLIB_MAJOR_VERSION < 2 */

#endif				/* !__MC_GLIBCOMPAT_H */

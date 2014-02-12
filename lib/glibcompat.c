/*
   GLIB - Library of useful routines for C programming

   Copyright (C) 2009-2014
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009, 2013.

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

/** \file glibcompat.c
 *  \brief Source: compatibility with older versions of glib
 *
 *  Following code was copied from glib to GNU Midnight Commander to
 *  provide compatibility with older versions of glib.
 */

#include <config.h>
#include <string.h>

#include "global.h"
#include "glibcompat.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

#if ! GLIB_CHECK_VERSION (2, 13, 0)
/*
   This is incomplete copy of same glib-function.
   For older glib (less than 2.13) functional is enought.
   For full version of glib welcome to glib update.
 */
gboolean
g_unichar_iszerowidth (gunichar c)
{
    if (G_UNLIKELY (c == 0x00AD))
        return FALSE;

    if (G_UNLIKELY ((c >= 0x1160 && c < 0x1200) || c == 0x200B))
        return TRUE;

    return FALSE;
}
#endif /* ! GLIB_CHECK_VERSION (2, 13, 0) */

/* --------------------------------------------------------------------------------------------- */

#if ! GLIB_CHECK_VERSION (2, 16, 0)
/**
 * g_strcmp0:
 * @str1: (allow-none): a C string or %NULL
 * @str2: (allow-none): another C string or %NULL
 *
 * Compares @str1 and @str2 like strcmp(). Handles %NULL
 * gracefully by sorting it before non-%NULL strings.
 * Comparing two %NULL pointers returns 0.
 *
 * Returns: an integer less than, equal to, or greater than zero, if @str1 is <, == or > than @str2.
 *
 * Since: 2.16
 */
int
g_strcmp0 (const char *str1, const char *str2)
{
    if (!str1)
        return -(str1 != str2);
    if (!str2)
        return str1 != str2;
    return strcmp (str1, str2);
}
#endif /* ! GLIB_CHECK_VERSION (2, 16, 0) */

/* --------------------------------------------------------------------------------------------- */

#if ! GLIB_CHECK_VERSION (2, 28, 0)
/**
 * g_slist_free_full:
 * @list: a pointer to a #GSList
 * @free_func: the function to be called to free each element's data
 *
 * Convenience method, which frees all the memory used by a #GSList, and
 * calls the specified destroy function on every element's data.
 *
 * Since: 2.28
 **/
void
g_slist_free_full (GSList * list, GDestroyNotify free_func)
{
    g_slist_foreach (list, (GFunc) free_func, NULL);
    g_slist_free (list);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * g_list_free_full:
 * @list: a pointer to a #GList
 * @free_func: the function to be called to free each element's data
 *
 * Convenience method, which frees all the memory used by a #GList, and
 * calls the specified destroy function on every element's data.
 *
 * Since: 2.28
 */
void
g_list_free_full (GList * list, GDestroyNotify free_func)
{
    g_list_foreach (list, (GFunc) free_func, NULL);
    g_list_free (list);
}

#endif /* ! GLIB_CHECK_VERSION (2, 28, 0) */

/* --------------------------------------------------------------------------------------------- */

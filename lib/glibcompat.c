/*
   GLIB - Library of useful routines for C programming

   Copyright (C) 2009-2019
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

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
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

/* --------------------------------------------------------------------------------------------- */

#endif /* ! GLIB_CHECK_VERSION (2, 28, 0) */

#if ! GLIB_CHECK_VERSION (2, 32, 0)
/**
 * g_queue_free_full:
 * @queue: a pointer to a #GQueue
 * @free_func: the function to be called to free each element's data
 *
 * Convenience method, which frees all the memory used by a #GQueue,
 * and calls the specified destroy function on every element's data.
 *
 * Since: 2.32
 */
void
g_queue_free_full (GQueue * queue, GDestroyNotify free_func)
{
    g_queue_foreach (queue, (GFunc) free_func, NULL);
    g_queue_free (queue);
}
#endif /* ! GLIB_CHECK_VERSION (2, 32, 0) */

/* --------------------------------------------------------------------------------------------- */

#if ! GLIB_CHECK_VERSION (2, 60, 0)
/**
 * g_queue_clear_full:
 * @queue: a pointer to a #GQueue
 * @free_func: (nullable): the function to be called to free memory allocated
 *
 * Convenience method, which frees all the memory used by a #GQueue,
 * and calls the provided @free_func on each item in the #GQueue.
 *
 * Since: 2.60
 */
void
g_queue_clear_full (GQueue * queue, GDestroyNotify free_func)
{
    g_return_if_fail (queue != NULL);

    if (free_func != NULL)
        g_queue_foreach (queue, (GFunc) free_func, NULL);

    g_queue_clear (queue);
}
#endif /* ! GLIB_CHECK_VERSION (2, 60, 0) */

/* --------------------------------------------------------------------------------------------- */

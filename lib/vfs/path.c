/* Virtual File System path handlers
   Copyright (C) 2011 Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011

   This file is part of the Midnight Commander.

   The Midnight Commander is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.
 */

/**
 * \file
 * \brief Source: Virtual File System: path handlers
 * \author Slava Zanko
 * \date 2011
 */


#include <config.h>

#include "lib/global.h"

#include "vfs.h"
#include "utilvfs.h"
#include "path.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

char *
vfs_path_to_str (const vfs_path_t * path)
{
    size_t element_index;
    GString *buffer;

    if (path == NULL)
        return NULL;

    buffer = g_string_new ("");

    for (element_index = 0; element_index < vfs_path_length (path); element_index++)
    {
        vfs_path_element_t *element = vfs_path_get_by_index (path, element_index);
        g_string_append (buffer, element->path);
    }
    return g_string_free (buffer, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

vfs_path_t *
vfs_path_from_str (const char *path_str)
{
    vfs_path_t *path;
    vfs_path_element_t *element;

    if (path_str == NULL)
        return NULL;

    path = vfs_path_new ();
    path->unparsed_encoding = g_strdup (vfs_get_encoding (path_str));

    path->unparsed = vfs_canon_and_translate (path_str);
    if (path->unparsed == NULL)
    {
        vfs_path_free (path);
        return NULL;
    }

    element = g_new0 (vfs_path_element_t, 1);
    element->class = vfs_get_class (path->unparsed);

    g_ptr_array_add (path->path, element);
    return path;
}

/* --------------------------------------------------------------------------------------------- */

vfs_path_t *
vfs_path_new (void)
{
    vfs_path_t *vpath;
    vpath = g_new0 (vfs_path_t, 1);
    vpath->path = g_ptr_array_new ();
    return vpath;
}

/* --------------------------------------------------------------------------------------------- */

size_t
vfs_path_length (const vfs_path_t * vpath)
{
    return vpath->path->len;
}

/* --------------------------------------------------------------------------------------------- */

vfs_path_element_t *
vfs_path_get_by_index (const vfs_path_t * path, size_t element_index)
{
    return g_ptr_array_index (path->path, element_index);
}

/* --------------------------------------------------------------------------------------------- */

void
vfs_path_element_free (vfs_path_element_t * element)
{
    if (element == NULL)
        return;

    g_free (element->path);
    g_free (element->encoding);
    g_free (element);
}

/* --------------------------------------------------------------------------------------------- */

void
vfs_path_free (vfs_path_t * path)
{
    if (path == NULL)
        return;

    g_ptr_array_foreach (path->path, (GFunc) vfs_path_element_free, NULL);
    g_ptr_array_free (path->path, TRUE);
    g_free (path->unparsed);
    g_free (path->unparsed_encoding);
    g_free (path);
}

/* --------------------------------------------------------------------------------------------- */

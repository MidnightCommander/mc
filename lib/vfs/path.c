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
#include "lib/strutil.h"
#include "lib/util.h"           /* concat_dir_and_file */

#include "vfs.h"
#include "utilvfs.h"
#include "path.h"

extern GPtrArray *vfs__classes_list;

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
path_magic (const char *path)
{
    struct stat buf;

    return (stat (path, &buf) != 0);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Splits path extracting vfs part.
 *
 * Splits path
 * \verbatim /p1#op/inpath \endverbatim
 * into
 * \verbatim inpath,op; \endverbatim
 * returns which vfs it is.
 * What is left in path is p1. You still want to g_free(path), you DON'T
 * want to free neither *inpath nor *op
 */

static struct vfs_class *
_vfs_split_with_semi_skip_count (char *path, const char **inpath, const char **op,
                                 size_t skip_count)
{
    char *semi;
    char *slash;
    struct vfs_class *ret;

    if (path == NULL)
        vfs_die ("Cannot split NULL");

    semi = strrstr_skip_count (path, "#", skip_count);

    if ((semi == NULL) || (!path_magic (path)))
        return NULL;

    slash = strchr (semi, PATH_SEP);
    *semi = '\0';

    if (op != NULL)
        *op = NULL;

    if (inpath != NULL)
        *inpath = NULL;

    if (slash != NULL)
        *slash = '\0';

    ret = vfs_prefix_to_class (semi + 1);
    if (ret != NULL)
    {
        if (op != NULL)
            *op = semi + 1;
        if (inpath != NULL)
            *inpath = slash != NULL ? slash + 1 : NULL;
        return ret;
    }

    if (slash != NULL)
        *slash = PATH_SEP;

    *semi = '#';
    ret = _vfs_split_with_semi_skip_count (path, inpath, op, skip_count + 1);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * remove //, /./ and /../
 *
 * @return newly allocated string
 */

static char *
vfs_canon (const char *path)
{
    if (!path)
        vfs_die ("Cannot canonicalize NULL");

    /* Relative to current directory */
    if (*path != PATH_SEP)
    {
        char *local, *result, *curr_dir;

        curr_dir = vfs_get_current_dir ();
        local = concat_dir_and_file (curr_dir, path);
        g_free (curr_dir);

        result = vfs_canon (local);
        g_free (local);
        return result;
    }

    /*
     * So we have path of following form:
     * /p1/p2#op/.././././p3#op/p4. Good luck.
     */
    {
        char *result = g_strdup (path);
        canonicalize_pathname (result);
        return result;
    }
}

/* --------------------------------------------------------------------------------------------- */
/** canonize and translate path
 *
 * @return new string
 */

static char *
vfs_canon_and_translate (const char *path)
{
    char *canon;
    char *result;
    if (path == NULL)
        canon = g_strdup ("");
    else
        canon = vfs_canon (path);
    result = vfs_translate_path_n (canon);
    g_free (canon);
    return result;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Convert first elements_count elements from vfs_path_t to string representation.
 *
 * @param vpath pointer to vfs_path_t object
 * @param elements_count count of first elements for convert
 *
 * @return pointer to newly created string.
 */

char *
vfs_path_to_str_elements_count (const vfs_path_t * vpath, int elements_count)
{
    int element_index;
    GString *buffer;

    if (vpath == NULL)
        return NULL;

    if (elements_count > vfs_path_elements_count (vpath))
        elements_count = vfs_path_elements_count (vpath);

    if (elements_count < 0)
        elements_count = vfs_path_elements_count (vpath) + elements_count;

    buffer = g_string_new ("");

    for (element_index = 0; element_index < elements_count; element_index++)
    {
        vfs_path_element_t *element = vfs_path_get_by_index (vpath, element_index);

        if (element->raw_url_str != NULL)
        {
            g_string_append (buffer, "#");
            g_string_append (buffer, element->raw_url_str);
        }
        if ((*element->path != PATH_SEP) && (*element->path != '\0'))
            g_string_append_c (buffer, PATH_SEP);
        g_string_append (buffer, element->path);
    }
    return g_string_free (buffer, FALSE);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Convert vfs_path_t to string representation.
 *
 * @param vpath pointer to vfs_path_t object
 *
 * @return pointer to newly created string.
 */
char *
vfs_path_to_str (const vfs_path_t * vpath)
{
    return vfs_path_to_str_elements_count (vpath, vfs_path_elements_count (vpath));
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Split path string to path elements.
 *
 * @param path_str VFS-path
 *
 * @return pointer to newly created vfs_path_t object with filled path elements array.
 */

vfs_path_t *
vfs_path_from_str (const char *path_str)
{
    vfs_path_t *vpath;
    vfs_path_element_t *element;
    struct vfs_class *class;
    const char *local, *op;
    char *path;

    if (path_str == NULL)
        return NULL;

    vpath = vfs_path_new ();
    vpath->unparsed = vfs_canon_and_translate (path_str);
    if (vpath->unparsed == NULL)
    {
        vfs_path_free (vpath);
        return NULL;
    }

    path = g_strdup (vpath->unparsed);
    while ((class = _vfs_split_with_semi_skip_count (path, &local, &op, 0)) != NULL)
    {
        element = g_new0 (vfs_path_element_t, 1);
        element->class = vfs_prefix_to_class (op);
        if (local == NULL)
            local = "";
        element->path = g_strdup (local);

        element->encoding = g_strdup (vfs_get_encoding (path));
        element->dir.converter = INVALID_CONV;

        element->raw_url_str = g_strdup (op);
        vpath->path = g_list_prepend (vpath->path, element);
    }
    if (path[0] != '\0')
    {
        element = g_new0 (vfs_path_element_t, 1);
        element->class = g_ptr_array_index (vfs__classes_list, 0);
        element->path = g_strdup (path);
        element->raw_url_str = NULL;
        vpath->path = g_list_prepend (vpath->path, element);
    }
    g_free (path);

    return vpath;
}

/* --------------------------------------------------------------------------------------------- */
/*
 * Create new vfs_path_t object.
 *
 * @return pointer to newly created vfs_path_t object.
 */

vfs_path_t *
vfs_path_new (void)
{
    vfs_path_t *vpath;
    vpath = g_new0 (vfs_path_t, 1);
    return vpath;
}

/* --------------------------------------------------------------------------------------------- */
/*
 * Get count of path elements.
 *
 * @param vpath pointer to vfs_path_t object
 *
 * @return count of path elements.
 */

int
vfs_path_elements_count (const vfs_path_t * vpath)
{
    if (vpath == NULL)
        return 0;

    return (vpath->path != NULL) ? g_list_length (vpath->path) : 0;
}

/* --------------------------------------------------------------------------------------------- */
/*
 * Get one path element by index.
 *
 * @param vpath pointer to vfs_path_t object
 * @param element_index element index. May have negative value (in this case count was started at the end of list).
 *
 * @return path element.
 */

vfs_path_element_t *
vfs_path_get_by_index (const vfs_path_t * vpath, int element_index)
{
    if (vpath == NULL)
        return NULL;

    if (element_index >= 0)
        return g_list_nth_data (vpath->path, element_index);

    return g_list_nth_data (vpath->path, vfs_path_elements_count (vpath) + element_index);
}

/* --------------------------------------------------------------------------------------------- */
/*
 * Free one path element.
 *
 * @param element pointer to vfs_path_element_t object
 *
 */

void
vfs_path_element_free (vfs_path_element_t * element)
{
    if (element == NULL)
        return;

    vfs_url_free (element->url);
    g_free (element->path);
    g_free (element->encoding);
    g_free (element);
}

/* --------------------------------------------------------------------------------------------- */
/*
 * Free vfs_path_t object.
 *
 * @param vpath pointer to vfs_path_t object
 *
 */

void
vfs_path_free (vfs_path_t * path)
{
    if (path == NULL)
        return;
    g_list_foreach (path->path, (GFunc) vfs_path_element_free, NULL);
    g_list_free (path->path);
    g_free (path->unparsed);
    g_free (path);
}

/* --------------------------------------------------------------------------------------------- */

/** Return VFS class for the given prefix */
struct vfs_class *
vfs_prefix_to_class (const char *prefix)
{
    guint i;

    /* Avoid first class (localfs) that would accept any prefix */
    for (i = 1; i < vfs__classes_list->len; i++)
    {
        struct vfs_class *vfs = (struct vfs_class *) g_ptr_array_index (vfs__classes_list, i);
        if (vfs->which != NULL)
        {
            if (vfs->which (vfs, prefix) == -1)
                continue;
            return vfs;
        }

        if (vfs->prefix != NULL && strncmp (prefix, vfs->prefix, strlen (vfs->prefix)) == 0)
            return vfs;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

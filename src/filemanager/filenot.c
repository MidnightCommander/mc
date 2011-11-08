/*
   Wrapper for routines to notify the
   tree about the changes made to the directory
   structure.

   Copyright (C) 2011
   The Free Software Foundation, Inc.

   Author:
   Janne Kukonlehto
   Miguel de Icaza

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


/** \file  filenot.c
 *  \brief Source: wrapper for routines to notify the
 *  tree about the changes made to the directory
 *  structure.
 */

#include <config.h>

#include <errno.h>
#include <string.h>

#include "lib/global.h"
#include "lib/fs.h"
#include "lib/util.h"
#include "lib/vfs/vfs.h"

#include "filenot.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static vfs_path_t *
get_absolute_name (const vfs_path_t * vpath)
{
    if (vpath == NULL)
        return NULL;

    if (*(vfs_path_get_by_index (vpath, 0)->path) == PATH_SEP)
        return vfs_path_clone (vpath);

    return vfs_path_append_vpath_new (vfs_get_raw_current_dir (), vpath);
}

/* --------------------------------------------------------------------------------------------- */

static int
my_mkdir_rec (char *s, mode_t mode)
{
    char *p, *q;
    int result;
    vfs_path_t *s_vpath;

    s_vpath = vfs_path_from_str (s);
    if (mc_mkdir (s_vpath, mode) == 0)
    {
        vfs_path_free (s_vpath);
        return 0;
    }
    else if (errno != ENOENT)
    {
        vfs_path_free (s_vpath);
        return -1;
    }

    /* FIXME: should check instead if s is at the root of that filesystem */
    {
        if (!vfs_file_is_local (s_vpath))
        {
            vfs_path_free (s_vpath);
            return -1;
        }
    }

    if (!strcmp (s, PATH_SEP_STR))
    {
        errno = ENOTDIR;
        vfs_path_free (s_vpath);
        return -1;
    }

    p = mc_build_filename (s, "..", NULL);
    {
        vfs_path_t *vpath;

        vpath = vfs_path_from_str (p);
        q = vfs_path_to_str (vpath);
        vfs_path_free (vpath);
    }
    g_free (p);

    result = my_mkdir_rec (q, mode);
    if (result == 0)
        result = mc_mkdir (s_vpath, mode);

    vfs_path_free (s_vpath);
    g_free (q);
    return result;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
my_mkdir (const vfs_path_t * s_vpath, mode_t mode)
{
    int result;

    result = mc_mkdir (s_vpath, mode);

    if (result != 0)
    {
        char *p;

        p = vfs_path_to_str (s_vpath);
        result = my_mkdir_rec (p, mode);
        g_free (p);
    }
    if (result == 0)
    {
        vfs_path_t *my_s;

        my_s = get_absolute_name (s_vpath);
#ifdef FIXME
        tree_add_entry (tree, my_s);
#endif
        vfs_path_free (my_s);
    }
    return result;
}

/* --------------------------------------------------------------------------------------------- */

int
my_rmdir (const char *s)
{
    int result;
    vfs_path_t *vpath;
#ifdef FIXME
    WTree *tree = 0;
#endif

    vpath = vfs_path_from_str_flags (s, VPF_NO_CANON);
    /* FIXME: Should receive a Wtree! */
    result = mc_rmdir (vpath);
    if (result == 0)
    {
        vfs_path_t *my_s;

        my_s = get_absolute_name (vpath);
#ifdef FIXME
        tree_remove_entry (tree, my_s);
#endif
        vfs_path_free (my_s);
    }
    vfs_path_free (vpath);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

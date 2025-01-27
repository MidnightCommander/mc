/*
   Wrapper for routines to notify the
   tree about the changes made to the directory
   structure.

   Copyright (C) 2011-2025
   Free Software Foundation, Inc.

   Author:
   Janne Kukonlehto
   Miguel de Icaza
   Slava Zanko <slavazanko@gmail.com>, 2013

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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
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

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
my_mkdir_rec (const vfs_path_t *vpath, mode_t mode)
{
    vfs_path_t *q;
    int result;

    if (mc_mkdir (vpath, mode) == 0)
        return 0;
    if (errno != ENOENT)
        return (-1);

    // FIXME: should check instead if vpath is at the root of that filesystem
    if (!vfs_file_is_local (vpath))
        return (-1);

    if (strcmp (vfs_path_as_str (vpath), PATH_SEP_STR) == 0)
    {
        errno = ENOTDIR;
        return (-1);
    }

    q = vfs_path_append_new (vpath, "..", (char *) NULL);
    result = my_mkdir_rec (q, mode);
    vfs_path_free (q, TRUE);

    if (result == 0)
        result = mc_mkdir (vpath, mode);

    return result;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
my_mkdir (const vfs_path_t *vpath, mode_t mode)
{
    int result;

    result = my_mkdir_rec (vpath, mode);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

int
my_rmdir (const char *path)
{
    int result;
    vfs_path_t *vpath;

    vpath = vfs_path_from_str_flags (path, VPF_NO_CANON);
    // FIXME: Should receive a Wtree!
    result = mc_rmdir (vpath);
    vfs_path_free (vpath, TRUE);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

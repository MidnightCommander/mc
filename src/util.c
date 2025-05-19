/*
   Various non-library utilities

   Copyright (C) 2003-2025
   Free Software Foundation, Inc.

   Written by:
   Adam Byrtek, 2003
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

#include <config.h>

#include <errno.h>

#include "lib/global.h"
#include "lib/util.h"
#include "lib/widget.h"

#include "src/filemanager/file.h"
#include "src/filemanager/filegui.h"

#include "util.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
check_for_default (const vfs_path_t *default_file_vpath, const vfs_path_t *file_vpath)
{
    if (!exist_file (vfs_path_as_str (file_vpath)))
    {
        file_op_context_t *ctx;

        if (!exist_file (vfs_path_as_str (default_file_vpath)))
            return FALSE;

        ctx = file_op_context_new (OP_COPY);
        file_progress_ui_create (ctx, 0, FALSE);
        copy_file_file (ctx, vfs_path_as_str (default_file_vpath), vfs_path_as_str (file_vpath));
        file_op_context_destroy (ctx);
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Report error with one file using errno. Unlike file_error(), this function contains only one
 * button "OK" and returns nothing.
 *
 * @param format printf()-like format for message
 * @param file file name. Can be NULL.
 */

void
file_error_message (const char *format, const char *filename)
{
    const char *error_string = unix_error_string (errno);

    if (filename == NULL || *filename == '\0')
        message (D_ERROR, MSG_ERROR, "%s\n%s", format, error_string);
    else
    {
        char *full_format;

        full_format = g_strconcat (format, "\n", error_string, (char *) NULL);
        // delete password and try to show a full path
        message (D_ERROR, MSG_ERROR, full_format, path_trunc (filename, -1));
        g_free (full_format);
    }
}

/* --------------------------------------------------------------------------------------------- */

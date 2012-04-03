/*
   Various non-library utilities

   Copyright (C) 2003, 2004, 2005, 2006, 2007, 2011
   The Free Software Foundation, Inc.

   Written by:
   Adam Byrtek, 2003

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

#include <config.h>

#include "lib/global.h"
#include "lib/util.h"

#include "src/filemanager/file.h"
#include "src/filemanager/filegui.h"

#include "util.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
check_for_default (const vfs_path_t * default_file_vpath, const vfs_path_t * file_vpath)
{
    char *file;

    file = vfs_path_to_str (file_vpath);

    if (!exist_file (file))
    {
        char *default_file;
        FileOpContext *ctx;
        FileOpTotalContext *tctx;

        default_file = vfs_path_to_str (default_file_vpath);
        if (!exist_file (default_file))
        {
            g_free (file);
            g_free (default_file);
            return FALSE;
        }

        ctx = file_op_context_new (OP_COPY);
        tctx = file_op_total_context_new ();
        file_op_context_create_ui (ctx, 0, FALSE);
        copy_file_file (tctx, ctx, default_file, file);
        file_op_total_context_destroy (tctx);
        file_op_context_destroy (ctx);
        g_free (default_file);
    }

    g_free (file);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

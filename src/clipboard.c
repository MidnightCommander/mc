/*
   Util for external clipboard.

   Copyright (C) 2009 The Free Software Foundation, Inc.

   Written by:
   Ilia Maslakon <il.smind@gmail.com>, 2010.

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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include "lib/global.h"
#include "lib/fileloc.h"
#include "lib/util.h"

#include "main.h"
#include "src/execute.h"

#include "clipboard.h"

/*** global variables ****************************************************************************/

/* path to X clipboard utility */
char *clipboard_store_path = NULL;
char *clipboard_paste_path = NULL;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
copy_file_to_ext_clip (void)
{
    char *tmp, *cmd;
    int res = 0;
    const char *d = getenv ("DISPLAY");

    if (d == NULL || clipboard_store_path == NULL || clipboard_store_path[0] == '\0')
        return FALSE;

    tmp = concat_dir_and_file (home_dir, EDIT_CLIP_FILE);
    cmd = g_strconcat (clipboard_store_path, " ", tmp, " 2>/dev/null", (char *) NULL);

    if (cmd != NULL)
        res = my_system (EXECUTE_AS_SHELL, shell, cmd);

    g_free (cmd);
    g_free (tmp);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
paste_to_file_from_ext_clip (void)
{
    char *tmp, *cmd;
    int res = 0;
    const char *d = getenv ("DISPLAY");

    if (d == NULL || clipboard_paste_path == NULL || clipboard_paste_path[0] == '\0')
        return FALSE;

    tmp = concat_dir_and_file (home_dir, EDIT_CLIP_FILE);
    cmd = g_strconcat (clipboard_paste_path, " > ", tmp, " 2>/dev/null", (char *) NULL);

    if (cmd != NULL)
        res = my_system (EXECUTE_AS_SHELL, shell, cmd);

    g_free (cmd);
    g_free (tmp);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

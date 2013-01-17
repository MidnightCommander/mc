/*
   Util for external clipboard.

   Copyright (C) 2009, 2011
   The Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2010.

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

#include <stdio.h>
#include <stdlib.h>

#include "lib/global.h"
#include "lib/fileloc.h"
#include "lib/mcconfig.h"
#include "lib/util.h"
#include "lib/event.h"

#include "lib/vfs/vfs.h"

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

/* event callback */
gboolean
clipboard_file_to_ext_clip (const gchar * event_group_name, const gchar * event_name,
                            gpointer init_data, gpointer data)
{
    char *tmp, *cmd;
    const char *d = getenv ("DISPLAY");

    (void) event_group_name;
    (void) event_name;
    (void) init_data;
    (void) data;

    if (d == NULL || clipboard_store_path == NULL || clipboard_store_path[0] == '\0')
        return TRUE;

    tmp = mc_config_get_full_path (EDIT_CLIP_FILE);
    cmd = g_strconcat (clipboard_store_path, " ", tmp, " 2>/dev/null", (char *) NULL);

    if (cmd != NULL)
        my_system (EXECUTE_AS_SHELL, mc_global.tty.shell, cmd);

    g_free (cmd);
    g_free (tmp);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* event callback */
gboolean
clipboard_file_from_ext_clip (const gchar * event_group_name, const gchar * event_name,
                              gpointer init_data, gpointer data)
{
    char *tmp, *cmd;
    const char *d = getenv ("DISPLAY");

    (void) event_group_name;
    (void) event_name;
    (void) init_data;
    (void) data;

    if (d == NULL || clipboard_paste_path == NULL || clipboard_paste_path[0] == '\0')
        return TRUE;

    tmp = mc_config_get_full_path (EDIT_CLIP_FILE);
    cmd = g_strconcat (clipboard_paste_path, " > ", tmp, " 2>/dev/null", (char *) NULL);

    if (cmd != NULL)
        my_system (EXECUTE_AS_SHELL, mc_global.tty.shell, cmd);

    g_free (cmd);
    g_free (tmp);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* event callback */
gboolean
clipboard_text_to_file (const gchar * event_group_name, const gchar * event_name,
                        gpointer init_data, gpointer data)
{
    int file;
    vfs_path_t *fname_vpath = NULL;
    size_t str_len;
    const char *text = (const char *) data;

    (void) event_group_name;
    (void) event_name;
    (void) init_data;

    if (text == NULL)
        return FALSE;

    fname_vpath = mc_config_get_full_vpath (EDIT_CLIP_FILE);
    file = mc_open (fname_vpath, O_CREAT | O_WRONLY | O_TRUNC,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | O_BINARY);
    vfs_path_free (fname_vpath);

    if (file == -1)
        return TRUE;

    str_len = strlen (text);
    {
        ssize_t ret;

        ret = mc_write (file, (char *) text, str_len);
        (void) ret;
    }
    mc_close (file);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* event callback */
gboolean
clipboard_text_from_file (const gchar * event_group_name, const gchar * event_name,
                          gpointer init_data, gpointer data)
{
    char buf[BUF_LARGE];
    FILE *f;
    char *fname = NULL;
    gboolean first = TRUE;
    ev_clipboard_text_from_file_t *event_data = (ev_clipboard_text_from_file_t *) data;

    (void) event_group_name;
    (void) event_name;
    (void) init_data;

    fname = mc_config_get_full_path (EDIT_CLIP_FILE);
    f = fopen (fname, "r");
    g_free (fname);

    if (f == NULL)
    {
        event_data->ret = FALSE;
        return TRUE;
    }

    *(event_data->text) = NULL;

    while (fgets (buf, sizeof (buf), f))
    {
        size_t len;

        len = strlen (buf);
        if (len > 0)
        {
            if (buf[len - 1] == '\n')
                buf[len - 1] = '\0';

            if (first)
            {
                first = FALSE;
                *(event_data->text) = g_strdup (buf);
            }
            else
            {
                /* remove \n on EOL */
                char *tmp;

                tmp = g_strconcat (*(event_data->text), " ", buf, (char *) NULL);
                g_free (*(event_data->text));
                *(event_data->text) = tmp;
            }
        }
    }

    fclose (f);
    event_data->ret = (*(event_data->text) != NULL);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/*
   Load and show history of edited and viewed files

   Copyright (C) 2019
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2019.

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

#include <stdio.h>              /* file functions */

#include "lib/global.h"

#include "lib/fileloc.h"        /* MC_FILEPOS_FILE */
#include "lib/mcconfig.h"       /* mc_config_get_full_path() */

#include "file_history.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static GList *
file_history_list_read (void)
{
    char *fn;
    FILE *f;
    char buf[MC_MAXPATHLEN + 100];
    GList *file_list = NULL;

    /* open file with positions */
    fn = mc_config_get_full_path (MC_FILEPOS_FILE);
    if (fn == NULL)
        return NULL;

    f = fopen (fn, "r");
    g_free (fn);
    if (f == NULL)
        return NULL;

    while (fgets (buf, sizeof (buf), f) != NULL)
    {
        char *s;

        s = strrchr (buf, ' ');
        if (s != NULL)
            s = g_strndup (buf, s - buf);
        else
            s = g_strdup (buf);

        file_list = g_list_prepend (file_list, s);
    }

    fclose (f);

    return file_list;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Show file history and return the selected file
 *
 * @param w widget used for positioning of history window
 * @param action to do with file (edit, view, etc)
 *
 * @return name of selected file, A newly allocated string.
 */
char *
show_file_history (const Widget * w, int *action)
{
    GList *file_list;

    file_list = file_history_list_read ();
    if (file_list == NULL)
        return NULL;

    file_list = g_list_last (file_list);
    s = history_show (&file_list, w, 0, action);
    file_list = g_list_first (file_list);
    g_list_free_full (file_list, (GDestroyNotify) g_free);

    return s;
}

/* --------------------------------------------------------------------------------------------- */

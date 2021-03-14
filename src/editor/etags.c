/*
   Editor C-code navigation via tags.
   make TAGS file via command:
   $ find . -type f -name "*.[ch]" | etags -l c --declarations -

   or, if etags utility not installed:
   $ find . -type f -name "*.[ch]" | ctags --c-kinds=+p --fields=+iaS --extra=+q -e -L-

   Copyright (C) 2009-2021
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2009
   Slava Zanko <slavazanko@gmail.com>, 2009

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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/global.h"
#include "lib/util.h"           /* canonicalize_pathname() */

#include "etags.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
etags_hash_free (gpointer data)
{
    etags_hash_t *hash = (etags_hash_t *) data;

    g_free (hash->filename);
    g_free (hash->fullpath);
    g_free (hash->short_define);
    g_free (hash);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
parse_define (const char *buf, char **long_name, char **short_name, long *line)
{
    /* *INDENT-OFF* */
    enum
    {
        in_longname,
        in_shortname,
        in_shortname_first_char,
        in_line,
        finish
    } def_state = in_longname;
    /* *INDENT-ON* */

    GString *longdef = NULL;
    GString *shortdef = NULL;
    GString *linedef = NULL;

    char c = *buf;

    while (!(c == '\0' || c == '\n'))
    {
        switch (def_state)
        {
        case in_longname:
            if (c == 0x01)
                def_state = in_line;
            else if (c == 0x7F)
                def_state = in_shortname;
            else
            {
                if (longdef == NULL)
                    longdef = g_string_sized_new (32);

                g_string_append_c (longdef, c);
            }
            break;

        case in_shortname_first_char:
            if (isdigit (c))
            {
                if (shortdef == NULL)
                    shortdef = g_string_sized_new (32);
                else
                    g_string_set_size (shortdef, 0);

                buf--;
                def_state = in_line;
            }
            else if (c == 0x01)
                def_state = in_line;
            else
            {
                if (shortdef == NULL)
                    shortdef = g_string_sized_new (32);

                g_string_append_c (shortdef, c);
                def_state = in_shortname;
            }
            break;

        case in_shortname:
            if (c == 0x01)
                def_state = in_line;
            else if (c == '\n')
                def_state = finish;
            else
            {
                if (shortdef == NULL)
                    shortdef = g_string_sized_new (32);

                g_string_append_c (shortdef, c);
            }
            break;

        case in_line:
            if (c == ',' || c == '\n')
                def_state = finish;
            else if (isdigit (c))
            {
                if (linedef == NULL)
                    linedef = g_string_sized_new (32);

                g_string_append_c (linedef, c);
            }
            break;

        case finish:
            *long_name = longdef == NULL ? NULL : g_string_free (longdef, FALSE);
            *short_name = shortdef == NULL ? NULL : g_string_free (shortdef, FALSE);

            if (linedef == NULL)
                *line = 0;
            else
            {
                *line = atol (linedef->str);
                g_string_free (linedef, TRUE);
            }
            return TRUE;

        default:
            break;
        }

        buf++;
        c = *buf;
    }

    *long_name = NULL;
    *short_name = NULL;
    *line = 0;

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

GPtrArray *
etags_set_definition_hash (const char *tagfile, const char *start_path, const char *match_func)
{
    /* *INDENT-OFF* */
    enum
    {
        start,
        in_filename,
        in_define
    } state = start;
    /* *INDENT-ON* */

    FILE *f;
    char buf[BUF_LARGE];
    char *filename = NULL;
    GPtrArray *ret = NULL;

    if (match_func == NULL || tagfile == NULL)
        return NULL;

    /* open file with positions */
    f = fopen (tagfile, "r");
    if (f == NULL)
        return NULL;

    while (fgets (buf, sizeof (buf), f) != NULL)
        switch (state)
        {
        case start:
            if (buf[0] == 0x0C)
                state = in_filename;
            break;

        case in_filename:
            {
                size_t pos;

                pos = strcspn (buf, ",");
                g_free (filename);
                filename = g_strndup (buf, pos);
                state = in_define;
                break;
            }

        case in_define:
            if (buf[0] == 0x0C)
                state = in_filename;
            else
            {
                char *chekedstr;

                /* check if the filename matches the define pos */
                chekedstr = strstr (buf, match_func);
                if (chekedstr != NULL)
                {
                    char *longname = NULL;
                    char *shortname = NULL;
                    etags_hash_t *def_hash;

                    def_hash = g_new (etags_hash_t, 1);

                    def_hash->fullpath = mc_build_filename (start_path, filename, (char *) NULL);
                    canonicalize_pathname (def_hash->fullpath);
                    def_hash->filename = g_strdup (filename);

                    def_hash->line = 0;

                    parse_define (chekedstr, &longname, &shortname, &def_hash->line);

                    if (shortname != NULL && *shortname != '\0')
                    {
                        def_hash->short_define = shortname;
                        g_free (longname);
                    }
                    else
                    {
                        def_hash->short_define = longname;
                        g_free (shortname);
                    }

                    if (ret == NULL)
                        ret = g_ptr_array_new_with_free_func (etags_hash_free);

                    g_ptr_array_add (ret, def_hash);
                }
            }
            break;

        default:
            break;
        }

    g_free (filename);
    fclose (f);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

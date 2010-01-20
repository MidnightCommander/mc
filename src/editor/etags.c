/* editor C-code navigation via tags.
   make TAGS file via command:
   $ find . -type f -name "*.[ch]" | etags -l c --declarations -

   or, if etags utility not installed:
   $ find . -type f -name "*.[ch]" | ctags --c-kinds=+p --fields=+iaS --extra=+q -e -L-

   Copyright (C) 2009 Free Software Foundation, Inc.

   Authors:
    Ilia Maslakov <il.smind@gmail.com>, 2009
    Slava Zanko <slavazanko@gmail.com>, 2009


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

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <ctype.h>

#include "lib/global.h"
#include "src/util.h"	/* canonicalize_pathname() */
#include "src/editor/etags.h"

/*** file scope functions **********************************************/

static gboolean parse_define(char *buf, char **long_name, char **short_name, long *line)
{
    enum {in_longname, in_shortname, in_shortname_first_char, in_line, finish} def_state = in_longname;

    static char longdef[LONG_DEF_LEN];
    static char shortdef[SHORT_DEF_LEN];
    static char linedef[LINE_DEF_LEN];
    int nlong = 0;
    int nshort = 0;
    int nline = 0;
    char c = *buf;

    while ( !(c =='\0' || c =='\n') ) {
        switch ( def_state ) {
        case in_longname:
            if ( c == 0x01 ) {
                def_state = in_line;
            } else if ( c == 0x7F ) {
                def_state = in_shortname;
            } else {
                if ( nlong < LONG_DEF_LEN - 1 ) {
                    longdef[nlong++] = c;
                }
            }
            break;
        case in_shortname_first_char:
            if ( isdigit(c) ) {
                nshort = 0;
                buf--;
                def_state = in_line;
            } else if ( c == 0x01 ) {
                def_state = in_line;
            } else {
                if ( nshort < SHORT_DEF_LEN - 1 ) {
                    shortdef[nshort++] = c;
                    def_state = in_shortname;
                }
            }
            break;
        case in_shortname:
            if ( c == 0x01 ) {
                def_state = in_line;
            } else if ( c == '\n' ) {
                def_state = finish;
            } else {
                if ( nshort < SHORT_DEF_LEN - 1 ) {
                    shortdef[nshort++] = c;
                }
            }
            break;
        case in_line:
            if ( c == ',' || c == '\n') {
                def_state = finish;
            } else if ( isdigit(c) ) {
                if ( nline < LINE_DEF_LEN - 1 ) {
                    linedef[nline++] = c;
                }
            }
            break;
        case finish:
            longdef[nlong] = '\0';
            shortdef[nshort] = '\0';
            linedef[nline] = '\0';
            *long_name = g_strdup (longdef);
            *short_name = g_strdup (shortdef);
            *line = atol (linedef);
            return TRUE;
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

/*** public functions **************************************************/

int etags_set_definition_hash(const char *tagfile, const char *start_path,
                              const char *match_func,
                              etags_hash_t *def_hash)
{
    enum {start, in_filename, in_define} state = start;

    FILE *f;
    static char buf[BUF_LARGE];

    char *longname = NULL;
    char *shortname = NULL;
    long line;

    char *chekedstr = NULL;

    int num = 0; /* returned value */
    int pos;
    char *filename = NULL;

    if ( !match_func || !tagfile )
        return 0;

    /* open file with positions */
    f = fopen (tagfile, "r");
    if (f == NULL)
        return 0;

    while (fgets (buf, sizeof (buf), f)) {

        switch ( state ) {
        case start:
            if ( buf[0] == 0x0C ) {
                state = in_filename;
            }
            break;
        case in_filename:
            pos = strcspn(buf, ",");
            g_free(filename);
            filename = g_malloc (pos + 2);
            g_strlcpy(filename, (char *)buf, pos + 1);
            state = in_define;
            break;
        case in_define:
            if ( buf[0] == 0x0C ) {
                state = in_filename;
                break;
            }
            /* check if the filename matches the define pos */
            chekedstr = strstr (buf, match_func);
            if ( chekedstr ) {
                parse_define (chekedstr, &longname, &shortname, &line);
                if ( num < MAX_DEFINITIONS - 1 ) {
                    def_hash[num].filename_len = strlen (filename);
                    def_hash[num].fullpath =  g_build_filename ( start_path, filename, (char *) NULL);

                    canonicalize_pathname (def_hash[num].fullpath);
                    def_hash[num].filename = g_strdup (filename);
                    if ( shortname ) {
                        def_hash[num].short_define = g_strdup (shortname);
                    } else {
                        def_hash[num].short_define = g_strdup (longname);
                    }
                    def_hash[num].line = line;
                    g_free(shortname);
                    g_free(longname);
                    num++;
                }
            }
            break;
        }
    }

    g_free(filename);
    fclose (f);
    return num;
}

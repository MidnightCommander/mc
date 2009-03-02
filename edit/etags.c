/* editor C-code navigation via tags.
   make TAGS file via command:
   $ find . -type f -name "*.[ch]" | etags -l c --declarations - 

   or, if etags utility not installed:
   $ ctags --languages=c -e -R -h '[ch]'

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
#include <glib.h>
#include <ctype.h>

#include "../src/global.h"
#include "../edit/etags.h"

/*** global variables **************************************************/

/*** file scope macro definitions **************************************/

/*** file scope type declarations **************************************/

/*** file scope variables **********************************************/

/*** file scope functions **********************************************/

static long etags_get_pos_from(char *str)
{
    static char buf[16];
    int i, j;
    j = 0;
    int len = strlen( str );
    for ( i = 0; i < len && i < 16; i++ ) {
        char c = (char) str[i];
        if ( isdigit (c) ) {
            buf[j++] = c;
            buf[j] = '\0';
        } else {
            return atol((char *)buf);
        }
    }
    return 0;
}

/*** public functions **************************************************/


int etags_set_def_hash(char *tagfile, char *start_path, char *match_func, struct def_hash_type *def_hash, int *num)
{
    FILE *f;
    static char buf[4048];
    int len;

    f = fopen (tagfile, "r");
    int i;
    if (!f)
        return 1;
    len = strlen( match_func );
    int pos;

    char *fullpath = NULL;
    char *filename = NULL;
    long line;
    enum {start, in_filename, in_define} state = start;

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
            filename = malloc(pos + 2);
            g_strlcpy ( filename, (char *)buf, pos + 1 );
            state = in_define;
            break;
        case in_define:
            if ( buf[0] == 0x0C ) {
                state = in_filename;
                break;
            }
            /* check if the filename matches the define pos */
            if ( strstr ( buf, match_func ) ) {
                int l = (int)strlen( buf );
                for ( i = 0; i < l; i++) {
                    if ( ( buf[i] == 0x7F || buf[i] == 0x01 ) && isdigit(buf[i+1]) ) {
                        line = etags_get_pos_from(&buf[i+1]);
                        state = start;
                        if ( *num < MAX_DEFINITIONS ) {
                            def_hash[*num].filename_len = strlen(filename);
                            fullpath = g_strdup_printf("%s/%s",start_path, filename);
                            canonicalize_pathname (fullpath);
                            def_hash[*num].filename = g_strdup(fullpath);
                            g_free(fullpath);
                            def_hash[*num].line = line;
                            (*num)++;
                        }
                    }
                }
            }
            break;
        }
    }
    g_free(filename);
    return 0;
}

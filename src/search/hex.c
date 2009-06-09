/*
   Search text engine.
   HEX-style pattern matching

   Copyright (C) 2009 The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009.

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

#include "../src/global.h"
#include "../src/search/search.h"
#include "../src/search/internal.h"
#include "../src/strutil.h"
#include "../src/strescape.h"
#include "../src/charsets.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static GString *
mc_search__hex_translate_to_regex (gchar * str, gsize * len)
{
    GString *buff = g_string_new ("");
    gchar *tmp_str = g_strndup (str, *len);
    gchar *tmp_str2;
    gsize loop = 0;
    int val, ptr;

    g_strchug (tmp_str);        /* trim leadind whitespaces */

    while (loop < *len) {
        if (sscanf (tmp_str + loop, "%i%n", &val, &ptr)) {
            if (val < -128 || val > 255) {
                loop++;
                continue;
            }
            tmp_str2 = g_strdup_printf ("\\x%02X", (unsigned char) val);
            g_string_append (buff, tmp_str2);
            g_free (tmp_str2);
            loop += ptr;
            continue;
        }

        if (*(tmp_str + loop) == '"') {
            loop++;
            gsize loop2 = 0;
            while (loop + loop2 < *len) {
                if (*(tmp_str + loop + loop2) == '"' &&
                    !strutils_is_char_escaped (tmp_str, tmp_str + loop + loop2 ))
                    break;
                loop2++;
            }
            g_string_append_len (buff, tmp_str + loop, loop2 - 1);
            loop += loop2;
            continue;
        }
        loop++;
    }
    *len = buff->len;
    return buff;
}

/*** public functions ****************************************************************************/

void
mc_search__cond_struct_new_init_hex (const char *charset, mc_search_t * mc_search,
                                     mc_search_cond_t * mc_search_cond)
{
    GString *tmp =
        mc_search__hex_translate_to_regex (mc_search_cond->str->str, &mc_search_cond->len);

    g_string_free (mc_search_cond->str, TRUE);
    mc_search_cond->str = tmp;

    mc_search__cond_struct_new_init_regex (charset, mc_search, mc_search_cond);

}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_search__run_hex (mc_search_t * mc_search, const void *user_data,
                    gsize start_search, gsize end_search, gsize * found_len)
{
    return mc_search__run_regex (mc_search, user_data, start_search, end_search, found_len);
}

/* --------------------------------------------------------------------------------------------- */

GString *
mc_search_hex_prepare_replace_str (mc_search_t * mc_search, GString * replace_str)
{
    (void) mc_search;
    return g_string_new_len (replace_str->str, replace_str->len);
}

/* --------------------------------------------------------------------------------------------- */

/*
   Search text engine.
   Glob-style pattern matching

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


#include "../src/global.h"
#include "../src/search/search.h"
#include "../src/search/internal.h"
#include "../src/strutil.h"
#include "../src/charsets.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static GString *
mc_search__glob_translate_to_regex (gchar * str, gsize * len)
{
    GString *buff = g_string_new ("");
    gsize orig_len = *len;
    gsize loop = 0;

    while (loop < orig_len) {
        switch (str[loop]) {
        case '*':
            if (!mc_search__regex_is_char_escaped (str, &(str[loop]) - 1)) {
                g_string_append (buff, ".*");
                loop++;
                continue;
            }
            break;
        case '?':
            if (!mc_search__regex_is_char_escaped (str, &(str[loop]) - 1)) {
                g_string_append (buff, ".");
                loop++;
                continue;
            }
            break;
        case ',':
            if (!mc_search__regex_is_char_escaped (str, &(str[loop]) - 1)) {
                g_string_append (buff, "|");
                loop++;
                continue;
            }
            break;
        case '{':
            if (!mc_search__regex_is_char_escaped (str, &(str[loop]) - 1)) {
                g_string_append (buff, "(");
                loop++;
                continue;
            }
            break;
        case '}':
            if (!mc_search__regex_is_char_escaped (str, &(str[loop]) - 1)) {
                g_string_append (buff, ")");
                loop++;
                continue;
            }
            break;
        case '\\':
        case '+':
        case '.':
        case '$':
        case '(':
        case ')':
        case '^':
        case '-':
            g_string_append_c (buff, '\\');
            g_string_append_c (buff, str[loop]);
            loop++;
            continue;
            break;
        }
        g_string_append_c (buff, str[loop]);
        loop++;
    }
    *len = buff->len;
    return buff;
}

/*** public functions ****************************************************************************/

void
mc_search__cond_struct_new_init_glob (const char *charset, mc_search_t * mc_search,
                                      mc_search_cond_t * mc_search_cond)
{
    GString *tmp = mc_search__glob_translate_to_regex (mc_search_cond->str->str, &mc_search_cond->len);

    g_string_free (mc_search_cond->str, TRUE);
    mc_search_cond->str = tmp;

    mc_search__cond_struct_new_init_regex (charset, mc_search, mc_search_cond);

}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_search__run_glob (mc_search_t * mc_search, const void *user_data,
                     gsize start_search, gsize end_search, gsize * found_len)
{
    return mc_search__run_regex (mc_search, user_data, start_search, end_search, found_len);
}

/* --------------------------------------------------------------------------------------------- */
GString *
mc_search_glob_prepare_replace_str (mc_search_t * mc_search, GString * replace_str)
{
    return mc_search_regex_prepare_replace_str (mc_search, replace_str);
}

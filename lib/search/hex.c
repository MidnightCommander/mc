/*
   Search text engine.
   HEX-style pattern matching

   Copyright (C) 2009-2014
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009.

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

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/search.h"
#include "lib/strescape.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static GString *
mc_search__hex_translate_to_regex (const GString * astr)
{
    GString *buff;
    gchar *tmp_str, *tmp_str2;
    gsize tmp_str_len;
    gsize loop = 0;

    buff = g_string_sized_new (64);
    tmp_str = g_strndup (astr->str, astr->len);
    tmp_str2 = tmp_str;

    /* remove 0x prefices */
    while (TRUE)
    {
        tmp_str2 = strstr (tmp_str2, "0x");
        if (tmp_str2 == NULL)
            break;

        *tmp_str2++ = ' ';
        *tmp_str2++ = ' ';
    }

    g_strchug (tmp_str);        /* trim leadind whitespaces */
    tmp_str_len = strlen (tmp_str);

    while (loop < tmp_str_len)
    {
        int val, ptr;

        /* cppcheck-suppress invalidscanf */
        if (sscanf (tmp_str + loop, "%x%n", &val, &ptr))
        {
            if (val < -128 || val > 255)
                loop++;
            else
            {
                g_string_append_printf (buff, "\\x%02X", (unsigned char) val);
                loop += ptr;
            }
        }
        else if (*(tmp_str + loop) == '"')
        {
            gsize loop2 = 0;

            loop++;
            while (loop + loop2 < tmp_str_len)
            {
                if (*(tmp_str + loop + loop2) == '"' &&
                    !strutils_is_char_escaped (tmp_str, tmp_str + loop + loop2))
                    break;
                loop2++;
            }

            g_string_append_len (buff, tmp_str + loop, loop2 - 1);
            loop += loop2;
        }
        else
            loop++;
    }

    g_free (tmp_str);

    return buff;
}

/*** public functions ****************************************************************************/

void
mc_search__cond_struct_new_init_hex (const char *charset, mc_search_t * lc_mc_search,
                                     mc_search_cond_t * mc_search_cond)
{
    GString *tmp;

    g_string_ascii_down (mc_search_cond->str);
    tmp = mc_search__hex_translate_to_regex (mc_search_cond->str);
    g_string_free (mc_search_cond->str, TRUE);
    mc_search_cond->str = tmp;

    mc_search__cond_struct_new_init_regex (charset, lc_mc_search, mc_search_cond);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_search__run_hex (mc_search_t * lc_mc_search, const void *user_data,
                    gsize start_search, gsize end_search, gsize * found_len)
{
    return mc_search__run_regex (lc_mc_search, user_data, start_search, end_search, found_len);
}

/* --------------------------------------------------------------------------------------------- */

GString *
mc_search_hex_prepare_replace_str (mc_search_t * lc_mc_search, GString * replace_str)
{
    (void) lc_mc_search;
    return g_string_new_len (replace_str->str, replace_str->len);
}

/* --------------------------------------------------------------------------------------------- */

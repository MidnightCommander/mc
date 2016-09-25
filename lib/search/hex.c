/*
   Search text engine.
   HEX-style pattern matching

   Copyright (C) 2009-2016
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

typedef enum
{
    MC_SEARCH_HEX_E_OK,
    MC_SEARCH_HEX_E_NUM_OUT_OF_RANGE
} mc_search_hex_parse_error_t;

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static GString *
mc_search__hex_translate_to_regex (const GString * astr, mc_search_hex_parse_error_t * error_ptr,
                                   int *error_pos_ptr)
{
    GString *buff;
    gchar *tmp_str, *tmp_str2;
    gsize tmp_str_len;
    gsize loop = 0;
    mc_search_hex_parse_error_t error = MC_SEARCH_HEX_E_OK;

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

    while (loop < tmp_str_len && error == MC_SEARCH_HEX_E_OK)
    {
        unsigned int val;
        int ptr;

        /* cppcheck-suppress invalidscanf */
        if (sscanf (tmp_str + loop, "%x%n", &val, &ptr))
        {
            if (val > 255)
                error = MC_SEARCH_HEX_E_NUM_OUT_OF_RANGE;
            else
            {
                g_string_append_printf (buff, "\\x%02X", val);
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

    if (error != MC_SEARCH_HEX_E_OK)
    {
        g_string_free (buff, TRUE);
        if (error_ptr != NULL)
            *error_ptr = error;
        if (error_pos_ptr != NULL)
            *error_pos_ptr = loop;
        return NULL;
    }

    return buff;
}

/*** public functions ****************************************************************************/

void
mc_search__cond_struct_new_init_hex (const char *charset, mc_search_t * lc_mc_search,
                                     mc_search_cond_t * mc_search_cond)
{
    GString *tmp;
    mc_search_hex_parse_error_t error = MC_SEARCH_HEX_E_OK;
    int error_pos = 0;

    g_string_ascii_down (mc_search_cond->str);
    tmp = mc_search__hex_translate_to_regex (mc_search_cond->str, &error, &error_pos);
    if (tmp != NULL)
    {
        g_string_free (mc_search_cond->str, TRUE);
        mc_search_cond->str = tmp;
        mc_search__cond_struct_new_init_regex (charset, lc_mc_search, mc_search_cond);
    }
    else
    {
        const char *desc;

        switch (error)
        {
        case MC_SEARCH_HEX_E_NUM_OUT_OF_RANGE:
            desc =
                _
                ("Number out of range (should be in byte range, 0 <= n <= 0xFF, expressed in hex)");
            break;
        default:
            desc = "";
        }

        lc_mc_search->error = MC_SEARCH_E_INPUT;
        lc_mc_search->error_str =
            g_strdup_printf (_("Hex pattern error at position %d:\n%s."), error_pos + 1, desc);
    }
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

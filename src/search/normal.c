/*
   Search text engine.
   Plain search

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

static mc_search__found_cond_t
mc_search__normal_found_cond (mc_search_t * mc_search, int current_chr, gsize search_pos)
{
    gsize loop1;
    mc_search_cond_t *mc_search_cond;

    for (loop1 = 0; loop1 < mc_search->conditions->len; loop1++) {
        mc_search_cond = (mc_search_cond_t *) g_ptr_array_index (mc_search->conditions, loop1);

        if (search_pos > mc_search_cond->len - 1)
            continue;

        if (mc_search->is_case_sentitive) {
            if ((char) current_chr == mc_search_cond->str->str[search_pos])
                return (search_pos ==
                        mc_search_cond->len - 1) ? COND__FOUND_CHAR_LAST : COND__FOUND_CHAR;
        } else {
            GString *upp, *low;
            upp = (mc_search_cond->upper) ? mc_search_cond->upper : mc_search_cond->str;
            low = (mc_search_cond->lower) ? mc_search_cond->lower : mc_search_cond->str;

            if (((char) current_chr == upp->str[search_pos])
                || ((char) current_chr == low->str[search_pos]))
                return (search_pos ==
                        mc_search_cond->len - 1) ? COND__FOUND_CHAR_LAST : COND__FOUND_CHAR;
        }
    }
    return COND__NOT_ALL_FOUND;
}

/*** public functions ****************************************************************************/

void
mc_search__cond_struct_new_init_normal (const char *charset, mc_search_t * mc_search,
                                        mc_search_cond_t * mc_search_cond)
{
    if (!mc_search->is_case_sentitive) {
        mc_search_cond->upper =
            mc_search__toupper_case_str (charset, mc_search_cond->str->str, mc_search_cond->len);
        mc_search_cond->lower =
            mc_search__tolower_case_str (charset, mc_search_cond->str->str, mc_search_cond->len);
    }

}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_search__run_normal (mc_search_t * mc_search, const void *user_data,
                       gsize start_search, gsize end_search, gsize * found_len)
{
    gsize current_pos, search_pos;
    int current_chr = 0;
    gboolean found;

    current_pos = start_search;
    while (1) {
        search_pos = 0;
        found = TRUE;

        while (1) {
            if (current_pos + search_pos > end_search)
                break;

            current_chr = mc_search__get_char (mc_search, user_data, current_pos + search_pos);
            if (current_chr == -1)
                break;
            switch (mc_search__normal_found_cond (mc_search, current_chr, search_pos)) {

            case COND__NOT_ALL_FOUND:
                found = FALSE;
                break;

            case COND__FOUND_CHAR_LAST:
                mc_search->normal_offset = current_pos;
                *found_len = search_pos + 1;
                return TRUE;
                break;

            default:
                break;
            }
            if (!found)
                break;

            search_pos++;
        }
        if (current_chr == -1)
            break;

        current_pos++;
        if (current_pos == end_search + 1)
            break;
    }
    mc_search->error = MC_SEARCH_E_NOTFOUND;
    mc_search->error_str = g_strdup (_(STR_E_NOTFOUND));
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

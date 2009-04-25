/*
   Search text engine.
   Regex search

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

static gboolean
mc_search__regex_str_append_if_special (GString * copy_to, GString * regex_str, gsize * offset)
{
    char *tmp_regex_str;
    gsize spec_chr_len;
    char **spec_chr;
    char *special_chars[] = {
        "\\s", "\\S",
        "\\d", "\\D",
        "\\B", "\\B",
        "\\w", "\\W",
        "\\t", "\\n",
        "\\r", "\\f",
        "\\a", "\\e",
        "\\x", "\\X",
        "\\c", "\\C",
        "\\l", "\\L",
        "\\u", "\\U",
        "\\E", "\\Q",
        NULL
    };
    spec_chr = special_chars;

    tmp_regex_str = &(regex_str->str[*offset]);

    while (*spec_chr) {
        spec_chr_len = strlen (*spec_chr);
        if (!strncmp (tmp_regex_str, *spec_chr, spec_chr_len)) {
            if (!mc_search__regex_is_char_escaped (regex_str->str, tmp_regex_str - 1)) {
                g_string_append_len (copy_to, *spec_chr, spec_chr_len);
                *offset += spec_chr_len;
                return TRUE;
            }
        }
        spec_chr++;
    }
    return FALSE;

}

/* --------------------------------------------------------------------------------------------- */
static void
mc_search__cond_struct_new_regex_hex_add (const char *charset, GString * str_to,
                                          const char *one_char, gsize str_len)
{
    GString *upp, *low;
    gchar *tmp_str;
    gsize loop;

    upp = mc_search__toupper_case_str (charset, one_char, str_len);
    low = mc_search__tolower_case_str (charset, one_char, str_len);

    for (loop = 0; loop < upp->len; loop++) {

        if (loop < low->len) {
            if (upp->str[loop] == low->str[loop])
                tmp_str = g_strdup_printf ("\\x%02X", (unsigned char) upp->str[loop]);
            else
                tmp_str = g_strdup_printf ("[\\x%02X\\x%02X]", (unsigned char)upp->str[loop], (unsigned char)low->str[loop]);
        } else {
            tmp_str = g_strdup_printf ("\\x%02X", (unsigned char) upp->str[loop]);
        }
        g_string_append (str_to, tmp_str);
        g_free (tmp_str);
    }
    g_string_free (upp, TRUE);
    g_string_free (low, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_search__cond_struct_new_regex_accum_append (const char *charset, GString * str_to,
                                               GString * str_from)
{
    GString *recoded_part;
    gchar *one_char;
    gsize loop;
    gboolean just_letters;

    loop = 0;
    recoded_part = g_string_new ("");

    while (loop < str_from->len) {
        one_char =
            mc_search__get_one_symbol (charset, &(str_from->str[loop]),
                                       (str_from->len - loop > 6) ? 6 : str_from->len - loop,
                                       &just_letters);
        if (!strlen (one_char)) {
            loop++;
            continue;
        }
        if (just_letters) {
            mc_search__cond_struct_new_regex_hex_add (charset, recoded_part, one_char,
                                                      strlen (one_char));
        } else {
            g_string_append (recoded_part, one_char);
        }
        loop += strlen (one_char);
        if (!strlen (one_char))
            loop++;
        g_free (one_char);
    }

    g_string_append (str_to, recoded_part->str);
    g_string_free (recoded_part, TRUE);
    g_string_set_size (str_from, 0);
}

/* --------------------------------------------------------------------------------------------- */

static GString *
mc_search__cond_struct_new_regex_ci_str (const char *charset, const char *str, gsize str_len)
{
    GString *accumulator, *spec_char, *ret_str;
    gsize loop;
    GString *tmp;
    tmp = g_string_new_len (str, str_len);


    ret_str = g_string_new ("");
    accumulator = g_string_new ("");
    spec_char = g_string_new ("");
    loop = 0;

    while (loop <= str_len) {
        if (mc_search__regex_str_append_if_special (spec_char, tmp, &loop)) {
            mc_search__cond_struct_new_regex_accum_append (charset, ret_str, accumulator);
            g_string_append_len (ret_str, spec_char->str, spec_char->len);
            g_string_set_size (spec_char, 0);
            continue;
        }

        if (tmp->str[loop] == '['
            && !mc_search__regex_is_char_escaped (tmp->str, &(tmp->str[loop]) - 1)) {
            mc_search__cond_struct_new_regex_accum_append (charset, ret_str, accumulator);

            while (!
                   (tmp->str[loop] == ']'
                    && !mc_search__regex_is_char_escaped (tmp->str, &(tmp->str[loop]) - 1))) {
                g_string_append_c (ret_str, tmp->str[loop]);
                loop++;

            }
            g_string_append_c (ret_str, tmp->str[loop]);
            loop++;
            continue;
        }
        /*
           TODO: handle [ and ]
         */
        g_string_append_c (accumulator, tmp->str[loop]);
        loop++;
    }
    mc_search__cond_struct_new_regex_accum_append (charset, ret_str, accumulator);

    g_string_free (accumulator, TRUE);
    g_string_free (spec_char, TRUE);
    g_string_free (tmp, TRUE);
    return ret_str;
}

/* --------------------------------------------------------------------------------------------- */

static mc_search__found_cond_t
mc_search__regex_found_cond_one (mc_search_t * mc_search, GRegex * regex, GString * search_str)
{
#if GLIB_CHECK_VERSION (2, 14, 0)
    GMatchInfo *match_info;
    GError *error = NULL;

    if (!g_regex_match_full (regex, search_str->str, -1, 0, 0, &match_info, &error)) {
        g_match_info_free (match_info);
        if (error) {
            mc_search->error = MC_SEARCH_E_REGEX;
            mc_search->error_str = g_strdup (error->message);
            g_error_free (error);
            return COND__FOUND_ERROR;
        }
        return COND__NOT_FOUND;
    }
    mc_search->regex_match_info = match_info;
    return COND__FOUND_OK;
#endif

}

/* --------------------------------------------------------------------------------------------- */

static mc_search__found_cond_t
mc_search__regex_found_cond (mc_search_t * mc_search, GString * search_str)
{
    gsize loop1;
    mc_search_cond_t *mc_search_cond;
    mc_search__found_cond_t ret;

    for (loop1 = 0; loop1 < mc_search->conditions->len; loop1++) {
        mc_search_cond = (mc_search_cond_t *) g_ptr_array_index (mc_search->conditions, loop1);

        if (!mc_search_cond->regex_str)
            continue;

        ret = mc_search__regex_found_cond_one (mc_search, mc_search_cond->regex_str, search_str);

        if (ret != COND__NOT_FOUND)
            return ret;
    }
    return COND__NOT_ALL_FOUND;
}

/*** public functions ****************************************************************************/

void
mc_search__cond_struct_new_init_regex (const char *charset, mc_search_t * mc_search,
                                       mc_search_cond_t * mc_search_cond)
{
#if GLIB_CHECK_VERSION (2, 14, 0)
    GString *tmp;
    GError *error = NULL;

    if (!mc_search->is_case_sentitive) {
        tmp = g_string_new_len (mc_search_cond->str->str, mc_search_cond->str->len);
        g_string_free (mc_search_cond->str, TRUE);
        mc_search_cond->str = mc_search__cond_struct_new_regex_ci_str (charset, tmp->str, tmp->len);
        g_string_free (tmp, TRUE);
    }

    mc_search_cond->regex_str = g_regex_new (mc_search_cond->str->str, G_REGEX_OPTIMIZE|G_REGEX_RAW, 0, &error);

    if (error != NULL) {
        mc_search->error = MC_SEARCH_E_REGEX_COMPILE;
        mc_search->error_str = g_strdup (error->message);
        g_error_free (error);
        return;
    }
#else
    (void) mc_search_cond;
    (void) mc_search;
#endif
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_search__run_regex (mc_search_t * mc_search, const void *user_data,
                      gsize start_search, gsize end_search, gsize * found_len)
{
#if GLIB_CHECK_VERSION (2, 14, 0)
    GString *buffer;
    gsize current_pos, start_buffer;
    int current_chr = 0;
    gint start_pos;
    gint end_pos;

    buffer = g_string_new ("");

    current_pos = start_search;
    while (current_pos <= end_search) {
        g_string_set_size (buffer, 0);
        start_buffer = current_pos;

        while (1) {
            current_chr = mc_search__get_char (mc_search, user_data, current_pos);
            if (current_chr == -1)
                break;

            g_string_append_c (buffer, (char) current_chr);

            current_pos++;

            if (current_chr == 0 || (char) current_chr == '\n')
                break;

            if (current_pos > end_search)
                break;

        }
        if (current_chr == -1)
            break;

        switch (mc_search__regex_found_cond (mc_search, buffer)) {
        case COND__FOUND_OK:
            g_match_info_fetch_pos (mc_search->regex_match_info, 0, &start_pos, &end_pos);
            *found_len = end_pos - start_pos;
            mc_search->normal_offset = start_buffer + start_pos;
            g_string_free (buffer, TRUE);
            return TRUE;
            break;
        case COND__NOT_ALL_FOUND:
            break;
        default:
            g_string_free (buffer, TRUE);
            return FALSE;
            break;
        }

    }
    g_string_free (buffer, TRUE);
#endif
    mc_search->error = MC_SEARCH_E_NOTFOUND;
    mc_search->error_str = g_strdup (_(STR_E_NOTFOUND));
    return FALSE;
}

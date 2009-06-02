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
                if (!strncmp ("\\x", *spec_chr, spec_chr_len)) {
                    if (*(tmp_regex_str + spec_chr_len) == '{') {
                        while ((spec_chr_len < regex_str->len - *offset)
                               && *(tmp_regex_str + spec_chr_len) != '}')
                            spec_chr_len++;
                        if (*(tmp_regex_str + spec_chr_len) == '}')
                            spec_chr_len++;
                    } else
                        spec_chr_len += 2;
                }
                g_string_append_len (copy_to, tmp_regex_str, spec_chr_len);
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
                tmp_str =
                    g_strdup_printf ("[\\x%02X\\x%02X]", (unsigned char) upp->str[loop],
                                     (unsigned char) low->str[loop]);
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

            while (loop < str_len && !(tmp->str[loop] == ']'
                                       && !mc_search__regex_is_char_escaped (tmp->str,
                                                                             &(tmp->str[loop]) -
                                                                             1))) {
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
mc_search__regex_found_cond_one (mc_search_t * mc_search, mc_search_regex_t * regex,
                                 GString * search_str)
{
#if GLIB_CHECK_VERSION (2, 14, 0)
    GError *error = NULL;

    if (!g_regex_match_full
        (regex, search_str->str, -1, 0, 0, &mc_search->regex_match_info, &error)) {
        g_match_info_free (mc_search->regex_match_info);
        mc_search->regex_match_info = NULL;
        if (error) {
            mc_search->error = MC_SEARCH_E_REGEX;
            mc_search->error_str = str_conv_gerror_message (error, _(" Regular expression error "));
            g_error_free (error);
            return COND__FOUND_ERROR;
        }
        return COND__NOT_FOUND;
    }
    mc_search->num_rezults = g_match_info_get_match_count (mc_search->regex_match_info);
#else
#if HAVE_LIBPCRE
    mc_search->num_rezults = pcre_exec (regex, mc_search->regex_match_info,
                                        search_str->str, search_str->len, 0, 0, mc_search->iovector,
                                        MC_SEARCH__PCRE_MAX_MATCHES);
    if (mc_search->num_rezults < 0) {
        return COND__NOT_FOUND;
    }
#else /* HAVE_LIBPCRE */

    if (regexec (regex, search_str->str, MC_SEARCH__NUM_REPL_ARGS, mc_search->regex_match_info, 0))
        return COND__NOT_FOUND;

    for (mc_search->num_rezults = 0; mc_search->num_rezults < MC_SEARCH__NUM_REPL_ARGS;
         mc_search->num_rezults++) {
        if (mc_search->regex_match_info[mc_search->num_rezults].rm_eo == 0)
            break;

    }
#endif /* HAVE_LIBPCRE */
#endif /* GLIB_CHECK_VERSION (2, 14, 0) */
    return COND__FOUND_OK;

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

        if (!mc_search_cond->regex_handle)
            continue;

        ret = mc_search__regex_found_cond_one (mc_search, mc_search_cond->regex_handle, search_str);

        if (ret != COND__NOT_FOUND)
            return ret;
    }
    return COND__NOT_ALL_FOUND;
}

/* --------------------------------------------------------------------------------------------- */

static int
mc_search_regex__get_num_replace_tokens (const gchar * str, gsize len)
{
    int count_tokens = 0;
    gsize loop;
    for (loop = 0; loop < len - 1; loop++) {
        if (str[loop] == '\\' && (str[loop + 1] & (char) 0xf0) == 0x30 /* 0-9 */ )
        {
            if (mc_search__regex_is_char_escaped (str, &str[loop - 1]))
                continue;

            count_tokens++;
            continue;
        }
        if (str[loop] == '$' && str[loop + 1] == '{' )
        {
            gsize tmp_len;
            if (mc_search__regex_is_char_escaped (str, &str[loop - 1]))
                continue;

            for (tmp_len = 0; loop + tmp_len + 2 < len && (str[loop + 2 + tmp_len] & (char) 0xf0) == 0x30; tmp_len++);
            if (str[loop + 2 + tmp_len] == '}')
                count_tokens++;
        }
    }
    return count_tokens;
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_search_regex__append_found_token_by_num (const mc_search_t * mc_search, const gchar * fnd_str,
                                            GString * str, gsize index)
{
    int fnd_start, fnd_end, fnd_len;
    gchar *start_str;

#if GLIB_CHECK_VERSION (2, 14, 0)
    g_match_info_fetch_pos (mc_search->regex_match_info, index, &fnd_start, &fnd_end);
#else /* GLIB_CHECK_VERSION (2, 14, 0) */
#if HAVE_LIBPCRE
    fnd_start = mc_search->iovector[index * 2 + 0];
    fnd_end = mc_search->iovector[index * 2 + 1];
#else /* HAVE_LIBPCRE */
    fnd_start = mc_search->regex_match_info[index].rm_so;
    fnd_end = mc_search->regex_match_info[index].rm_eo;
#endif /* HAVE_LIBPCRE */
#endif /* GLIB_CHECK_VERSION (2, 14, 0) */

    fnd_len = fnd_end - fnd_start;
    start_str = fnd_str + fnd_start;

    if (fnd_len == 0)
        return;

    g_string_append_len (str, start_str, fnd_len);

}


/*** public functions ****************************************************************************/

void
mc_search__cond_struct_new_init_regex (const char *charset, mc_search_t * mc_search,
                                       mc_search_cond_t * mc_search_cond)
{
    GString *tmp = NULL;
#if GLIB_CHECK_VERSION (2, 14, 0)
    GError *error = NULL;
#else
    const char *error;
    int erroffset;
#endif

    if (!mc_search->is_case_sentitive) {
        tmp = g_string_new_len (mc_search_cond->str->str, mc_search_cond->str->len);
        g_string_free (mc_search_cond->str, TRUE);
        mc_search_cond->str = mc_search__cond_struct_new_regex_ci_str (charset, tmp->str, tmp->len);
        g_string_free (tmp, TRUE);
    }
#if GLIB_CHECK_VERSION (2, 14, 0)
    mc_search_cond->regex_handle =
        g_regex_new (mc_search_cond->str->str, G_REGEX_OPTIMIZE | G_REGEX_RAW, 0, &error);

    if (error != NULL) {
        mc_search->error = MC_SEARCH_E_REGEX_COMPILE;
        mc_search->error_str = str_conv_gerror_message (error, _(" Regular expression error "));
        g_error_free (error);
        return;
    }
#else /* GLIB_CHECK_VERSION (2, 14, 0) */
#if HAVE_LIBPCRE
    mc_search_cond->regex_handle =
        pcre_compile (mc_search_cond->str->str, PCRE_EXTRA, &error, &erroffset, NULL);
    if (mc_search_cond->regex_handle == NULL) {
        mc_search->error = MC_SEARCH_E_REGEX_COMPILE;
        mc_search->error_str = g_strdup (error);
        return;
    }
    mc_search->regex_match_info = pcre_study (mc_search_cond->regex_handle, 0, &error);
    if (mc_search->regex_match_info == NULL) {
        if (error) {
            mc_search->error = MC_SEARCH_E_REGEX_COMPILE;
            mc_search->error_str = g_strdup (error);
            free (mc_search_cond->regex_handle);
            mc_search_cond->regex_handle = NULL;
            return;
        }
    }
#else /* HAVE_LIBPCRE */
    mc_search_cond->regex_handle = g_malloc0 (sizeof (regex_t));
    erroffset = regcomp (mc_search_cond->regex_handle, mc_search_cond->str->str, REG_EXTENDED);
    if (erroffset) {
        size_t err_len = regerror (erroffset, mc_search_cond->regex_handle, NULL, 0);
        error = g_malloc (err_len + 1);
        regerror (erroffset, mc_search_cond->regex_handle, error, err_len);
        mc_search->error = MC_SEARCH_E_REGEX_COMPILE;
        mc_search->error_str = error;
        regfree (mc_search_cond->regex_handle);
        mc_search_cond->regex_handle = NULL;
        return;
    }
    mc_search->regex_match_info = g_new0 (mc_search_matchinfo_t, MC_SEARCH__NUM_REPL_ARGS);
#endif /* HAVE_LIBPCRE */
#endif /* GLIB_CHECK_VERSION (2, 14, 0) */
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_search__run_regex (mc_search_t * mc_search, const void *user_data,
                      gsize start_search, gsize end_search, gsize * found_len)
{
    gsize current_pos, start_buffer;
    int current_chr = 0;
    gint start_pos;
    gint end_pos;

    if (mc_search->regex_buffer != NULL)
        g_string_free (mc_search->regex_buffer, TRUE);

    mc_search->regex_buffer = g_string_new ("");

    current_pos = start_search;
    while (current_pos <= end_search) {
        g_string_set_size (mc_search->regex_buffer, 0);
        start_buffer = current_pos;

        while (1) {
            current_chr = mc_search__get_char (mc_search, user_data, current_pos);
            if (current_chr == -1)
                break;

            g_string_append_c (mc_search->regex_buffer, (char) current_chr);

            current_pos++;

            if (current_chr == 0 || (char) current_chr == '\n')
                break;

            if (current_pos > end_search)
                break;

        }
        if (current_chr == -1)
            break;

        switch (mc_search__regex_found_cond (mc_search, mc_search->regex_buffer)) {
        case COND__FOUND_OK:
#if GLIB_CHECK_VERSION (2, 14, 0)
            g_match_info_fetch_pos (mc_search->regex_match_info, 0, &start_pos, &end_pos);
#else /* GLIB_CHECK_VERSION (2, 14, 0) */
#if HAVE_LIBPCRE
            start_pos = mc_search->iovector[0];
            end_pos = mc_search->iovector[1];
#else /* HAVE_LIBPCRE */
            start_pos = mc_search->regex_match_info[0].rm_so;
            end_pos = mc_search->regex_match_info[0].rm_eo;
#endif /* HAVE_LIBPCRE */
#endif /* GLIB_CHECK_VERSION (2, 14, 0) */
            if (found_len)
                *found_len = end_pos - start_pos;
            mc_search->normal_offset = start_buffer + start_pos;
            return TRUE;
            break;
        case COND__NOT_ALL_FOUND:
            break;
        default:
            g_string_free (mc_search->regex_buffer, TRUE);
            mc_search->regex_buffer = NULL;
            return FALSE;
            break;
        }
    }
    g_string_free (mc_search->regex_buffer, TRUE);
    mc_search->regex_buffer = NULL;
    mc_search->error = MC_SEARCH_E_NOTFOUND;
    mc_search->error_str = g_strdup (_(STR_E_NOTFOUND));
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
GString *
mc_search_regex_prepare_replace_str (mc_search_t * mc_search, GString * replace_str)
{
    GString *ret;
    gchar *tmp_str;

    int num_replace_tokens;
    gsize loop;
    gsize index, len;

    gchar *prev_str;

    num_replace_tokens =
        mc_search_regex__get_num_replace_tokens (replace_str->str, replace_str->len);

    if (mc_search->num_rezults < 0)
        return g_string_new_len (replace_str->str, replace_str->len);

    if (num_replace_tokens > mc_search->num_rezults - 1
        || num_replace_tokens > MC_SEARCH__NUM_REPLACE_ARGS ) {
        mc_search->error = MC_SEARCH_E_REGEX_REPLACE;
        mc_search->error_str = g_strdup (STR_E_RPL_NOT_EQ_TO_FOUND);
        return NULL;
    }

    ret = g_string_new ("");
    prev_str = replace_str->str;
    for (loop = 0; loop < replace_str->len - 1; loop++) {
        tmp_str = NULL;

        if (replace_str->str[loop] == '\\'
            && (replace_str->str[loop + 1] & (char) 0xf0) == 0x30 /* 0-9 */ ) {
            /* \1 relace tokens */
            if (! mc_search__regex_is_char_escaped (replace_str->str, &replace_str->str[loop - 1]))
                tmp_str = g_strndup (&(replace_str->str[loop + 1]), 1);
            len=2;

        } else if (replace_str->str[loop] == '$' && replace_str->str[loop + 1] == '{' && (replace_str->str[loop + 2] & (char) 0xf0) == 0x30) {
            /* ${1} replace token */
            for (len = 0; loop+len+2 < replace_str->len && (replace_str->str[loop + 2 + len] & (char) 0xf0) == 0x30; len++);
            if (replace_str->str[loop + 2 + len] == '}')
                tmp_str = g_strndup (&(replace_str->str[loop + 2]), len);
            len+=3;
        }

        if (tmp_str == NULL)
            continue;

            index = (gsize) atoi (tmp_str);
            g_free (tmp_str);
            tmp_str = NULL;

            if (index > mc_search->num_rezults) {
                g_string_free (ret, TRUE);
                mc_search->error = MC_SEARCH_E_REGEX_REPLACE;
                mc_search->error_str = g_strdup_printf (STR_E_RPL_INVALID_TOKEN, index);
                return NULL;
            }
            if (loop)
                g_string_append_len (ret, prev_str, replace_str->str - prev_str + loop);

            mc_search_regex__append_found_token_by_num (mc_search,
                                                        mc_search->regex_buffer->str, ret, index);
            prev_str = replace_str->str + loop + len;
    }
    g_string_append_len (ret, prev_str, replace_str->str - prev_str + replace_str->len);

    return ret;
    //#endif /* GLIB_CHECK_VERSION (2, 14, 0) */
}

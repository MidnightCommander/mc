/*
   Search text engine.

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
#include "../src/search.h"
#include "../src/strutil.h"
#include "../src/charsets.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define STR_E_NOTFOUND		" Search string not found "
#define STR_E_UNKNOWN_TYPE	" Unknown search type "

/*** file scope type declarations ****************************************************************/

typedef struct mc_search_cond_struct {
    GString *str;
    GString *upper;
    GString *lower;
#if GLIB_CHECK_VERSION (2, 14, 0)
    GRegex *regex_str;
#endif
    gsize len;
    gchar *charset;
} mc_search_cond_t;

typedef enum {
    COND__NOT_FOUND,
    COND__NOT_ALL_FOUND,
    COND__FOUND_CHAR,
    COND__FOUND_CHAR_LAST,
    COND__FOUND_OK,
    COND__FOUND_ERROR
} mc_search__found_cond_t;

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static gchar *
mc_search__recode_str (const char *str, gsize str_len,
                       const char *charset_from, const char *charset_to, gsize * bytes_written)
{
    gchar *ret;
    gsize bytes_read;
    GIConv conv;


    if (!strcmp (charset_to, charset_from)) {
        *bytes_written = str_len;
        return g_strndup (str, str_len);
    }

    conv = g_iconv_open (charset_to, charset_from);
    if (conv == (GIConv) - 1)
        return NULL;

    ret = g_convert_with_iconv (str, str_len, conv, &bytes_read, bytes_written, NULL);
    g_iconv_close (conv);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static char *
mc_search__get_one_symbol (const char *charset, const char *str, gsize str_len,
                                 gboolean * just_letters)
{

    gchar *converted_str, *next_char, *converted_str2;

    gsize converted_str_len;
    gsize tmp_len;

    converted_str = mc_search__recode_str (str, str_len, charset, cp_display, &converted_str_len);


    next_char = (char *) str_cget_next_char (converted_str);

    tmp_len = next_char - converted_str;

    converted_str[tmp_len] = '\0';

    converted_str2 =
        mc_search__recode_str (converted_str, tmp_len, cp_display, charset, &converted_str_len);

    if (str_isalnum (converted_str) && !str_isdigit (converted_str))
        *just_letters = TRUE;
    else
        *just_letters = FALSE;


    g_free (converted_str);
    return converted_str2;
}

/* --------------------------------------------------------------------------------------------- */

static GString *
mc_search__tolower_case_str (const char *charset, const char *str, gsize str_len)
{
    gchar *converted_str, *tmp_str1, *tmp_str2, *tmp_str3;
    gsize converted_str_len;
    gsize tmp_len;

    tmp_str2 = converted_str =
        mc_search__recode_str (str, str_len, charset, cp_display, &converted_str_len);
    if (converted_str == NULL)
        return NULL;

    tmp_len = converted_str_len + 1;

    tmp_str3 = tmp_str1 = g_strdup (converted_str);

    while (str_tolower (tmp_str1, &tmp_str2, &tmp_len))
        tmp_str1 += str_length_char (tmp_str1);

    g_free (tmp_str3);
    tmp_str2 =
        mc_search__recode_str (converted_str, converted_str_len, cp_display, charset, &tmp_len);
    g_free (converted_str);
    if (tmp_str2 == NULL)
        return NULL;

    return g_string_new_len (tmp_str2, tmp_len);
}

/* --------------------------------------------------------------------------------------------- */

static GString *
mc_search__toupper_case_str (const char *charset, const char *str, gsize str_len)
{
    gchar *converted_str, *tmp_str1, *tmp_str2, *tmp_str3;
    gsize converted_str_len;
    gsize tmp_len;

    tmp_str2 = converted_str =
        mc_search__recode_str (str, str_len, charset, cp_display, &converted_str_len);
    if (converted_str == NULL)
        return NULL;

    tmp_len = converted_str_len + 1;

    tmp_str3 = tmp_str1 = g_strdup (converted_str);

    while (str_toupper (tmp_str1, &tmp_str2, &tmp_len))
        tmp_str1 += str_length_char (tmp_str1);

    g_free (tmp_str3);

    tmp_str2 =
        mc_search__recode_str (converted_str, converted_str_len, cp_display, charset, &tmp_len);
    g_free (converted_str);
    if (tmp_str2 == NULL)
        return NULL;

    return g_string_new_len (tmp_str2, tmp_len);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_search__regex_is_char_escaped (char *start, char *current)
{
    int num_esc = 0;
    while (*current == '\\' && current >= start) {
        num_esc++;
        current--;
    }
    return (gboolean) num_esc % 2;
}

/* --------------------------------------------------------------------------------------------- */

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
        "\\c[", "\\C",
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

mc_search__cond_struct_new_regex_accum_append (const char *charset, GString * str_to,
                                               GString * str_from)
{
    GString *recoded_part, *tmp;
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
            g_string_append_c (recoded_part, '[');

            tmp = mc_search__toupper_case_str (charset, one_char, strlen (one_char));
            g_string_append (recoded_part, tmp->str);
            g_string_free (tmp, TRUE);

            tmp = mc_search__tolower_case_str (charset, one_char, strlen (one_char));
            g_string_append (recoded_part, tmp->str);
            g_string_free (tmp, TRUE);

            g_string_append_c (recoded_part, ']');

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
    mc_log ("!!! %s\n", ret_str->str);
    return ret_str;
}

/* --------------------------------------------------------------------------------------------- */

static void

mc_search__cond_struct_new_init_regex (mc_search_t * mc_search, mc_search_cond_t * mc_search_cond)
{
#if GLIB_CHECK_VERSION (2, 14, 0)
    GError *error = NULL;

    mc_search_cond->regex_str = g_regex_new (mc_search_cond->str->str, G_REGEX_OPTIMIZE, 0, &error);
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

static mc_search_cond_t *
mc_search__cond_struct_new (mc_search_t * mc_search, const char *str,
                            gsize str_len, const char *charset)
{
    mc_search_cond_t *mc_search_cond;
    mc_search_cond = g_malloc0 (sizeof (mc_search_cond_t));
    switch (mc_search->search_type) {
    case MC_SEARCH_T_GLOB:
    case MC_SEARCH_T_NORMAL:
        mc_search_cond->str = g_string_new_len (str, str_len);
        mc_search_cond->len = str_len;
        mc_search_cond->charset = g_strdup (charset);
        if (!mc_search->is_case_sentitive) {
            mc_search_cond->upper = mc_search__toupper_case_str (charset, str, str_len);
            mc_search_cond->lower = mc_search__tolower_case_str (charset, str, str_len);
        }
        break;
    case MC_SEARCH_T_REGEX:

        mc_search_cond->charset = g_strdup (charset);

        if (mc_search->is_case_sentitive) {
            mc_search_cond->str = g_string_new_len (str, str_len);
            mc_search_cond->len = str_len;
        } else {
            mc_search_cond->str = mc_search__cond_struct_new_regex_ci_str (charset, str, str_len);
        }
        mc_search__cond_struct_new_init_regex (mc_search, mc_search_cond);
        break;
    case MC_SEARCH_T_SCANF:
    case MC_SEARCH_T_HEX:
    default:
        break;
    }
    return mc_search_cond;
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
mc_search__conditions_new (mc_search_t * mc_search)
{
    GPtrArray *ret;
    ret = g_ptr_array_new ();

    if (mc_search->is_all_charsets) {
        gsize loop1, recoded_str_len;

        gchar *buffer;

        for (loop1 = 0; loop1 < (gsize) n_codepages; loop1++) {
            if (g_ascii_strcasecmp (codepages[loop1].id, cp_source)) {
                g_ptr_array_add (ret,
                                 mc_search__cond_struct_new (mc_search, mc_search->original,
                                                             mc_search->original_len, cp_source));
                continue;
            }

            buffer =
                mc_search__recode_str (mc_search->original, mc_search->original_len, cp_source,
                                       codepages[loop1].id, &recoded_str_len);
            if (buffer == NULL)
                continue;

            g_ptr_array_add (ret,
                             mc_search__cond_struct_new (mc_search, buffer,
                                                         recoded_str_len, codepages[loop1].id));
            g_free (buffer);
        }
    } else {
        g_ptr_array_add (ret,
                         (gpointer) mc_search__cond_struct_new (mc_search, mc_search->original,
                                                                mc_search->original_len,
                                                                cp_source));
    }
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_search__cond_struct_free (mc_search_cond_t * mc_search_cond)
{
    if (mc_search_cond->upper)
        g_string_free (mc_search_cond->upper, TRUE);

    if (mc_search_cond->lower)
        g_string_free (mc_search_cond->lower, TRUE);

    g_string_free (mc_search_cond->str, TRUE);
    g_free (mc_search_cond->charset);

#if GLIB_CHECK_VERSION (2, 14, 0)
    if (mc_search_cond->regex_str)
        g_regex_unref (mc_search_cond->regex_str);

#endif

    g_free (mc_search_cond);
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_search__conditions_free (GPtrArray * array)
{
    gsize loop1;
    mc_search_cond_t *mc_search;

    for (loop1 = 0; loop1 < array->len; loop1++) {
        mc_search = (mc_search_cond_t *) g_ptr_array_index (array, loop1);
        mc_search__cond_struct_free (mc_search);
    }
    g_ptr_array_free (array, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static int
mc_search__get_char (mc_search_t * mc_search, const void *user_data, gsize current_pos)
{
    char *data;
    if (mc_search->search_fn)
        return (mc_search->search_fn) (user_data, current_pos);

    data = (char *) user_data;
    return (int) data[current_pos];
}

/* --------------------------------------------------------------------------------------------- */

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

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_search__run_normal (mc_search_t * mc_search, const void *user_data,
                       gsize start_search, gsize end_search, gsize * found_len)
{
    gsize current_pos, search_pos;
    int current_chr = 0;
    gboolean found;

    if (mc_search->is_backward) {
        current_pos = end_search;
    } else {
        current_pos = start_search;
    }
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

        if (mc_search->is_backward) {
            current_pos--;
            if (current_pos == start_search - 1)
                break;
        } else {
            current_pos++;
            if (current_pos == end_search + 1)
                break;
        }
    }
    mc_search->error = MC_SEARCH_E_NOTFOUND;
    mc_search->error_str = g_strdup (_(STR_E_NOTFOUND));
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static mc_search__found_cond_t
mc_search__regex_found_cond_one (mc_search_t * mc_search, GRegex * regex, GString * search_str)
{
#if GLIB_CHECK_VERSION (2, 14, 0)
    GMatchInfo *match_info;
    GError *error = NULL;
//mc_log("%s\n",search_str->str);

    if (!g_regex_match_full (regex, search_str->str, -1, 0, 0, &match_info, &error)) {
//mc_log("1\n");
        g_match_info_free (match_info);
        if (error) {
//mc_log("2\n");
            mc_search->error = MC_SEARCH_E_REGEX;
            mc_search->error_str = g_strdup (error->message);
            g_error_free (error);
            return COND__FOUND_ERROR;
        }
//mc_log("3\n");
        return COND__NOT_FOUND;
    }
//mc_log("4\n");
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

/* --------------------------------------------------------------------------------------------- */

static gboolean
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

/*** public functions ****************************************************************************/

mc_search_t *
mc_search_new (const gchar * original, gsize str_len)
{
    mc_search_t *mc_search;
    if (!original)
        return NULL;

    if ((gssize) str_len == -1) {
        str_len = strlen (original);
        if (str_len == 0)
            return NULL;
    }

    mc_search = g_malloc0 (sizeof (mc_search_t));
    mc_search->original = g_strndup (original, str_len);
    mc_search->original_len = str_len;
    return mc_search;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_search_free (mc_search_t * mc_search)
{
    if (!mc_search)
        return;

    g_free (mc_search->original);

    if (mc_search->error_str)
        g_free (mc_search->error_str);

    if (mc_search->conditions)
        mc_search__conditions_free (mc_search->conditions);

#if GLIB_CHECK_VERSION (2, 14, 0)
    if (mc_search->regex_match_info)
        g_match_info_free (mc_search->regex_match_info);
#endif

    g_free (mc_search);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_search_run (mc_search_t * mc_search, const void *user_data,
               gsize start_search, gsize end_search, gsize * found_len)
{
    gboolean ret = FALSE;

    if (!mc_search)
        return FALSE;
    if (!mc_search_is_type_avail (mc_search->search_type)) {
        mc_search->error = MC_SEARCH_E_INPUT;
        mc_search->error_str = g_strdup (_(STR_E_UNKNOWN_TYPE));
        return FALSE;
    }
#if GLIB_CHECK_VERSION (2, 14, 0)
    if (mc_search->regex_match_info) {
        g_match_info_free (mc_search->regex_match_info);
        mc_search->regex_match_info = NULL;
    }
#endif

    mc_search->error = MC_SEARCH_E_OK;
    if (mc_search->error_str) {
        g_free (mc_search->error_str);
        mc_search->error_str = NULL;
    }

    if (!mc_search->conditions) {
        mc_search->conditions = mc_search__conditions_new (mc_search);

        if (mc_search->error != MC_SEARCH_E_OK)
            return FALSE;
    }

    switch (mc_search->search_type) {
    case MC_SEARCH_T_NORMAL:
        ret = mc_search__run_normal (mc_search, user_data, start_search, end_search, found_len);
        break;
    case MC_SEARCH_T_REGEX:
        ret = mc_search__run_regex (mc_search, user_data, start_search, end_search, found_len);
        break;
    case MC_SEARCH_T_HEX:
    case MC_SEARCH_T_SCANF:
    case MC_SEARCH_T_GLOB:
    default:
        break;
    }
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_search_is_type_avail (mc_search_type_t search_type)
{
    switch (search_type) {
    case MC_SEARCH_T_NORMAL:
        return TRUE;
        break;
    case MC_SEARCH_T_REGEX:
#if ! GLIB_CHECK_VERSION (2, 14, 0)
        return FALSE;
#else
        return TRUE;
#endif
        break;
    case MC_SEARCH_T_HEX:
    case MC_SEARCH_T_SCANF:
    case MC_SEARCH_T_GLOB:
    default:
        break;
    }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

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
    gsize len;
    gchar *charset;
} mc_search_cond_t;

typedef enum {
    COND__NOT_ALL_FOUND,
    COND__FOUND_CHAR,
    COND__FOUND_CHAR_LAST,
} mc_search__found_cond_t;

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static gchar *
mc_search__recode_str (const char *str, guint str_len, const char *charset_from,
                       const char *charset_to, guint * bytes_written)
{
    gchar *ret;
    gsize bytes_read;
    GIConv conv;


    if (!strcmp(charset_to, charset_from)){
	*bytes_written = str_len;
	return g_strndup(str,str_len);
    }

    conv = g_iconv_open (charset_to, charset_from);
    if (conv == (GIConv) - 1)
        return NULL;

    ret = g_convert_with_iconv (str, str_len, conv, &bytes_read, bytes_written, NULL);
    g_iconv_close (conv);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */
static GString *
mc_search__tolower_case_str (const char *charset, const char *str, guint str_len)
{
    gchar *converted_str, *tmp_str1, *tmp_str2, *tmp_str3;
    guint converted_str_len;
    guint tmp_len;

    tmp_str2 = converted_str =
        mc_search__recode_str (str, str_len, charset, cp_display, &converted_str_len);
    if (converted_str == NULL)
        return NULL;

    tmp_len = converted_str_len+1;

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
mc_search__toupper_case_str (const char *charset, const char *str, guint str_len)
{
    gchar *converted_str, *tmp_str1, *tmp_str2, *tmp_str3;
    guint converted_str_len;
    guint tmp_len;

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
static mc_search_cond_t *
mc_search__cond_struct_new (const char *str, guint str_len, const char *charset, int case_sentitive)
{
    mc_search_cond_t *mc_search_cond;
    mc_search_cond = g_malloc0 (sizeof (mc_search_cond_t));
    mc_search_cond->str = g_string_new_len (str, str_len);
    mc_search_cond->len = str_len;
    mc_search_cond->charset = g_strdup (charset);
    if (case_sentitive) {
        mc_search_cond->upper = mc_search__toupper_case_str (charset, str, str_len);
        mc_search_cond->lower = mc_search__tolower_case_str (charset, str, str_len);
    }
char *up, *lo;
up = (mc_search_cond->upper) ? mc_search_cond->upper->str: "(null)";
lo = (mc_search_cond->lower) ? mc_search_cond->lower->str: "(null)";
mc_log("\
mc_search_cond->str   = %s\n\
mc_search_cond->upper = %s\n\
mc_search_cond->lower = %s\n\
\n",mc_search_cond->str->str, up, lo);
    return mc_search_cond;
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
mc_search__conditions_new (const char *str, guint str_len, int all_charsets, int case_sentitive)
{
    GPtrArray *ret;
    ret = g_ptr_array_new ();

    if (all_charsets) {
        guint loop1, recoded_str_len;

        gchar *buffer;

        for (loop1 = 0; loop1 < n_codepages; loop1++) {
            if (g_ascii_strcasecmp (codepages[loop1].id, cp_source)) {
                g_ptr_array_add (ret,
                                 mc_search__cond_struct_new (str, str_len, cp_source,
                                                             case_sentitive));
                continue;
            }

            buffer =
                mc_search__recode_str (str, str_len, cp_source, codepages[loop1].id,
                                       &recoded_str_len);
            if (buffer == NULL)
                continue;

            g_ptr_array_add (ret,
                             mc_search__cond_struct_new (buffer, recoded_str_len,
                                                         codepages[loop1].id, case_sentitive));
            g_free (buffer);
        }
    } else {
        g_ptr_array_add (ret,
                         (gpointer) mc_search__cond_struct_new (str, str_len, cp_source,
                                                                case_sentitive));
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
    g_free (mc_search_cond);
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_search__conditions_free (GPtrArray * array)
{
    guint loop1;
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
    int loop1;
    mc_search_cond_t *mc_search_cond;

    for (loop1 = 0; loop1 < mc_search->conditions->len; loop1++) {
        mc_search_cond = (mc_search_cond_t *) g_ptr_array_index (mc_search->conditions, loop1);

        if (search_pos > mc_search_cond->len)
            continue;

        if (mc_search->is_case_sentitive) {
            if ((char) current_chr == mc_search_cond->str->str[search_pos])
                return (search_pos ==
                        mc_search_cond->len) ? COND__FOUND_CHAR_LAST : COND__FOUND_CHAR;
        } else {
            GString *upp, *low;
            upp = (mc_search_cond->upper) ? mc_search_cond->upper : mc_search_cond->str;
            low = (mc_search_cond->lower) ? mc_search_cond->lower : mc_search_cond->str;

            if (((char) current_chr == upp->str[search_pos])
                || ((char) current_chr == low->str[search_pos]))
                return (search_pos ==
                        mc_search_cond->len) ? COND__FOUND_CHAR_LAST : COND__FOUND_CHAR;
        }
    }
    return COND__NOT_ALL_FOUND;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_search__run_normal (mc_search_t * mc_search, const void *user_data,
                       gsize start_search, gsize end_search, gsize * founded_len)
{
    gsize current_pos, search_pos;
    int current_chr = 0;
    gboolean founded;

    if (mc_search->is_backward) {
        current_pos = end_search;
    } else {
        current_pos = start_search;
    }
    while (1) {
        search_pos = 0;
        founded = TRUE;

        while (1) {
            if (current_pos + search_pos > end_search)
                break;

            current_chr = mc_search__get_char (mc_search, user_data, current_pos + search_pos);
            if (current_chr == -1)
                break;


            switch (mc_search__normal_found_cond (mc_search, current_chr, search_pos)) {

            case COND__NOT_ALL_FOUND:
                founded = FALSE;
                break;

            case COND__FOUND_CHAR_LAST:
                mc_search->normal_offset = current_pos;
                *founded_len = search_pos;
                return TRUE;
                break;

            default:
                break;
            }
            if (!founded)
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

static gboolean
mc_search__run_regex (mc_search_t * mc_search, const void *user_data,
                      gsize start_search, gsize end_search, gsize * founded_len)
{

}

/*** public functions ****************************************************************************/

mc_search_t *
mc_search_new (const gchar * original, guint str_len)
{
    mc_search_t *mc_search;
    if (!original)
        return NULL;

    if (str_len == -1) {
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

    g_free (mc_search);
}

/* --------------------------------------------------------------------------------------------- */
gboolean
mc_search_run (mc_search_t * mc_search, const void *user_data, gsize start_search, gsize end_search,
               gsize * founded_len)
{
    gboolean ret=FALSE;
    if (!mc_search)
        return FALSE;

    if (!mc_search->conditions)
	mc_search->conditions = mc_search__conditions_new (mc_search->original, mc_search->original_len,
                                                       mc_search->is_all_charsets,
                                                       mc_search->is_case_sentitive);

    mc_search->error = MC_SEARCH_E_OK;
    if (mc_search->error_str) {
        g_free (mc_search->error_str);
        mc_search->error_str = NULL;
    }

    switch (mc_search->search_type) {
    case MC_SEARCH_T_NORMAL:
        ret = mc_search__run_normal (mc_search, user_data, start_search, end_search, founded_len);
        break;
    case MC_SEARCH_T_REGEX:
//        ret = mc_search__run_regex (mc_search, user_data, start_search, end_search, founded_len);
//        break;
    case MC_SEARCH_T_HEX:
    case MC_SEARCH_T_SCANF:
    case MC_SEARCH_T_GLOB:
    default:
        mc_search->error = MC_SEARCH_E_INPUT;
	mc_search->error_str = g_strdup (_(STR_E_UNKNOWN_TYPE));
	ret = FALSE;
        break;
    }
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

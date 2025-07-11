/*
   Search text engine.
   Interface functions

   Copyright (C) 2009-2025
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009
   Andrew Borodin <aborodin@vmail.ru>, 2013

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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/search.h"
#include "lib/util.h"
#include "lib/charsets.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static const mc_search_type_str_t mc_search__list_types[] = {
    { N_ ("No&rmal"), MC_SEARCH_T_NORMAL },
    { N_ ("Re&gular expression"), MC_SEARCH_T_REGEX },
    { N_ ("He&xadecimal"), MC_SEARCH_T_HEX },
    { N_ ("Wil&dcard search"), MC_SEARCH_T_GLOB },
    { NULL, MC_SEARCH_T_INVALID }
};

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static mc_search_cond_t *
mc_search__cond_struct_new (mc_search_t *lc_mc_search, const GString *str, const char *charset)
{
    mc_search_cond_t *mc_search_cond;

    mc_search_cond = g_malloc0 (sizeof (mc_search_cond_t));
    mc_search_cond->str = mc_g_string_dup (str);
    mc_search_cond->charset = g_strdup (charset);

    switch (lc_mc_search->search_type)
    {
    case MC_SEARCH_T_GLOB:
        mc_search__cond_struct_new_init_glob (charset, lc_mc_search, mc_search_cond);
        break;
    case MC_SEARCH_T_NORMAL:
        mc_search__cond_struct_new_init_normal (charset, lc_mc_search, mc_search_cond);
        break;
    case MC_SEARCH_T_REGEX:
        mc_search__cond_struct_new_init_regex (charset, lc_mc_search, mc_search_cond);
        break;
    case MC_SEARCH_T_HEX:
        mc_search__cond_struct_new_init_hex (charset, lc_mc_search, mc_search_cond);
        break;
    default:
        break;
    }
    return mc_search_cond;
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_search__cond_struct_free (gpointer data)
{
    mc_search_cond_t *mc_search_cond = (mc_search_cond_t *) data;

    if (mc_search_cond->upper != NULL)
        g_string_free (mc_search_cond->upper, TRUE);

    if (mc_search_cond->lower != NULL)
        g_string_free (mc_search_cond->lower, TRUE);

    g_string_free (mc_search_cond->str, TRUE);
    g_free (mc_search_cond->charset);

    if (mc_search_cond->regex_handle != NULL)
        g_regex_unref (mc_search_cond->regex_handle);

    g_free (mc_search_cond);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/* Init search descriptor.
 *
 * @param original pattern to search
 * @param original_charset charset of #original. If NULL then cp_display will be used
 *
 * @return new mc_search_t object. Use #mc_search_free() to free it.
 */

mc_search_t *
mc_search_new (const gchar *original, const gchar *original_charset)
{
    if (original == NULL)
        return NULL;

    return mc_search_new_len (original, strlen (original), original_charset);
}

/* --------------------------------------------------------------------------------------------- */
/* Init search descriptor.
 *
 * @param original pattern to search
 * @param original_len length of #original or -1 if #original is NULL-terminated
 * @param original_charset charset of #original. If NULL then cp_display will be used
 *
 * @return new mc_search_t object. Use #mc_search_free() to free it.
 */

mc_search_t *
mc_search_new_len (const gchar *original, gsize original_len, const gchar *original_charset)
{
    mc_search_t *lc_mc_search;

    if (original == NULL || original_len == 0)
        return NULL;

    lc_mc_search = g_new0 (mc_search_t, 1);
    lc_mc_search->original.str = g_string_new_len (original, original_len);
    lc_mc_search->original.charset = g_strdup (
        original_charset != NULL && *original_charset != '\0' ? original_charset : cp_display);

    return lc_mc_search;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_search_free (mc_search_t *lc_mc_search)
{
    if (lc_mc_search == NULL)
        return;

    g_string_free (lc_mc_search->original.str, TRUE);
    g_free (lc_mc_search->original.charset);
    g_free (lc_mc_search->error_str);

    if (lc_mc_search->prepared.conditions != NULL)
        g_ptr_array_free (lc_mc_search->prepared.conditions, TRUE);

    if (lc_mc_search->regex_match_info != NULL)
        g_match_info_free (lc_mc_search->regex_match_info);

    if (lc_mc_search->regex_buffer != NULL)
        g_string_free (lc_mc_search->regex_buffer, TRUE);

    g_free (lc_mc_search);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_search_prepare (mc_search_t *lc_mc_search)
{
    GPtrArray *ret;

    if (lc_mc_search->prepared.conditions != NULL)
        return lc_mc_search->prepared.result;

    ret = g_ptr_array_new_with_free_func (mc_search__cond_struct_free);
    if (!lc_mc_search->is_all_charsets)
        g_ptr_array_add (ret,
                         mc_search__cond_struct_new (lc_mc_search, lc_mc_search->original.str,
                                                     lc_mc_search->original.charset));
    else
    {
        gsize loop1;

        for (loop1 = 0; loop1 < codepages->len; loop1++)
        {
            const char *id = get_codepage_id (loop1);

            if (g_ascii_strcasecmp (id, lc_mc_search->original.charset) == 0)
                g_ptr_array_add (ret,
                                 mc_search__cond_struct_new (lc_mc_search,
                                                             lc_mc_search->original.str,
                                                             lc_mc_search->original.charset));
            else
            {
                GString *buffer;

                buffer = mc_search__recode_str (lc_mc_search->original.str->str,
                                                lc_mc_search->original.str->len,
                                                lc_mc_search->original.charset, id);
                g_ptr_array_add (ret, mc_search__cond_struct_new (lc_mc_search, buffer, id));
                g_string_free (buffer, TRUE);
            }
        }
    }

    lc_mc_search->prepared.conditions = ret;
    lc_mc_search->prepared.result = (lc_mc_search->error == MC_SEARCH_E_OK);

    return lc_mc_search->prepared.result;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Carries out the search.
 *
 * Returns TRUE if found.
 *
 * Returns FALSE if not found. In this case, lc_mc_search->error reveals
 * the reason:
 *
 *   - MC_SEARCH_E_NOTFOUND: the pattern isn't in the subject string.
 *   - MC_SEARCH_E_ABORT: the user aborted the search.
 *   - For any other reason (but not for the above two!): the description
 *     is in lc_mc_search->error_str.
 */
gboolean
mc_search_run (mc_search_t *lc_mc_search, const void *user_data, off_t start_search,
               off_t end_search, gsize *found_len)
{
    gboolean ret = FALSE;

    if (lc_mc_search == NULL || user_data == NULL)
        return FALSE;
    if (!mc_search_is_type_avail (lc_mc_search->search_type))
    {
        mc_search_set_error (lc_mc_search, MC_SEARCH_E_INPUT, "%s", _ (STR_E_UNKNOWN_TYPE));
        return FALSE;
    }

    if (lc_mc_search->regex_match_info != NULL)
    {
        g_match_info_free (lc_mc_search->regex_match_info);
        lc_mc_search->regex_match_info = NULL;
    }

    mc_search_set_error (lc_mc_search, MC_SEARCH_E_OK, NULL);

    if (!mc_search_prepare (lc_mc_search))
        return FALSE;

    switch (lc_mc_search->search_type)
    {
    case MC_SEARCH_T_NORMAL:
        ret = mc_search__run_normal (lc_mc_search, user_data, start_search, end_search, found_len);
        break;
    case MC_SEARCH_T_REGEX:
        ret = mc_search__run_regex (lc_mc_search, user_data, start_search, end_search, found_len);
        break;
    case MC_SEARCH_T_GLOB:
        ret = mc_search__run_glob (lc_mc_search, user_data, start_search, end_search, found_len);
        break;
    case MC_SEARCH_T_HEX:
        ret = mc_search__run_hex (lc_mc_search, user_data, start_search, end_search, found_len);
        break;
    default:
        break;
    }
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_search_is_type_avail (mc_search_type_t search_type)
{
    switch (search_type)
    {
    case MC_SEARCH_T_GLOB:
    case MC_SEARCH_T_NORMAL:
    case MC_SEARCH_T_REGEX:
    case MC_SEARCH_T_HEX:
        return TRUE;
    default:
        break;
    }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

const mc_search_type_str_t *
mc_search_types_list_get (size_t *num)
{
    // don't count last NULL item
    if (num != NULL)
        *num = G_N_ELEMENTS (mc_search__list_types) - 1;

    return mc_search__list_types;
}

/* --------------------------------------------------------------------------------------------- */

GString *
mc_search_prepare_replace_str (mc_search_t *lc_mc_search, GString *replace_str)
{
    GString *ret;

    if (replace_str == NULL || replace_str->len == 0)
        return g_string_new ("");

    if (lc_mc_search == NULL)
        return mc_g_string_dup (replace_str);

    switch (lc_mc_search->search_type)
    {
    case MC_SEARCH_T_REGEX:
        ret = mc_search_regex_prepare_replace_str (lc_mc_search, replace_str);
        break;
    case MC_SEARCH_T_GLOB:
        ret = mc_search_glob_prepare_replace_str (lc_mc_search, replace_str);
        break;
    case MC_SEARCH_T_NORMAL:
        ret = mc_search_normal_prepare_replace_str (lc_mc_search, replace_str);
        break;
    case MC_SEARCH_T_HEX:
        ret = mc_search_hex_prepare_replace_str (lc_mc_search, replace_str);
        break;
    default:
        ret = mc_g_string_dup (replace_str);
        break;
    }
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

char *
mc_search_prepare_replace_str2 (mc_search_t *lc_mc_search, const char *replace_str)
{
    GString *ret;
    GString *replace_str2;

    replace_str2 = g_string_new (replace_str);
    ret = mc_search_prepare_replace_str (lc_mc_search, replace_str2);
    g_string_free (replace_str2, TRUE);
    return (ret != NULL) ? g_string_free (ret, FALSE) : NULL;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_search_is_fixed_search_str (const mc_search_t *lc_mc_search)
{
    if (lc_mc_search == NULL)
        return FALSE;
    switch (lc_mc_search->search_type)
    {
    case MC_SEARCH_T_REGEX:
    case MC_SEARCH_T_GLOB:
        return FALSE;
    default:
        return TRUE;
    }
}

/* --------------------------------------------------------------------------------------------- */
/* Search specified pattern in specified string.
 *
 * @param pattern string to search
 * @param pattern_charset charset of #pattern. If NULL then cp_display will be used
 * @param str string where search #pattern
 * @param search type (normal, regex, hex or glob)
 *
 * @return TRUE if found is successful, FALSE otherwise.
 */

gboolean
mc_search (const gchar *pattern, const gchar *pattern_charset, const gchar *str,
           mc_search_type_t type)
{
    gboolean ret;
    mc_search_t *search;

    if (str == NULL)
        return FALSE;

    search = mc_search_new (pattern, pattern_charset);
    if (search == NULL)
        return FALSE;

    search->search_type = type;
    search->is_case_sensitive = TRUE;

    if (type == MC_SEARCH_T_GLOB)
        search->is_entire_line = TRUE;

    ret = mc_search_run (search, str, 0, strlen (str), NULL);
    mc_search_free (search);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

int
mc_search_getstart_result_by_num (mc_search_t *lc_mc_search, int lc_index)
{
    if (lc_mc_search == NULL)
        return 0;
    if (lc_mc_search->search_type == MC_SEARCH_T_NORMAL)
        return 0;
    {
        gint start_pos;
        gint end_pos;

        g_match_info_fetch_pos (lc_mc_search->regex_match_info, lc_index, &start_pos, &end_pos);
        return (int) start_pos;
    }
}

/* --------------------------------------------------------------------------------------------- */

int
mc_search_getend_result_by_num (mc_search_t *lc_mc_search, int lc_index)
{
    if (lc_mc_search == NULL)
        return 0;
    if (lc_mc_search->search_type == MC_SEARCH_T_NORMAL)
        return 0;
    {
        gint start_pos;
        gint end_pos;

        g_match_info_fetch_pos (lc_mc_search->regex_match_info, lc_index, &start_pos, &end_pos);
        return (int) end_pos;
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Replace an old error code and message of an mc_search_t object.
 *
 * @param mc_search mc_search_t object
 * @param code error code, one of mc_search_error_t values
 * @param format format of error message. If NULL, the old error string is free'd and become NULL
 */

void
mc_search_set_error (mc_search_t *lc_mc_search, mc_search_error_t code, const gchar *format, ...)
{
    lc_mc_search->error = code;

    MC_PTR_FREE (lc_mc_search->error_str);

    if (format != NULL)
    {
        va_list args;

        va_start (args, format);
        lc_mc_search->error_str = g_strdup_vprintf (format, args);
        va_end (args);
    }
}

/* --------------------------------------------------------------------------------------------- */

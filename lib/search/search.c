/*
   Search text engine.
   Interface functions

   Copyright (C) 2009, 2011
   The Free Software Foundation, Inc.

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

#include <stdlib.h>
#include <sys/types.h>

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/search.h"
#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static const mc_search_type_str_t mc_search__list_types[] = {
    {N_("No&rmal"), MC_SEARCH_T_NORMAL},
    {N_("Re&gular expression"), MC_SEARCH_T_REGEX},
    {N_("He&xadecimal"), MC_SEARCH_T_HEX},
    {N_("Wil&dcard search"), MC_SEARCH_T_GLOB},
    {NULL, -1}
};

/*** file scope functions ************************************************************************/

static mc_search_cond_t *
mc_search__cond_struct_new (mc_search_t * lc_mc_search, const char *str,
                            gsize str_len, const char *charset)
{
    mc_search_cond_t *mc_search_cond;
    mc_search_cond = g_malloc0 (sizeof (mc_search_cond_t));

    mc_search_cond->str = g_string_new_len (str, str_len);
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
mc_search__cond_struct_free (mc_search_cond_t * mc_search_cond)
{
    if (mc_search_cond->upper)
        g_string_free (mc_search_cond->upper, TRUE);

    if (mc_search_cond->lower)
        g_string_free (mc_search_cond->lower, TRUE);

    g_string_free (mc_search_cond->str, TRUE);
    g_free (mc_search_cond->charset);

#ifdef SEARCH_TYPE_GLIB
    if (mc_search_cond->regex_handle)
        g_regex_unref (mc_search_cond->regex_handle);
#else /* SEARCH_TYPE_GLIB */
    g_free (mc_search_cond->regex_handle);
#endif /* SEARCH_TYPE_GLIB */

    g_free (mc_search_cond);
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_search__conditions_free (GPtrArray * array)
{
    gsize loop1;
    mc_search_cond_t *lc_mc_search;

    for (loop1 = 0; loop1 < array->len; loop1++)
    {
        lc_mc_search = (mc_search_cond_t *) g_ptr_array_index (array, loop1);
        mc_search__cond_struct_free (lc_mc_search);
    }
    g_ptr_array_free (array, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

/*** public functions ****************************************************************************/

mc_search_t *
mc_search_new (const gchar * original, gsize str_len)
{
    mc_search_t *lc_mc_search;
    if (!original)
        return NULL;

    if ((gssize) str_len == -1)
    {
        str_len = strlen (original);
        if (str_len == 0)
            return NULL;
    }

    lc_mc_search = g_malloc0 (sizeof (mc_search_t));
    lc_mc_search->original = g_strndup (original, str_len);
    lc_mc_search->original_len = str_len;
    return lc_mc_search;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_search_free (mc_search_t * lc_mc_search)
{
    if (lc_mc_search == NULL)
        return;

    g_free (lc_mc_search->original);
    g_free (lc_mc_search->error_str);

    if (lc_mc_search->conditions)
        mc_search__conditions_free (lc_mc_search->conditions);

#ifdef SEARCH_TYPE_GLIB
    if (lc_mc_search->regex_match_info)
        g_match_info_free (lc_mc_search->regex_match_info);
#else /* SEARCH_TYPE_GLIB */
    g_free (lc_mc_search->regex_match_info);
#endif /* SEARCH_TYPE_GLIB */

    if (lc_mc_search->regex_buffer != NULL)
        g_string_free (lc_mc_search->regex_buffer, TRUE);

    g_free (lc_mc_search);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_search_prepare (mc_search_t * lc_mc_search)
{
    GPtrArray *ret;
    ret = g_ptr_array_new ();
#ifdef HAVE_CHARSET
    if (lc_mc_search->is_all_charsets)
    {
        gsize loop1, recoded_str_len;
        gchar *buffer;
        for (loop1 = 0; loop1 < codepages->len; loop1++)
        {
            const char *id = ((codepage_desc *) g_ptr_array_index (codepages, loop1))->id;
            if (!g_ascii_strcasecmp (id, cp_source))
            {
                g_ptr_array_add (ret,
                                 mc_search__cond_struct_new (lc_mc_search, lc_mc_search->original,
                                                             lc_mc_search->original_len,
                                                             cp_source));
                continue;
            }

            buffer =
                mc_search__recode_str (lc_mc_search->original, lc_mc_search->original_len,
                                       cp_source, id, &recoded_str_len);

            g_ptr_array_add (ret,
                             mc_search__cond_struct_new (lc_mc_search, buffer,
                                                         recoded_str_len, id));
            g_free (buffer);
        }
    }
    else
    {
        g_ptr_array_add (ret,
                         (gpointer) mc_search__cond_struct_new (lc_mc_search,
                                                                lc_mc_search->original,
                                                                lc_mc_search->original_len,
                                                                cp_source));
    }
#else
    g_ptr_array_add (ret,
                     (gpointer) mc_search__cond_struct_new (lc_mc_search, lc_mc_search->original,
                                                            lc_mc_search->original_len,
                                                            str_detect_termencoding ()));
#endif
    lc_mc_search->conditions = ret;

    return (lc_mc_search->error == MC_SEARCH_E_OK);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_search_run (mc_search_t * lc_mc_search, const void *user_data,
               gsize start_search, gsize end_search, gsize * found_len)
{
    gboolean ret = FALSE;

    if (lc_mc_search == NULL || user_data == NULL)
        return FALSE;
    if (!mc_search_is_type_avail (lc_mc_search->search_type))
    {
        lc_mc_search->error = MC_SEARCH_E_INPUT;
        lc_mc_search->error_str = g_strdup (_(STR_E_UNKNOWN_TYPE));
        return FALSE;
    }
#ifdef SEARCH_TYPE_GLIB
    if (lc_mc_search->regex_match_info)
    {
        g_match_info_free (lc_mc_search->regex_match_info);
        lc_mc_search->regex_match_info = NULL;
    }
#endif /* SEARCH_TYPE_GLIB */

    lc_mc_search->error = MC_SEARCH_E_OK;
    g_free (lc_mc_search->error_str);
    lc_mc_search->error_str = NULL;

    if ((lc_mc_search->conditions == NULL) && !mc_search_prepare (lc_mc_search))
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
mc_search_types_list_get (size_t * num)
{
    /* don't count last NULL item */
    if (num != NULL)
        *num = sizeof (mc_search__list_types) / sizeof (mc_search__list_types[0]) - 1;

    return mc_search__list_types;
}

/* --------------------------------------------------------------------------------------------- */

GString *
mc_search_prepare_replace_str (mc_search_t * lc_mc_search, GString * replace_str)
{
    GString *ret;

    if (lc_mc_search == NULL)
        return g_string_new_len (replace_str->str, replace_str->len);

    if (replace_str == NULL || replace_str->str == NULL || replace_str->len == 0)
        return g_string_new ("");

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
        ret = g_string_new_len (replace_str->str, replace_str->len);
        break;
    }
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

char *
mc_search_prepare_replace_str2 (mc_search_t * lc_mc_search, char *replace_str)
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
mc_search_is_fixed_search_str (mc_search_t * lc_mc_search)
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

gboolean
mc_search (const gchar * pattern, const gchar * str, mc_search_type_t type)
{
    gboolean ret;
    mc_search_t *search;

    if (str == NULL)
        return FALSE;

    search = mc_search_new (pattern, -1);
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
mc_search_getstart_result_by_num (mc_search_t * lc_mc_search, int lc_index)
{
    if (!lc_mc_search)
        return 0;
    if (lc_mc_search->search_type == MC_SEARCH_T_NORMAL)
        return 0;
#ifdef SEARCH_TYPE_GLIB
    {
        gint start_pos;
        gint end_pos;
        g_match_info_fetch_pos (lc_mc_search->regex_match_info, lc_index, &start_pos, &end_pos);
        return (int) start_pos;
    }
#else /* SEARCH_TYPE_GLIB */
    return lc_mc_search->iovector[lc_index * 2];
#endif /* SEARCH_TYPE_GLIB */
}

/* --------------------------------------------------------------------------------------------- */

int
mc_search_getend_result_by_num (mc_search_t * lc_mc_search, int lc_index)
{
    if (!lc_mc_search)
        return 0;
    if (lc_mc_search->search_type == MC_SEARCH_T_NORMAL)
        return 0;
#ifdef SEARCH_TYPE_GLIB
    {
        gint start_pos;
        gint end_pos;
        g_match_info_fetch_pos (lc_mc_search->regex_match_info, lc_index, &start_pos, &end_pos);
        return (int) end_pos;
    }
#else /* SEARCH_TYPE_GLIB */
    return lc_mc_search->iovector[lc_index * 2 + 1];
#endif /* SEARCH_TYPE_GLIB */
}

/* --------------------------------------------------------------------------------------------- */

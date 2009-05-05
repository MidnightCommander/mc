/*
   Search text engine.
   Interface functions

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

static const mc_search_type_str_t mc_search__list_types[] = {
    {N_("Normal"), MC_SEARCH_T_NORMAL},
    {N_("&Regular expression"), MC_SEARCH_T_REGEX},
    {N_("Hexadecimal"), MC_SEARCH_T_HEX},
    {N_("Wildcard search"), MC_SEARCH_T_GLOB},
    {NULL, -1}
};

/*** file scope functions ************************************************************************/

static mc_search_cond_t *
mc_search__cond_struct_new (mc_search_t * mc_search, const char *str,
                            gsize str_len, const char *charset)
{
    mc_search_cond_t *mc_search_cond;
    mc_search_cond = g_malloc0 (sizeof (mc_search_cond_t));

    mc_search_cond->str = g_string_new_len (str, str_len);
    mc_search_cond->len = str_len;
    mc_search_cond->charset = g_strdup (charset);

    switch (mc_search->search_type) {
    case MC_SEARCH_T_GLOB:
        mc_search__cond_struct_new_init_glob (charset, mc_search, mc_search_cond);
        break;
    case MC_SEARCH_T_NORMAL:
        mc_search__cond_struct_new_init_normal (charset, mc_search, mc_search_cond);
        break;
    case MC_SEARCH_T_REGEX:
        mc_search__cond_struct_new_init_regex (charset, mc_search, mc_search_cond);
        break;
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
            if (!g_ascii_strcasecmp (codepages[loop1].id, cp_source)) {
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
    if (mc_search_cond->regex_handle)
        g_regex_unref (mc_search_cond->regex_handle);
#else /* GLIB_CHECK_VERSION (2, 14, 0) */
#if HAVE_LIBPCRE
    if (mc_search_cond->regex_handle)
        free (mc_search_cond->regex_handle);
#else /* HAVE_LIBPCRE */
    if (mc_search_cond->regex_handle)
        regfree (mc_search_cond->regex_handle);
#endif /* HAVE_LIBPCRE */
#endif /* GLIB_CHECK_VERSION (2, 14, 0) */

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
#else /* GLIB_CHECK_VERSION (2, 14, 0) */
#if HAVE_LIBPCRE
    if (mc_search->regex_match_info)
        free (mc_search->regex_match_info);
#else /* HAVE_LIBPCRE */
    if (mc_search->regex_match_info)
        g_free (mc_search->regex_match_info);
#endif /* HAVE_LIBPCRE */
#endif /* GLIB_CHECK_VERSION (2, 14, 0) */

    if (mc_search->regex_buffer != NULL)
        g_string_free (mc_search->regex_buffer, TRUE);

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
#endif /* GLIB_CHECK_VERSION (2, 14, 0) */

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
    case MC_SEARCH_T_GLOB:
        ret = mc_search__run_glob (mc_search, user_data, start_search, end_search, found_len);
        break;
    case MC_SEARCH_T_HEX:
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
    case MC_SEARCH_T_GLOB:
    case MC_SEARCH_T_NORMAL:
        return TRUE;
        break;
    case MC_SEARCH_T_REGEX:
        return TRUE;
        break;
    case MC_SEARCH_T_HEX:
    default:
        break;
    }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

mc_search_type_str_t *
mc_search_types_list_get (void)
{
    return mc_search__list_types;
}

/* --------------------------------------------------------------------------------------------- */

GString *
mc_search_prepare_replace_str (mc_search_t * mc_search, GString * replace_str)
{
    GString *ret;

    if (mc_search == NULL)
        return g_string_new_len (replace_str->str, replace_str->len);

    switch (mc_search->search_type) {
    case MC_SEARCH_T_REGEX:
        ret = mc_search_regex_prepare_replace_str (mc_search, replace_str);
        break;
    case MC_SEARCH_T_NORMAL:
    case MC_SEARCH_T_HEX:
    case MC_SEARCH_T_GLOB:
    default:
        ret = g_string_new_len (replace_str->str, replace_str->len);
        break;
    }
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_search_is_fixed_search_str (mc_search_t * mc_search)
{
    if (mc_search == NULL)
        return FALSE;
    switch (mc_search->search_type) {
    case MC_SEARCH_T_REGEX:
    case MC_SEARCH_T_GLOB:
        return FALSE;
        break;
    default:
        return TRUE;
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */

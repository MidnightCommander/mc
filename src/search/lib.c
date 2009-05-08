/*
   Search text engine.
   Common share code for module.

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

char STR_E_NOTFOUND[] = N_(" Search string not found ");
char STR_E_UNKNOWN_TYPE[] = N_(" Not implemented yet ");
char STR_E_RPL_NOT_EQ_TO_FOUND[] = N_(" Num of replace tokens not equal to num of found tokens ");
char STR_E_RPL_INVALID_TOKEN[] = N_(" Invalid token number %d ");

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/*** public functions ****************************************************************************/

gchar *
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
    if (conv == INVALID_CONV)
        return NULL;

    ret = g_convert_with_iconv (str, str_len, conv, &bytes_read, bytes_written, NULL);
    g_iconv_close (conv);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

gchar *
mc_search__get_one_symbol (const char *charset, const char *str, gsize str_len,
                           gboolean * just_letters)
{

    gchar *converted_str, *next_char;

    gsize tmp_len;
#ifdef HAVE_CHARSET
    gsize converted_str_len;
    gchar *converted_str2;

    converted_str = mc_search__recode_str (str, str_len, charset, cp_display, &converted_str_len);
#else
    converted_str = g_strndup(str, str_len);
#endif

    next_char = (char *) str_cget_next_char (converted_str);

    tmp_len = next_char - converted_str;

    converted_str[tmp_len] = '\0';

#ifdef HAVE_CHARSET
    converted_str2 =
        mc_search__recode_str (converted_str, tmp_len, cp_display, charset, &converted_str_len);
#endif

    if (str_isalnum (converted_str) && !str_isdigit (converted_str))
        *just_letters = TRUE;
    else
        *just_letters = FALSE;


#ifdef HAVE_CHARSET
    g_free (converted_str);
    return converted_str2;
#else
    return converted_str;
#endif
}

/* --------------------------------------------------------------------------------------------- */
int
mc_search__get_char (mc_search_t * mc_search, const void *user_data, gsize current_pos)
{
    char *data;
    if (mc_search->search_fn)
        return (mc_search->search_fn) (user_data, current_pos);

    data = (char *) user_data;
    return (int) (unsigned char) data[current_pos];
}

/* --------------------------------------------------------------------------------------------- */


GString *
mc_search__tolower_case_str (const char *charset, const char *str, gsize str_len)
{
    GString *ret;
#ifdef HAVE_CHARSET
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

    ret = g_string_new_len (tmp_str2, tmp_len);
    g_free(tmp_str2);
    return ret;
#else
    const gchar *tmp_str1 = str;
    gchar *converted_str, *tmp_str2;
    gsize converted_str_len = str_len;

    tmp_str2 = converted_str = g_strndup(str, str_len);

    while (str_tolower (tmp_str1, &tmp_str2, &converted_str_len))
        tmp_str1 += str_length_char (tmp_str1);

    ret = g_string_new_len (converted_str, str_len);
    g_free(converted_str);
    return ret;
#endif
}

/* --------------------------------------------------------------------------------------------- */

GString *
mc_search__toupper_case_str (const char *charset, const char *str, gsize str_len)
{
    GString *ret;
#ifdef HAVE_CHARSET
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

    ret = g_string_new_len (tmp_str2, tmp_len);
    g_free(tmp_str2);
    return ret;
#else
    const gchar *tmp_str1 = str;
    gchar *converted_str, *tmp_str2;
    gsize converted_str_len = str_len;

    tmp_str2 = converted_str = g_strndup(str, str_len);

    while (str_toupper (tmp_str1, &tmp_str2, &converted_str_len))
        tmp_str1 += str_length_char (tmp_str1);

    ret = g_string_new_len (converted_str, str_len);
    g_free(converted_str);
    return ret;
#endif
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_search__regex_is_char_escaped (const char *start, const char *current)
{
    int num_esc = 0;
    while (*current == '\\' && current >= start) {
        num_esc++;
        current--;
    }
    return (gboolean) num_esc % 2;
}

/* --------------------------------------------------------------------------------------------- */

gchar **
mc_search_get_types_strings_array (void)
{
    GString *tmp;
    gchar **ret;
    const mc_search_type_str_t *type_str;
    const mc_search_type_str_t *types_str = mc_search_types_list_get ();

    tmp = g_string_new ("");
    type_str = types_str;
    while (type_str->str) {
        if (tmp->len)
            g_string_append (tmp, "__||__");
        g_string_append (tmp, type_str->str);
        type_str++;
    }
    ret = g_strsplit (tmp->str, "__||__", -1);
    g_string_free (tmp, TRUE);
    return ret;
}

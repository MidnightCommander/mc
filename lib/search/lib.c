/*
   Search text engine.
   Common share code for module.

   Copyright (C) 2009-2024
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009, 2011
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

/* *INDENT-OFF* */
const char *STR_E_NOTFOUND = N_("Search string not found");
const char *STR_E_UNKNOWN_TYPE = N_("Not implemented yet");
const char *STR_E_RPL_NOT_EQ_TO_FOUND =
            N_("Num of replace tokens not equal to num of found tokens");
const char *STR_E_RPL_INVALID_TOKEN = N_("Invalid token number %d");
/* *INDENT-ON* */

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef gboolean (*case_conv_fn) (const char *ch, char **out, size_t * remain);

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static GString *
mc_search__change_case_str (const char *charset, const GString * str, case_conv_fn case_conv)
{
    GString *ret;
    const char *src_ptr;
    gchar *dst_str;
    gchar *dst_ptr;
    gsize dst_len;
#ifdef HAVE_CHARSET
    GString *converted_str;

    if (charset == NULL)
        charset = cp_source;

    converted_str = mc_search__recode_str (str->str, str->len, charset, cp_display);

    dst_len = converted_str->len + 1;   /* +1 is required for str_toupper/str_tolower */
    dst_str = g_malloc (dst_len);

    for (src_ptr = converted_str->str, dst_ptr = dst_str;
         case_conv (src_ptr, &dst_ptr, &dst_len); src_ptr += str_length_char (src_ptr))
        ;
    *dst_ptr = '\0';

    dst_len = converted_str->len;
    g_string_free (converted_str, TRUE);

    ret = mc_search__recode_str (dst_str, dst_len, cp_display, charset);
    g_free (dst_str);
#else
    (void) charset;

    dst_len = str->len + 1;     /* +1 is required for str_toupper/str_tolower */
    dst_str = g_malloc (dst_len);

    for (src_ptr = str->str, dst_ptr = dst_str;
         case_conv (src_ptr, &dst_ptr, &dst_len); src_ptr += str_length_char (src_ptr))
        ;
    *dst_ptr = '\0';

    ret = g_string_new_len (dst_str, dst_len);
    g_free (dst_str);
#endif
    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

GString *
mc_search__recode_str (const char *str, gsize str_len, const char *charset_from,
                       const char *charset_to)
{
    GString *ret = NULL;

    if (charset_from != NULL && charset_to != NULL
        && g_ascii_strcasecmp (charset_to, charset_from) != 0)
    {
        GIConv conv;

        conv = g_iconv_open (charset_to, charset_from);
        if (conv != INVALID_CONV)
        {
            gchar *val;
            gsize bytes_read = 0;
            gsize bytes_written = 0;

            val = g_convert_with_iconv (str, str_len, conv, &bytes_read, &bytes_written, NULL);

            g_iconv_close (conv);

            if (val != NULL)
            {
                ret = g_string_new_len (val, bytes_written);
                g_free (val);
            }
        }
    }

    if (ret == NULL)
        ret = g_string_new_len (str, str_len);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

GString *
mc_search__get_one_symbol (const char *charset, const char *str, gsize str_len,
                           gboolean * just_letters)
{
    GString *converted_str;
    const gchar *next_char;

#ifdef HAVE_CHARSET
    GString *converted_str2;

    if (charset == NULL)
        charset = cp_source;

    converted_str = mc_search__recode_str (str, str_len, charset, cp_display);
#else
    (void) charset;

    converted_str = g_string_new_len (str, str_len);
#endif

    next_char = str_cget_next_char (converted_str->str);
    g_string_set_size (converted_str, (gsize) (next_char - converted_str->str));

#ifdef HAVE_CHARSET
    converted_str2 =
        mc_search__recode_str (converted_str->str, converted_str->len, cp_display, charset);
#endif
    if (just_letters != NULL)
        *just_letters = str_isalnum (converted_str->str) && !str_isdigit (converted_str->str);
#ifdef HAVE_CHARSET
    g_string_free (converted_str, TRUE);
    return converted_str2;
#else
    return converted_str;
#endif
}

/* --------------------------------------------------------------------------------------------- */

GString *
mc_search__tolower_case_str (const char *charset, const GString * str)
{
    return mc_search__change_case_str (charset, str, str_tolower);
}

/* --------------------------------------------------------------------------------------------- */

GString *
mc_search__toupper_case_str (const char *charset, const GString * str)
{
    return mc_search__change_case_str (charset, str, str_toupper);
}

/* --------------------------------------------------------------------------------------------- */

gchar **
mc_search_get_types_strings_array (size_t * num)
{
    gchar **ret;
    int lc_index;
    size_t n;

    const mc_search_type_str_t *type_str;
    const mc_search_type_str_t *types_str = mc_search_types_list_get (&n);

    ret = g_try_new0 (char *, n + 1);
    if (ret == NULL)
        return NULL;

    for (lc_index = 0, type_str = types_str; type_str->str != NULL; type_str++, lc_index++)
        ret[lc_index] = g_strdup (type_str->str);

    /* don't count last NULL item */
    if (num != NULL)
        *num = (size_t) lc_index;

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

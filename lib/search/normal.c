/*
   Search text engine.
   Plain search

   Copyright (C) 2009-2026
   Free Software Foundation, Inc.

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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/search.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
mc_search__normal_translate_to_regex (GString *str)
{
    gsize loop;

    for (loop = 0; loop < str->len; loop++)
        switch (str->str[loop])
        {
        case '*':
        case '?':
        case ',':
        case '{':
        case '}':
        case '[':
        case ']':
        case '\\':
        case '+':
        case '.':
        case '$':
        case '(':
        case ')':
        case '^':
        case '-':
        case '|':
            g_string_insert_c (str, loop, '\\');
            loop++;
            break;
        default:
            break;
        }
}

/* --------------------------------------------------------------------------------------------- */

/* Letters without a canonical decomposition, folded by hand */
static const struct
{
    gunichar from;
    gunichar to;
} accents_extra[] = {
    { 0x00D8, 'O' },  // Ø
    { 0x00F8, 'o' },  // ø
    { 0x0110, 'D' },  // Đ
    { 0x0111, 'd' },  // đ
    { 0x0126, 'H' },  // Ħ
    { 0x0127, 'h' },  // ħ
    { 0x0131, 'i' },  // ı
    { 0x013F, 'L' },  // Ŀ
    { 0x0140, 'l' },  // ŀ
    { 0x0141, 'L' },  // Ł
    { 0x0142, 'l' },  // ł
    { 0x0166, 'T' },  // Ŧ
    { 0x0167, 't' },  // ŧ
    { 0x0180, 'b' },  // ƀ
    { 0x0197, 'I' },  // Ɨ
};

/* Ranges of the characters that can match a letter of the search string, besides its case
 * variants: Latin, Greek and Cyrillic letters with diacritics */
static const struct
{
    gunichar first;
    gunichar last;
} accents_ranges[] = {
    { 0x0041, 0x005A },  // A-Z
    { 0x0061, 0x007A },  // a-z
    { 0x00C0, 0x024F },  // Latin-1 Supplement, Latin Extended-A and -B
    { 0x0370, 0x04FF },  // Greek and Coptic, Cyrillic
    { 0x1E00, 0x1FFF },  // Latin Extended Additional, Greek Extended
};

/* --------------------------------------------------------------------------------------------- */
/**
 * Fold a character for a search ignoring accents: strip its diacritics (and the case, if needed).
 * The diacritics are stripped only if the character decomposes to a base character and
 * combining marks, so e.g. Hangul syllables are kept.
 */

static gunichar
mc_search__accents_fold (gunichar c, gboolean case_sensitive)
{
    gunichar d[G_UNICHAR_MAX_DECOMPOSITION_LENGTH];
    gsize n, i;

    n = g_unichar_fully_decompose (c, FALSE, d, G_N_ELEMENTS (d));
    for (i = 1; i < n && g_unichar_ismark (d[i]); i++)
        ;
    if (n > 1 && i == n)
        c = d[0];

    for (i = 0; i < G_N_ELEMENTS (accents_extra); i++)
        if (c == accents_extra[i].from)
        {
            c = accents_extra[i].to;
            break;
        }

    return case_sensitive ? c : g_unichar_tolower (c);
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_search__accents_add_candidate (GArray *candidates, gunichar c, gunichar folded,
                                  gboolean case_sensitive)
{
    guint i;

    if (mc_search__accents_fold (c, case_sensitive) != folded)
        return;

    for (i = 0; i < candidates->len; i++)
        if (g_array_index (candidates, gunichar, i) == c)
            return;

    g_array_append_val (candidates, c);
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_search__accents_append_hex (GString *str, const char *bytes, gsize len)
{
    gsize i;

    for (i = 0; i < len; i++)
        g_string_append_printf (str, "\\x%02x", (unsigned char) bytes[i]);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Translate a UTF-8 search string ignoring accents to a regex: every character is replaced with
 * the alternation of the characters that fold to the same, followed by any combining marks
 * (for decomposed text, e.g. NFD file names from macOS). Combining marks in the search string
 * are dropped.
 *
 * @param str        search string
 * @param case_sensitive TRUE if the case must match
 * @param utf8_regex TRUE if the regex is compiled for UTF-8, FALSE for raw bytes
 * @return regex, NULL if the search string isn't valid UTF-8
 */

static GString *
mc_search__accents_translate_utf8 (const GString *str, gboolean case_sensitive, gboolean utf8_regex)
{
    GString *ret;
    GArray *candidates;
    const char *p, *end;

    if (!g_utf8_validate (str->str, (gssize) str->len, NULL))
        return NULL;

    ret = g_string_sized_new (str->len * 8);
    candidates = g_array_new (FALSE, FALSE, sizeof (gunichar));

    for (p = str->str, end = str->str + str->len; p < end; p = g_utf8_next_char (p))
    {
        const gunichar c = g_utf8_get_char (p);
        gunichar folded;
        guint i;

        if (g_unichar_ismark (c))
            continue;

        folded = mc_search__accents_fold (c, case_sensitive);

        g_array_set_size (candidates, 0);
        mc_search__accents_add_candidate (candidates, c, folded, case_sensitive);
        mc_search__accents_add_candidate (candidates, folded, folded, case_sensitive);
        mc_search__accents_add_candidate (candidates, g_unichar_toupper (folded), folded,
                                          case_sensitive);
        for (i = 0; i < G_N_ELEMENTS (accents_ranges); i++)
        {
            gunichar r;

            for (r = accents_ranges[i].first; r <= accents_ranges[i].last; r++)
                mc_search__accents_add_candidate (candidates, r, folded, case_sensitive);
        }

        if (candidates->len > 1)
            g_string_append (ret, "(?:");

        for (i = 0; i < candidates->len; i++)
        {
            char utf8[6];
            const gint len = g_unichar_to_utf8 (g_array_index (candidates, gunichar, i), utf8);

            if (i != 0)
                g_string_append_c (ret, '|');

            if (utf8_regex)
            {
                char *escaped;

                escaped = g_regex_escape_string (utf8, len);
                g_string_append (ret, escaped);
                g_free (escaped);
            }
            else
                mc_search__accents_append_hex (ret, utf8, (gsize) len);
        }

        if (candidates->len > 1)
            g_string_append_c (ret, ')');

        // any combining marks after a letter (in raw bytes, U+0300..U+036F only)
        if (g_unichar_isalpha (folded))
            g_string_append (ret,
                             utf8_regex ? "\\p{M}*" : "(?:\\xcc[\\x80-\\xbf]|\\xcd[\\x80-\\xaf])*");
    }

    g_array_free (candidates, TRUE);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Translate a search string in an 8-bit charset ignoring accents to a regex for raw bytes: every
 * byte is replaced with the class of the bytes that fold to the same character in this charset.
 *
 * @return regex, NULL if the charset is unknown
 */

static GString *
mc_search__accents_translate_8bit (const char *charset, const GString *str, gboolean case_sensitive)
{
    GIConv conv;
    gunichar folded[256];
    GString *ret;
    gsize i;
    int b;

    conv = g_iconv_open ("UTF-8", charset);
    if (conv == (GIConv) (-1))
        return NULL;

    // the folded character of every byte, (gunichar) -1 if the byte isn't a character
    for (b = 0; b < 256; b++)
    {
        char in = (char) b, out[8];
        gchar *inbuf = &in, *outbuf = out;
        gsize inleft = 1, outleft = sizeof (out);

        folded[b] = (gunichar) (-1);

        g_iconv (conv, NULL, NULL, NULL, NULL);
        if (g_iconv (conv, &inbuf, &inleft, &outbuf, &outleft) != (gsize) (-1) && inleft == 0)
        {
            const gunichar c = g_utf8_get_char_validated (out, (gssize) (outbuf - out));

            if (c != (gunichar) (-1) && c != (gunichar) (-2))
                folded[b] = mc_search__accents_fold (c, case_sensitive);
        }
    }

    g_iconv_close (conv);

    ret = g_string_sized_new (str->len * 16);

    for (i = 0; i < str->len; i++)
    {
        const unsigned char n = (unsigned char) str->str[i];

        if (folded[n] == (gunichar) (-1))
            mc_search__accents_append_hex (ret, str->str + i, 1);
        else
        {
            g_string_append_c (ret, '[');
            for (b = 0; b < 256; b++)
                if (folded[b] == folded[n])
                    g_string_append_printf (ret, "\\x%02x", (unsigned int) b);
            g_string_append_c (ret, ']');
        }
    }

    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_search__cond_struct_new_init_normal (const char *charset, mc_search_t *lc_mc_search,
                                        mc_search_cond_t *mc_search_cond)
{
    GString *accents_regex = NULL;

    if (lc_mc_search->ignore_accents)
    {
        if (str_isutf8 (charset))
            accents_regex = mc_search__accents_translate_utf8 (
                mc_search_cond->str, lc_mc_search->is_case_sensitive, mc_global.utf8_display);
        else
            accents_regex = mc_search__accents_translate_8bit (charset, mc_search_cond->str,
                                                               lc_mc_search->is_case_sensitive);
    }

    if (accents_regex != NULL)
    {
        g_string_free (mc_search_cond->str, TRUE);
        mc_search_cond->str = accents_regex;
        mc_search_cond->case_handled = TRUE;
    }
    else
        mc_search__normal_translate_to_regex (mc_search_cond->str);

    mc_search__cond_struct_new_init_regex (charset, lc_mc_search, mc_search_cond);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_search__run_normal (mc_search_t *lc_mc_search, const void *user_data, off_t start_search,
                       off_t end_search, gsize *found_len)
{
    return mc_search__run_regex (lc_mc_search, user_data, start_search, end_search, found_len);
}

/* --------------------------------------------------------------------------------------------- */
GString *
mc_search_normal_prepare_replace_str (mc_search_t *lc_mc_search, GString *replace_str)
{
    (void) lc_mc_search;

    return mc_g_string_dup (replace_str);
}

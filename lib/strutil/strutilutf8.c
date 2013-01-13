/*
   UTF-8 strings utilities

   Copyright (C) 2007, 2011
   The Free Software Foundation, Inc.

   Written by:
   Rostislav Benes, 2007

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
#include <stdio.h>
#include <errno.h>
#include <glib.h>
#include <langinfo.h>
#include <string.h>

#include "lib/global.h"
#include "lib/strutil.h"

/* using function for utf-8 from glib */

static const char replch[] = "\xEF\xBF\xBD";

static gboolean
str_unichar_iscombiningmark (gunichar uni)
{
    GUnicodeType type;

    type = g_unichar_type (uni);
    return (type == G_UNICODE_COMBINING_MARK)
        || (type == G_UNICODE_ENCLOSING_MARK) || (type == G_UNICODE_NON_SPACING_MARK);
}

static void
str_utf8_insert_replace_char (GString * buffer)
{
    g_string_append (buffer, replch);
}

static int
str_utf8_is_valid_string (const char *text)
{
    return g_utf8_validate (text, -1, NULL);
}

static int
str_utf8_is_valid_char (const char *ch, size_t size)
{
    switch (g_utf8_get_char_validated (ch, size))
    {
    case (gunichar) (-2):
        return -2;
    case (gunichar) (-1):
        return -1;
    default:
        return 1;
    }
}

static void
str_utf8_cnext_char (const char **text)
{
    (*text) = g_utf8_next_char (*text);
}

static void
str_utf8_cprev_char (const char **text)
{
    (*text) = g_utf8_prev_char (*text);
}

static void
str_utf8_cnext_char_safe (const char **text)
{
    if (str_utf8_is_valid_char (*text, -1) == 1)
        (*text) = g_utf8_next_char (*text);
    else
        (*text)++;
}

static void
str_utf8_cprev_char_safe (const char **text)
{
    const char *result = g_utf8_prev_char (*text);
    const char *t = result;
    str_utf8_cnext_char_safe (&t);
    if (t == *text)
        (*text) = result;
    else
        (*text)--;
}

static void
str_utf8_fix_string (char *text)
{
    gunichar uni;

    while (text[0] != '\0')
    {
        uni = g_utf8_get_char_validated (text, -1);
        if ((uni != (gunichar) (-1)) && (uni != (gunichar) (-2)))
        {
            text = g_utf8_next_char (text);
        }
        else
        {
            text[0] = '?';
            text++;
        }
    }
}

static int
str_utf8_isspace (const char *text)
{
    gunichar uni = g_utf8_get_char_validated (text, -1);
    return g_unichar_isspace (uni);
}

static int
str_utf8_ispunct (const char *text)
{
    gunichar uni = g_utf8_get_char_validated (text, -1);
    return g_unichar_ispunct (uni);
}

static int
str_utf8_isalnum (const char *text)
{
    gunichar uni = g_utf8_get_char_validated (text, -1);
    return g_unichar_isalnum (uni);
}

static int
str_utf8_isdigit (const char *text)
{
    gunichar uni = g_utf8_get_char_validated (text, -1);
    return g_unichar_isdigit (uni);
}

static int
str_utf8_isprint (const char *ch)
{
    gunichar uni = g_utf8_get_char_validated (ch, -1);
    return g_unichar_isprint (uni);
}

static gboolean
str_utf8_iscombiningmark (const char *ch)
{
    gunichar uni = g_utf8_get_char_validated (ch, -1);
    return str_unichar_iscombiningmark (uni);
}

static int
str_utf8_cnext_noncomb_char (const char **text)
{
    int count = 0;
    while ((*text)[0] != '\0')
    {
        str_utf8_cnext_char_safe (text);
        count++;
        if (!str_utf8_iscombiningmark (*text))
            break;
    }
    return count;
}

static int
str_utf8_cprev_noncomb_char (const char **text, const char *begin)
{
    int count = 0;
    while ((*text) != begin)
    {
        str_utf8_cprev_char_safe (text);
        count++;
        if (!str_utf8_iscombiningmark (*text))
            break;
    }
    return count;
}

static int
str_utf8_toupper (const char *text, char **out, size_t * remain)
{
    gunichar uni;
    size_t left;

    uni = g_utf8_get_char_validated (text, -1);
    if (uni == (gunichar) (-1) || uni == (gunichar) (-2))
        return 0;

    uni = g_unichar_toupper (uni);
    left = g_unichar_to_utf8 (uni, NULL);
    if (left >= *remain)
        return 0;

    left = g_unichar_to_utf8 (uni, *out);
    (*out) += left;
    (*remain) -= left;
    return 1;
}

static int
str_utf8_tolower (const char *text, char **out, size_t * remain)
{
    gunichar uni;
    size_t left;

    uni = g_utf8_get_char_validated (text, -1);
    if (uni == (gunichar) (-1) || uni == (gunichar) (-2))
        return 0;

    uni = g_unichar_tolower (uni);
    left = g_unichar_to_utf8 (uni, NULL);
    if (left >= *remain)
        return 0;

    left = g_unichar_to_utf8 (uni, *out);
    (*out) += left;
    (*remain) -= left;
    return 1;
}

static int
str_utf8_length (const char *text)
{
    int result = 0;
    const char *start;
    const char *end;

    start = text;
    while (!g_utf8_validate (start, -1, &end) && start[0] != '\0')
    {
        if (start != end)
        {
            result += g_utf8_strlen (start, end - start);
        }
        result++;
        start = end + 1;
    }

    if (start == text)
    {
        result = g_utf8_strlen (text, -1);
    }
    else
    {
        if (start[0] != '\0' && start != end)
        {
            result += g_utf8_strlen (start, end - start);
        }
    }

    return result;
}

static int
str_utf8_length2 (const char *text, int size)
{
    int result = 0;
    const char *start;
    const char *end;

    start = text;
    while (!g_utf8_validate (start, -1, &end) && start[0] != '\0' && size > 0)
    {
        if (start != end)
        {
            result += g_utf8_strlen (start, min (end - start, size));
            size -= end - start;
        }
        result += (size > 0);
        size--;
        start = end + 1;
    }

    if (start == text)
    {
        result = g_utf8_strlen (text, size);
    }
    else
    {
        if (start[0] != '\0' && start != end && size > 0)
        {
            result += g_utf8_strlen (start, min (end - start, size));
        }
    }

    return result;
}

static int
str_utf8_length_noncomb (const char *text)
{
    int result = 0;
    const char *t = text;

    while (t[0] != '\0')
    {
        str_utf8_cnext_noncomb_char (&t);
        result++;
    }

    return result;
}

/*
   static void
   str_utf8_questmark_sustb (char **string, size_t * left, GString * buffer)
   {
   char *next = g_utf8_next_char (*string);
   (*left) -= next - (*string);
   (*string) = next;
   g_string_append_c (buffer, '?');
   }
 */

static gchar *
str_utf8_conv_gerror_message (GError * error, const char *def_msg)
{
    if ((error != NULL) && (error->message != NULL))
        return g_strdup (error->message);

    return g_strdup (def_msg != NULL ? def_msg : "");
}

static estr_t
str_utf8_vfs_convert_to (GIConv coder, const char *string, int size, GString * buffer)
{
    estr_t result;

    if (coder == str_cnv_not_convert)
    {
        g_string_append_len (buffer, string, size);
        result = ESTR_SUCCESS;
    }
    else
        result = str_nconvert (coder, (char *) string, size, buffer);

    return result;
}

struct term_form
{
    char text[BUF_MEDIUM * 6];
    size_t width;
    gboolean compose;
};

/* utiliti function, that make string valid in utf8 and all characters printable
 * return width of string too*/
static const struct term_form *
str_utf8_make_make_term_form (const char *text, size_t length)
{
    static struct term_form result;
    gunichar uni;
    size_t left;
    char *actual;

    result.text[0] = '\0';
    result.width = 0;
    result.compose = FALSE;
    actual = result.text;

    /* check if text start with combining character,
     * add space at begin in this case */
    if (length != 0 && text[0] != '\0')
    {
        uni = g_utf8_get_char_validated (text, -1);
        if ((uni != (gunichar) (-1)) && (uni != (gunichar) (-2)))
        {
            if (str_unichar_iscombiningmark (uni))
            {
                actual[0] = ' ';
                actual++;
                result.width++;
                result.compose = TRUE;
            }
        }
    }

    while (length != 0 && text[0] != '\0')
    {
        uni = g_utf8_get_char_validated (text, -1);
        if ((uni != (gunichar) (-1)) && (uni != (gunichar) (-2)))
        {
            if (g_unichar_isprint (uni))
            {
                left = g_unichar_to_utf8 (uni, actual);
                actual += left;
                if (str_unichar_iscombiningmark (uni))
                    result.compose = TRUE;
                else
                {
                    result.width++;
                    if (g_unichar_iswide (uni))
                        result.width++;
                }
            }
            else
            {
                actual[0] = '.';
                actual++;
                result.width++;
            }
            text = g_utf8_next_char (text);
        }
        else
        {
            text++;
            /*actual[0] = '?'; */
            memcpy (actual, replch, strlen (replch));
            actual += strlen (replch);
            result.width++;
        }
        if (length != (size_t) (-1))
            length--;
    }
    actual[0] = '\0';

    return &result;
}

static const char *
str_utf8_term_form (const char *text)
{
    static char result[BUF_MEDIUM * 6];
    const struct term_form *pre_form;
    char *composed;

    pre_form = str_utf8_make_make_term_form (text, (size_t) (-1));
    if (pre_form->compose)
    {
        composed = g_utf8_normalize (pre_form->text, -1, G_NORMALIZE_DEFAULT_COMPOSE);
        g_strlcpy (result, composed, sizeof (result));
        g_free (composed);
    }
    else
    {
        g_strlcpy (result, pre_form->text, sizeof (result));
    }
    return result;
}

struct utf8_tool
{
    char *actual;
    size_t remain;
    const char *cheked;
    int ident;
    gboolean compose;
};

/* utiliti function, that copy all characters from cheked to actual */
static gboolean
utf8_tool_copy_chars_to_end (struct utf8_tool *tool)
{
    size_t left;
    gunichar uni;

    tool->compose = FALSE;

    while (tool->cheked[0] != '\0')
    {
        uni = g_utf8_get_char (tool->cheked);
        tool->compose = tool->compose || str_unichar_iscombiningmark (uni);
        left = g_unichar_to_utf8 (uni, NULL);
        if (tool->remain <= left)
            return FALSE;
        left = g_unichar_to_utf8 (uni, tool->actual);
        tool->actual += left;
        tool->remain -= left;
        tool->cheked = g_utf8_next_char (tool->cheked);
    }
    return TRUE;
}

/* utiliti function, that copy characters from cheked to actual until ident is
 * smaller than to_ident */
static gboolean
utf8_tool_copy_chars_to (struct utf8_tool *tool, int to_ident)
{
    size_t left;
    gunichar uni;
    int w;

    tool->compose = FALSE;

    while (tool->cheked[0] != '\0')
    {
        uni = g_utf8_get_char (tool->cheked);
        if (!str_unichar_iscombiningmark (uni))
        {
            w = 1;
            if (g_unichar_iswide (uni))
                w++;
            if (tool->ident + w > to_ident)
                return TRUE;
        }
        else
        {
            w = 0;
            tool->compose = TRUE;
        }

        left = g_unichar_to_utf8 (uni, NULL);
        if (tool->remain <= left)
            return FALSE;
        left = g_unichar_to_utf8 (uni, tool->actual);
        tool->actual += left;
        tool->remain -= left;
        tool->cheked = g_utf8_next_char (tool->cheked);
        tool->ident += w;
    }
    return TRUE;
}

/* utiliti function, add count spaces to actual */
static int
utf8_tool_insert_space (struct utf8_tool *tool, int count)
{
    if (count <= 0)
        return 1;
    if (tool->remain <= (gsize) count)
        return 0;
    memset (tool->actual, ' ', count);
    tool->actual += count;
    tool->remain -= count;
    return 1;
}

/* utiliti function, add one characters to actual */
static int
utf8_tool_insert_char (struct utf8_tool *tool, char ch)
{
    if (tool->remain <= 1)
        return 0;
    tool->actual[0] = ch;
    tool->actual++;
    tool->remain--;
    return 1;
}

/* utiliti function, thah skip characters from cheked until ident is greater or
 * equal to to_ident */
static gboolean
utf8_tool_skip_chars_to (struct utf8_tool *tool, int to_ident)
{
    gunichar uni;

    while (to_ident > tool->ident && tool->cheked[0] != '\0')
    {
        uni = g_utf8_get_char (tool->cheked);
        if (!str_unichar_iscombiningmark (uni))
        {
            tool->ident++;
            if (g_unichar_iswide (uni))
                tool->ident++;
        }
        tool->cheked = g_utf8_next_char (tool->cheked);
    }
    uni = g_utf8_get_char (tool->cheked);
    while (str_unichar_iscombiningmark (uni))
    {
        tool->cheked = g_utf8_next_char (tool->cheked);
        uni = g_utf8_get_char (tool->cheked);
    }
    return TRUE;
}

static void
utf8_tool_compose (char *buffer, size_t size)
{
    char *composed = g_utf8_normalize (buffer, -1, G_NORMALIZE_DEFAULT_COMPOSE);
    g_strlcpy (buffer, composed, size);
    g_free (composed);
}


static const char *
str_utf8_fit_to_term (const char *text, int width, align_crt_t just_mode)
{
    static char result[BUF_MEDIUM * 6];
    const struct term_form *pre_form;
    struct utf8_tool tool;

    pre_form = str_utf8_make_make_term_form (text, (size_t) (-1));
    tool.cheked = pre_form->text;
    tool.actual = result;
    tool.remain = sizeof (result);
    tool.compose = FALSE;

    if (pre_form->width <= (gsize) width)
    {
        tool.ident = 0;
        switch (HIDE_FIT (just_mode))
        {
        case J_CENTER_LEFT:
        case J_CENTER:
            tool.ident = (width - pre_form->width) / 2;
            break;
        case J_RIGHT:
            tool.ident = width - pre_form->width;
            break;
        }

        utf8_tool_insert_space (&tool, tool.ident);
        utf8_tool_copy_chars_to_end (&tool);
        utf8_tool_insert_space (&tool, width - pre_form->width - tool.ident);
    }
    else
    {
        if (IS_FIT (just_mode))
        {
            tool.ident = 0;
            utf8_tool_copy_chars_to (&tool, width / 2);
            utf8_tool_insert_char (&tool, '~');

            tool.ident = 0;
            utf8_tool_skip_chars_to (&tool, pre_form->width - width + 1);
            utf8_tool_copy_chars_to_end (&tool);
            utf8_tool_insert_space (&tool, width - (pre_form->width - tool.ident + 1));
        }
        else
        {
            tool.ident = 0;
            switch (HIDE_FIT (just_mode))
            {
            case J_CENTER:
                tool.ident = (width - pre_form->width) / 2;
                break;
            case J_RIGHT:
                tool.ident = width - pre_form->width;
                break;
            }

            utf8_tool_skip_chars_to (&tool, 0);
            utf8_tool_insert_space (&tool, tool.ident);
            utf8_tool_copy_chars_to (&tool, width);
            utf8_tool_insert_space (&tool, width - tool.ident);
        }
    }

    tool.actual[0] = '\0';
    if (tool.compose)
        utf8_tool_compose (result, sizeof (result));
    return result;
}

static const char *
str_utf8_term_trim (const char *text, int width)
{
    static char result[BUF_MEDIUM * 6];
    const struct term_form *pre_form;
    struct utf8_tool tool;

    if (width < 1)
    {
        result[0] = '\0';
        return result;
    }

    pre_form = str_utf8_make_make_term_form (text, (size_t) (-1));

    tool.cheked = pre_form->text;
    tool.actual = result;
    tool.remain = sizeof (result);
    tool.compose = FALSE;

    if ((gsize) width < pre_form->width)
    {
        if (width <= 3)
        {
            memset (tool.actual, '.', width);
            tool.actual += width;
            tool.remain -= width;
        }
        else
        {
            memset (tool.actual, '.', 3);
            tool.actual += 3;
            tool.remain -= 3;

            tool.ident = 0;
            utf8_tool_skip_chars_to (&tool, pre_form->width - width + 3);
            utf8_tool_copy_chars_to_end (&tool);
        }
    }
    else
    {
        utf8_tool_copy_chars_to_end (&tool);
    }

    tool.actual[0] = '\0';
    if (tool.compose)
        utf8_tool_compose (result, sizeof (result));
    return result;
}

static int
str_utf8_term_width2 (const char *text, size_t length)
{
    const struct term_form *result;

    result = str_utf8_make_make_term_form (text, length);
    return result->width;
}

static int
str_utf8_term_width1 (const char *text)
{
    return str_utf8_term_width2 (text, (size_t) (-1));
}

static int
str_utf8_term_char_width (const char *text)
{
    gunichar uni = g_utf8_get_char_validated (text, -1);
    return (str_unichar_iscombiningmark (uni)) ? 0 : ((g_unichar_iswide (uni)) ? 2 : 1);
}

static const char *
str_utf8_term_substring (const char *text, int start, int width)
{
    static char result[BUF_MEDIUM * 6];
    const struct term_form *pre_form;
    struct utf8_tool tool;

    pre_form = str_utf8_make_make_term_form (text, (size_t) (-1));

    tool.cheked = pre_form->text;
    tool.actual = result;
    tool.remain = sizeof (result);
    tool.compose = FALSE;

    tool.ident = -start;
    utf8_tool_skip_chars_to (&tool, 0);
    if (tool.ident < 0)
        tool.ident = 0;
    utf8_tool_insert_space (&tool, tool.ident);

    utf8_tool_copy_chars_to (&tool, width);
    utf8_tool_insert_space (&tool, width - tool.ident);

    tool.actual[0] = '\0';
    if (tool.compose)
        utf8_tool_compose (result, sizeof (result));
    return result;
}

static const char *
str_utf8_trunc (const char *text, int width)
{
    static char result[MC_MAXPATHLEN * 6 * 2];
    const struct term_form *pre_form;
    struct utf8_tool tool;

    pre_form = str_utf8_make_make_term_form (text, (size_t) (-1));

    tool.cheked = pre_form->text;
    tool.actual = result;
    tool.remain = sizeof (result);
    tool.compose = FALSE;

    if (pre_form->width > (gsize) width)
    {
        tool.ident = 0;
        utf8_tool_copy_chars_to (&tool, width / 2);
        utf8_tool_insert_char (&tool, '~');

        tool.ident = 0;
        utf8_tool_skip_chars_to (&tool, pre_form->width - width + 1);
        utf8_tool_copy_chars_to_end (&tool);
    }
    else
    {
        utf8_tool_copy_chars_to_end (&tool);
    }

    tool.actual[0] = '\0';
    if (tool.compose)
        utf8_tool_compose (result, sizeof (result));
    return result;
}

static int
str_utf8_offset_to_pos (const char *text, size_t length)
{
    if (str_utf8_is_valid_string (text))
        return g_utf8_offset_to_pointer (text, length) - text;
    else
    {
        int result;
        GString *buffer = g_string_new (text);

        str_utf8_fix_string (buffer->str);
        result = g_utf8_offset_to_pointer (buffer->str, length) - buffer->str;
        g_string_free (buffer, TRUE);
        return result;
    }
}

static int
str_utf8_column_to_pos (const char *text, size_t pos)
{
    static int result;
    gunichar uni;
    int width;

    width = 0;
    result = 0;

    while (text[0] != '\0')
    {
        uni = g_utf8_get_char_validated (text, 6);
        if ((uni != (gunichar) (-1)) && (uni != (gunichar) (-2)))
        {
            if (g_unichar_isprint (uni))
            {
                if (!str_unichar_iscombiningmark (uni))
                {
                    width++;
                    if (g_unichar_iswide (uni))
                        width++;
                }
            }
            else
            {
                width++;
            }
            text = g_utf8_next_char (text);
        }
        else
        {
            text++;
            width++;
        }
        if ((gsize) width > pos)
            return result;

        result++;
    }

    return result;
}

static char *
str_utf8_create_search_needle (const char *needle, int case_sen)
{
    if (needle != NULL)
    {
        if (case_sen)
        {
            return g_utf8_normalize (needle, -1, G_NORMALIZE_ALL);
        }
        else
        {
            char *fold = g_utf8_casefold (needle, -1);
            char *result = g_utf8_normalize (fold, -1, G_NORMALIZE_ALL);
            g_free (fold);
            return result;
        }
    }
    else
        return NULL;
}

static void
str_utf8_release_search_needle (char *needle, int case_sen)
{
    (void) case_sen;
    if (needle != NULL)
        g_free (needle);
}

static const char *
str_utf8_search_first (const char *text, const char *search, int case_sen)
{
    char *fold_text;
    char *deco_text;
    const char *match;
    const char *result = NULL;
    const char *m;

    fold_text = (case_sen) ? (char *) text : g_utf8_casefold (text, -1);
    deco_text = g_utf8_normalize (fold_text, -1, G_NORMALIZE_ALL);

    match = deco_text;
    do
    {
        match = g_strstr_len (match, -1, search);
        if (match != NULL)
        {
            if ((!str_utf8_iscombiningmark (match) || (match == deco_text)) &&
                !str_utf8_iscombiningmark (match + strlen (search)))
            {

                result = text;
                m = deco_text;
                while (m < match)
                {
                    str_utf8_cnext_noncomb_char (&m);
                    str_utf8_cnext_noncomb_char (&result);
                }
            }
            else
            {
                str_utf8_cnext_char (&match);
            }
        }
    }
    while (match != NULL && result == NULL);

    g_free (deco_text);
    if (!case_sen)
        g_free (fold_text);

    return result;
}

static const char *
str_utf8_search_last (const char *text, const char *search, int case_sen)
{
    char *fold_text;
    char *deco_text;
    char *match;
    const char *result = NULL;
    const char *m;

    fold_text = (case_sen) ? (char *) text : g_utf8_casefold (text, -1);
    deco_text = g_utf8_normalize (fold_text, -1, G_NORMALIZE_ALL);

    do
    {
        match = g_strrstr_len (deco_text, -1, search);
        if (match != NULL)
        {
            if ((!str_utf8_iscombiningmark (match) || (match == deco_text)) &&
                !str_utf8_iscombiningmark (match + strlen (search)))
            {

                result = text;
                m = deco_text;
                while (m < match)
                {
                    str_utf8_cnext_noncomb_char (&m);
                    str_utf8_cnext_noncomb_char (&result);
                }
            }
            else
            {
                match[0] = '\0';
            }
        }
    }
    while (match != NULL && result == NULL);

    g_free (deco_text);
    if (!case_sen)
        g_free (fold_text);

    return result;
}

static char *
str_utf8_normalize (const char *text)
{
    GString *fixed;
    char *tmp;
    char *result;
    const char *start;
    const char *end;

    fixed = g_string_sized_new (4);

    start = text;
    while (!g_utf8_validate (start, -1, &end) && start[0] != '\0')
    {
        if (start != end)
        {
            tmp = g_utf8_normalize (start, end - start, G_NORMALIZE_ALL);
            g_string_append (fixed, tmp);
            g_free (tmp);
        }
        g_string_append_c (fixed, end[0]);
        start = end + 1;
    }

    if (start == text)
    {
        result = g_utf8_normalize (text, -1, G_NORMALIZE_ALL);
        g_string_free (fixed, TRUE);
    }
    else
    {
        if (start[0] != '\0' && start != end)
        {
            tmp = g_utf8_normalize (start, end - start, G_NORMALIZE_ALL);
            g_string_append (fixed, tmp);
            g_free (tmp);
        }
        result = g_string_free (fixed, FALSE);
    }

    return result;
}

static char *
str_utf8_casefold_normalize (const char *text)
{
    GString *fixed;
    char *tmp, *fold;
    char *result;
    const char *start;
    const char *end;

    fixed = g_string_sized_new (4);

    start = text;
    while (!g_utf8_validate (start, -1, &end) && start[0] != '\0')
    {
        if (start != end)
        {
            fold = g_utf8_casefold (start, end - start);
            tmp = g_utf8_normalize (fold, -1, G_NORMALIZE_ALL);
            g_string_append (fixed, tmp);
            g_free (tmp);
            g_free (fold);
        }
        g_string_append_c (fixed, end[0]);
        start = end + 1;
    }

    if (start == text)
    {
        fold = g_utf8_casefold (text, -1);
        result = g_utf8_normalize (fold, -1, G_NORMALIZE_ALL);
        g_free (fold);
        g_string_free (fixed, TRUE);
    }
    else
    {
        if (start[0] != '\0' && start != end)
        {
            fold = g_utf8_casefold (start, end - start);
            tmp = g_utf8_normalize (fold, -1, G_NORMALIZE_ALL);
            g_string_append (fixed, tmp);
            g_free (tmp);
            g_free (fold);
        }
        result = g_string_free (fixed, FALSE);
    }

    return result;
}

static int
str_utf8_compare (const char *t1, const char *t2)
{
    char *n1, *n2;
    int result;

    n1 = str_utf8_normalize (t1);
    n2 = str_utf8_normalize (t2);

    result = strcmp (n1, n2);

    g_free (n1);
    g_free (n2);

    return result;
}

static int
str_utf8_ncompare (const char *t1, const char *t2)
{
    char *n1, *n2;
    int result;

    n1 = str_utf8_normalize (t1);
    n2 = str_utf8_normalize (t2);

    result = strncmp (n1, n2, min (strlen (n1), strlen (n2)));

    g_free (n1);
    g_free (n2);

    return result;
}

static int
str_utf8_casecmp (const char *t1, const char *t2)
{
    char *n1, *n2;
    int result;

    n1 = str_utf8_casefold_normalize (t1);
    n2 = str_utf8_casefold_normalize (t2);

    result = strcmp (n1, n2);

    g_free (n1);
    g_free (n2);

    return result;
}

static int
str_utf8_ncasecmp (const char *t1, const char *t2)
{
    char *n1, *n2;
    int result;

    n1 = str_utf8_casefold_normalize (t1);
    n2 = str_utf8_casefold_normalize (t2);

    result = strncmp (n1, n2, min (strlen (n1), strlen (n2)));

    g_free (n1);
    g_free (n2);

    return result;
}

static int
str_utf8_prefix (const char *text, const char *prefix)
{
    char *t = str_utf8_normalize (text);
    char *p = str_utf8_normalize (prefix);
    const char *nt = t;
    const char *np = p;
    const char *nnt = t;
    const char *nnp = p;
    int result;

    while (nt[0] != '\0' && np[0] != '\0')
    {
        str_utf8_cnext_char_safe (&nnt);
        str_utf8_cnext_char_safe (&nnp);
        if (nnt - nt != nnp - np)
            break;
        if (strncmp (nt, np, nnt - nt) != 0)
            break;
        nt = nnt;
        np = nnp;
    }

    result = np - p;

    g_free (t);
    g_free (p);

    return result;
}

static int
str_utf8_caseprefix (const char *text, const char *prefix)
{
    char *t = str_utf8_casefold_normalize (text);
    char *p = str_utf8_casefold_normalize (prefix);
    const char *nt = t;
    const char *np = p;
    const char *nnt = t;
    const char *nnp = p;
    int result;

    while (nt[0] != '\0' && np[0] != '\0')
    {
        str_utf8_cnext_char_safe (&nnt);
        str_utf8_cnext_char_safe (&nnp);
        if (nnt - nt != nnp - np)
            break;
        if (strncmp (nt, np, nnt - nt) != 0)
            break;
        nt = nnt;
        np = nnp;
    }

    result = np - p;

    g_free (t);
    g_free (p);

    return result;
}

static char *
str_utf8_create_key_gen (const char *text, int case_sen,
                         gchar * (*keygen) (const gchar * text, gssize size))
{
    char *result;

    if (case_sen)
    {
        result = str_utf8_normalize (text);
    }
    else
    {
        gboolean dot;
        GString *fixed;
        const char *start, *end;
        char *fold, *key;

        dot = text[0] == '.';
        fixed = g_string_sized_new (16);

        if (!dot)
            start = text;
        else
        {
            start = text + 1;
            g_string_append_c (fixed, '.');
        }

        while (!g_utf8_validate (start, -1, &end) && start[0] != '\0')
        {
            if (start != end)
            {
                fold = g_utf8_casefold (start, end - start);
                key = keygen (fold, -1);
                g_string_append (fixed, key);
                g_free (key);
                g_free (fold);
            }
            g_string_append_c (fixed, end[0]);
            start = end + 1;
        }

        if (start == text)
        {
            fold = g_utf8_casefold (start, -1);
            result = keygen (fold, -1);
            g_free (fold);
            g_string_free (fixed, TRUE);
        }
        else if (dot && (start == text + 1))
        {
            fold = g_utf8_casefold (start, -1);
            key = keygen (fold, -1);
            g_string_append (fixed, key);
            g_free (key);
            g_free (fold);
            result = g_string_free (fixed, FALSE);
        }
        else
        {
            if (start[0] != '\0' && start != end)
            {
                fold = g_utf8_casefold (start, end - start);
                key = keygen (fold, -1);
                g_string_append (fixed, key);
                g_free (key);
                g_free (fold);
            }
            result = g_string_free (fixed, FALSE);
        }
    }
    return result;
}

static char *
str_utf8_create_key (const char *text, int case_sen)
{
    return str_utf8_create_key_gen (text, case_sen, g_utf8_collate_key);
}

#ifdef MC__USE_STR_UTF8_CREATE_KEY_FOR_FILENAME
static char *
str_utf8_create_key_for_filename (const char *text, int case_sen)
{
    return str_utf8_create_key_gen (text, case_sen, g_utf8_collate_key_for_filename);
}
#endif

static int
str_utf8_key_collate (const char *t1, const char *t2, int case_sen)
{
    (void) case_sen;
    return strcmp (t1, t2);
}

static void
str_utf8_release_key (char *key, int case_sen)
{
    (void) case_sen;
    g_free (key);
}

struct str_class
str_utf8_init (void)
{
    struct str_class result;

    result.conv_gerror_message = str_utf8_conv_gerror_message;
    result.vfs_convert_to = str_utf8_vfs_convert_to;
    result.insert_replace_char = str_utf8_insert_replace_char;
    result.is_valid_string = str_utf8_is_valid_string;
    result.is_valid_char = str_utf8_is_valid_char;
    result.cnext_char = str_utf8_cnext_char;
    result.cprev_char = str_utf8_cprev_char;
    result.cnext_char_safe = str_utf8_cnext_char_safe;
    result.cprev_char_safe = str_utf8_cprev_char_safe;
    result.cnext_noncomb_char = str_utf8_cnext_noncomb_char;
    result.cprev_noncomb_char = str_utf8_cprev_noncomb_char;
    result.char_isspace = str_utf8_isspace;
    result.char_ispunct = str_utf8_ispunct;
    result.char_isalnum = str_utf8_isalnum;
    result.char_isdigit = str_utf8_isdigit;
    result.char_isprint = str_utf8_isprint;
    result.char_iscombiningmark = str_utf8_iscombiningmark;
    result.char_toupper = str_utf8_toupper;
    result.char_tolower = str_utf8_tolower;
    result.length = str_utf8_length;
    result.length2 = str_utf8_length2;
    result.length_noncomb = str_utf8_length_noncomb;
    result.fix_string = str_utf8_fix_string;
    result.term_form = str_utf8_term_form;
    result.fit_to_term = str_utf8_fit_to_term;
    result.term_trim = str_utf8_term_trim;
    result.term_width2 = str_utf8_term_width2;
    result.term_width1 = str_utf8_term_width1;
    result.term_char_width = str_utf8_term_char_width;
    result.term_substring = str_utf8_term_substring;
    result.trunc = str_utf8_trunc;
    result.offset_to_pos = str_utf8_offset_to_pos;
    result.column_to_pos = str_utf8_column_to_pos;
    result.create_search_needle = str_utf8_create_search_needle;
    result.release_search_needle = str_utf8_release_search_needle;
    result.search_first = str_utf8_search_first;
    result.search_last = str_utf8_search_last;
    result.compare = str_utf8_compare;
    result.ncompare = str_utf8_ncompare;
    result.casecmp = str_utf8_casecmp;
    result.ncasecmp = str_utf8_ncasecmp;
    result.prefix = str_utf8_prefix;
    result.caseprefix = str_utf8_caseprefix;
    result.create_key = str_utf8_create_key;
#ifdef MC__USE_STR_UTF8_CREATE_KEY_FOR_FILENAME
    /* case insensitive sort files in "a1 a2 a10" order */
    result.create_key_for_filename = str_utf8_create_key_for_filename;
#else
    /* case insensitive sort files in "a1 a10 a2" order */
    result.create_key_for_filename = str_utf8_create_key;
#endif
    result.key_collate = str_utf8_key_collate;
    result.release_key = str_utf8_release_key;

    return result;
}

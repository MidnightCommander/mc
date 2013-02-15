/*
   Word-processor mode for the editor: does dynamic
   paragraph formatting.

   Copyright (C) 2011, 2013
   The Free Software Foundation, Inc.

   Copyright (C) 1996 Paul Sheer

   Writen by:
   Paul Sheer, 1996
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

/** \file
 *  \brief Source: word-processor mode for the editor: does dynamic paragraph formatting
 *  \author Paul Sheer
 *  \date 1996
 *  \author Andrew Borodin
 *  \date 2013
 */

#include <config.h>

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#include <stdlib.h>

#include "lib/global.h"

#include "src/setup.h"          /* option_tab_spacing */

#include "edit-impl.h"
#include "editwidget.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define tab_width option_tab_spacing

#define NO_FORMAT_CHARS_START "-+*\\,.;:&>"
#define FONT_MEAN_WIDTH 1

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static long
line_start (WEdit * edit, long line)
{
    off_t p;
    long l;

    l = edit->curs_line;
    p = edit->buffer.curs1;

    if (line < l)
        p = edit_move_backward (edit, p, l - line);
    else if (line > l)
        p = edit_move_forward (edit, p, line - l, 0);

    p = edit_bol (edit, p);
    while (strchr ("\t ", edit_buffer_get_byte (&edit->buffer, p)))
        p++;
    return p;
}

/* --------------------------------------------------------------------------------------------- */

static int
bad_line_start (WEdit * edit, off_t p)
{
    int c;

    c = edit_buffer_get_byte (&edit->buffer, p);
    if (c == '.')
    {                           /* `...' is acceptable */
        if (edit_buffer_get_byte (&edit->buffer, p + 1) == '.'
            && edit_buffer_get_byte (&edit->buffer, p + 2) == '.')
            return 0;
        return 1;
    }
    if (c == '-')
    {
        if (edit_buffer_get_byte (&edit->buffer, p + 1) == '-'
            && edit_buffer_get_byte (&edit->buffer, p + 2) == '-')
            return 0;       /* `---' is acceptable */
        return 1;
    }
    if (strchr (NO_FORMAT_CHARS_START, c))
        return 1;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Find the start of the current paragraph for the purpose of formatting.
 * Return position in the file.
 */

static long
begin_paragraph (WEdit * edit, int force)
{
    long i;
    for (i = edit->curs_line - 1; i >= 0; i--)
    {
        if (edit_line_is_blank (edit, i))
        {
            i++;
            break;
        }
        if (force)
        {
            if (bad_line_start (edit, line_start (edit, i)))
            {
                i++;
                break;
            }
        }
    }
    return edit_move_backward (edit, edit_bol (edit, edit->buffer.curs1), edit->curs_line - i);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Find the end of the current paragraph for the purpose of formatting.
 * Return position in the file.
 */

static long
end_paragraph (WEdit * edit, int force)
{
    long i;
    for (i = edit->curs_line + 1; i <= edit->total_lines; i++)
    {
        if (edit_line_is_blank (edit, i))
        {
            i--;
            break;
        }
        if (force)
            if (bad_line_start (edit, line_start (edit, i)))
            {
                i--;
                break;
            }
    }
    return edit_eol (edit,
                     edit_move_forward (edit, edit_bol (edit, edit->buffer.curs1),
                                        i - edit->curs_line, 0));
}

/* --------------------------------------------------------------------------------------------- */

static unsigned char *
get_paragraph (WEdit * edit, off_t p, off_t q, int indent, int *size)
{
    unsigned char *s, *t;
#if 0
    t = g_try_malloc ((q - p) + 2 * (q - p) / option_word_wrap_line_length + 10);
#else
    t = g_try_malloc (2 * (q - p) + 100);
#endif
    if (t == NULL)
        return NULL;
    for (s = t; p < q; p++, s++)
    {
        if (indent && edit_buffer_get_byte (&edit->buffer, p - 1) == '\n')
            while (strchr ("\t ", edit_buffer_get_byte (&edit->buffer, p)))
                p++;
        *s = edit_buffer_get_byte (&edit->buffer, p);
    }
    *size = (unsigned long) (s - t);
    /* FIXME: all variables related to 'size' should be fixed */
    t[*size] = '\n';
    return t;
}

/* --------------------------------------------------------------------------------------------- */

static inline void
strip_newlines (unsigned char *t, int size)
{
    unsigned char *p = t;
    while (size-- != 0)
    {
        if (*p == '\n')
            *p = ' ';
        p++;
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
   This function calculates the number of chars in a line specified to length l in pixels
 */

static inline int
next_tab_pos (int x)
{
    return x += tab_width - x % tab_width;
}

/* --------------------------------------------------------------------------------------------- */

static inline int
line_pixel_length (unsigned char *t, long b, int l)
{
    int x = 0, c, xn = 0;

    while (TRUE)
    {
        c = t[b];
        switch (c)
        {
        case '\n':
            return b;
        case '\t':
            xn = next_tab_pos (x);
            break;
        default:
            xn = x + 1;
            break;
        }
        if (xn > l)
            break;
        x = xn;
        b++;
    }
    return b;
}

/* --------------------------------------------------------------------------------------------- */

static int
next_word_start (unsigned char *t, int q, int size)
{
    int i;
    int saw_ws = 0;

    for (i = q; i < size; i++)
    {
        switch (t[i])
        {
        case '\n':
            return -1;
        case '\t':
        case ' ':
            saw_ws = 1;
            break;
        default:
            if (saw_ws != 0)
                return i;
            break;
        }
    }
    return -1;
}

/* --------------------------------------------------------------------------------------------- */
/** find the start of a word */

static inline int
word_start (unsigned char *t, int q, int size)
{
    int i = q;

    if (t[q] == ' ' || t[q] == '\t')
        return next_word_start (t, q, size);

    while (TRUE)
    {
        int c;

        if (i == 0)
            return -1;
        c = t[i - 1];
        if (c == '\n')
            return -1;
        if (c == ' ' || c == '\t')
            return i;
        i--;
    }
}

/* --------------------------------------------------------------------------------------------- */
/** replaces ' ' with '\n' to properly format a paragraph */

static inline void
format_this (unsigned char *t, int size, int indent)
{
    int q = 0, ww;

    strip_newlines (t, size);
    ww = option_word_wrap_line_length * FONT_MEAN_WIDTH - indent;
    if (ww < FONT_MEAN_WIDTH * 2)
        ww = FONT_MEAN_WIDTH * 2;
    while (TRUE)
    {
        int p;

        q = line_pixel_length (t, q, ww);
        if (q > size)
            break;
        if (t[q] == '\n')
            break;
        p = word_start (t, q, size);
        if (p == -1)
            q = next_word_start (t, q, size);   /* Return the end of the word if the beginning
                                                   of the word is at the beginning of a line
                                                   (i.e. a very long word) */
        else
            q = p;
        if (q == -1)            /* end of paragraph */
            break;
        if (q != 0)
            t[q - 1] = '\n';
    }
}

/* --------------------------------------------------------------------------------------------- */

static inline void
replace_at (WEdit * edit, long q, int c)
{
    edit_cursor_move (edit, q - edit->buffer.curs1);
    edit_delete (edit, TRUE);
    edit_insert_ahead (edit, c);
}

/* --------------------------------------------------------------------------------------------- */
/** replaces a block of text */

static inline void
put_paragraph (WEdit * edit, unsigned char *t, off_t p, int indent, int size)
{
    long cursor;
    int i, c = 0;

    cursor = edit->buffer.curs1;
    if (indent)
        while (strchr ("\t ", edit_buffer_get_byte (&edit->buffer, p)))
            p++;
    for (i = 0; i < size; i++, p++)
    {
        if (i && indent)
        {
            if (t[i - 1] == '\n' && c == '\n')
            {
                while (strchr ("\t ", edit_buffer_get_byte (&edit->buffer, p)))
                    p++;
            }
            else if (t[i - 1] == '\n')
            {
                off_t curs;
                edit_cursor_move (edit, p - edit->buffer.curs1);
                curs = edit->buffer.curs1;
                edit_insert_indent (edit, indent);
                if (cursor >= curs)
                    cursor += edit->buffer.curs1 - p;
                p = edit->buffer.curs1;
            }
            else if (c == '\n')
            {
                edit_cursor_move (edit, p - edit->buffer.curs1);
                while (strchr ("\t ", edit_buffer_get_byte (&edit->buffer, p)))
                {
                    edit_delete (edit, TRUE);
                    if (cursor > edit->buffer.curs1)
                        cursor--;
                }
                p = edit->buffer.curs1;
            }
        }
        c = edit_buffer_get_byte (&edit->buffer, p);
        if (c != t[i])
            replace_at (edit, p, t[i]);
    }
    edit_cursor_move (edit, cursor - edit->buffer.curs1);      /* restore cursor position */
}

/* --------------------------------------------------------------------------------------------- */

static inline int
test_indent (const WEdit * edit, off_t p, off_t q)
{
    int indent;

    indent = edit_indent_width (edit, p++);
    if (indent == 0)
        return 0;

    for (; p < q; p++)
        if (edit_buffer_get_byte (&edit->buffer, p - 1) == '\n'
            && indent != edit_indent_width (edit, p))
            return 0;
    return indent;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
format_paragraph (WEdit * edit, int force)
{
    long p, q;
    int size;
    unsigned char *t;
    int indent = 0;
    if (option_word_wrap_line_length < 2)
        return;
    if (edit_line_is_blank (edit, edit->curs_line))
        return;
    p = begin_paragraph (edit, force);
    q = end_paragraph (edit, force);
    indent = test_indent (edit, p, q);
    t = get_paragraph (edit, p, q, indent, &size);
    if (!t)
        return;
    if (!force)
    {
        int i;
        if (strchr (NO_FORMAT_CHARS_START, *t))
        {
            g_free (t);
            return;
        }
        for (i = 0; i < size - 1; i++)
        {
            if (t[i] == '\n')
            {
                if (strchr (NO_FORMAT_CHARS_START "\t ", t[i + 1]))
                {
                    g_free (t);
                    return;
                }
            }
        }
    }
    format_this (t, q - p, indent);
    put_paragraph (edit, t, p, indent, size);
    g_free (t);

    /* Scroll left as much as possible to show the formatted paragraph */
    edit_scroll_left (edit, -edit->start_col);
}

/* --------------------------------------------------------------------------------------------- */

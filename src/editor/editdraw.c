/*
   Editor text drawing.

   Copyright (C) 1996, 1997, 1998, 2001, 2002, 2003, 2004, 2005, 2006,
   2007, 2011
   The Free Software Foundation, Inc.

   Written by:
   Paul Sheer, 1996, 1997

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
 *  \brief Source: editor text drawing
 *  \author Paul Sheer
 *  \date 1996, 1997
 */

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#include "lib/global.h"
#include "lib/tty/tty.h"        /* tty_printf() */
#include "lib/tty/key.h"        /* is_idle() */
#include "lib/skin.h"
#include "lib/strutil.h"        /* utf string functions */
#include "lib/util.h"           /* is_printable() */
#include "lib/widget.h"         /* buttonbar_redraw() */
#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "src/setup.h"          /* edit_tab_spacing */
#include "src/main.h"           /* macro_index */

#include "edit-impl.h"
#include "editwidget.h"

/*** global variables ****************************************************************************/

/* Toggles statusbar draw style */
int simple_statusbar = 0;

int visible_tabs = 1, visible_tws = 1;

/*** file scope macro definitions ****************************************************************/

#define MAX_LINE_LEN 1024

/* Text styles */
#define MOD_ABNORMAL            (1 << 8)
#define MOD_BOLD                (1 << 9)
#define MOD_MARKED              (1 << 10)
#define MOD_CURSOR              (1 << 11)
#define MOD_WHITESPACE          (1 << 12)

#define edit_move(x,y) widget_move(edit, y, x);

#define key_pending(x) (!is_idle())

#define EDITOR_MINIMUM_TERMINAL_WIDTH 30

/*** file scope type declarations ****************************************************************/

struct line_s
{
    unsigned int ch;
    unsigned int style;
};

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static inline void
status_string (WEdit * edit, char *s, int w)
{
    char byte_str[16];

    /*
     * If we are at the end of file, print <EOF>,
     * otherwise print the current character as is (if printable),
     * as decimal and as hex.
     */
    if (edit->curs1 < edit->last_byte)
    {
        unsigned char cur_byte = 0;

#ifdef HAVE_CHARSET
        int cw = 1;

        if (!edit->utf8)
        {
            cur_byte = edit_get_byte (edit, edit->curs1);

            g_snprintf (byte_str, sizeof (byte_str), "%4d 0x%03X",
                        (int) cur_byte, (unsigned) cur_byte);
        }
        else
        {
            unsigned int cur_utf = 0;

            cur_utf = edit_get_utf (edit, edit->curs1, &cw);
            if (cw > 0)
            {
                g_snprintf (byte_str, sizeof (byte_str), "%04d 0x%03X",
                            (unsigned) cur_utf, (unsigned) cur_utf);
            }
            else
#endif
            {
                cur_byte = edit_get_byte (edit, edit->curs1);
                g_snprintf (byte_str, sizeof (byte_str), "%04d 0x%03X",
                            (int) cur_byte, (unsigned) cur_byte);
            }
#ifdef HAVE_CHARSET
        }
#endif
    }
    else
    {
        strcpy (byte_str, "<EOF>     ");
    }

    /* The field lengths just prevent the status line from shortening too much */
    if (simple_statusbar)
        g_snprintf (s, w,
                    "%c%c%c%c %3ld %5ld/%ld %6ld/%ld %s %s",
                    edit->mark1 != edit->mark2 ? (edit->column_highlight ? 'C' : 'B') : '-',
                    edit->modified ? 'M' : '-',
                    macro_index < 0 ? '-' : 'R',
                    edit->overwrite == 0 ? '-' : 'O',
                    edit->curs_col + edit->over_col,
                    edit->curs_line + 1,
                    edit->total_lines + 1, (long) edit->curs1, (long) edit->last_byte, byte_str,
#ifdef HAVE_CHARSET
                    mc_global.source_codepage >= 0 ? get_codepage_id (mc_global.source_codepage) :
#endif
                    "");
    else
        g_snprintf (s, w,
                    "[%c%c%c%c] %2ld L:[%3ld+%2ld %3ld/%3ld] *(%-4ld/%4ldb) %s  %s",
                    edit->mark1 != edit->mark2 ? (edit->column_highlight ? 'C' : 'B') : '-',
                    edit->modified ? 'M' : '-',
                    macro_index < 0 ? '-' : 'R',
                    edit->overwrite == 0 ? '-' : 'O',
                    edit->curs_col + edit->over_col,
                    edit->start_line + 1,
                    edit->curs_row,
                    edit->curs_line + 1,
                    edit->total_lines + 1, (long) edit->curs1, (long) edit->last_byte, byte_str,
#ifdef HAVE_CHARSET
                    mc_global.source_codepage >= 0 ? get_codepage_id (mc_global.source_codepage) :
#endif
                    "");
}

/* --------------------------------------------------------------------------------------------- */

static inline void
printwstr (const char *s, int len)
{
    if (len > 0)
        tty_printf ("%-*.*s", len, len, s);
}

/* --------------------------------------------------------------------------------------------- */

static inline void
print_to_widget (WEdit * edit, long row, int start_col, int start_col_real,
                 long end_col, struct line_s line[], char *status, int bookmarked)
{
    struct line_s *p;

    int x = start_col_real;
    int x1 = start_col + EDIT_TEXT_HORIZONTAL_OFFSET + option_line_state_width;
    int y = row + EDIT_TEXT_VERTICAL_OFFSET;
    int cols_to_skip = abs (x);
    int i;
    int wrap_start;
    int len;

    tty_setcolor (EDITOR_NORMAL_COLOR);
    if (bookmarked != 0)
        tty_setcolor (bookmarked);

    len = end_col + 1 - start_col;
    wrap_start = option_word_wrap_line_length + edit->start_col;

    if (len > 0 && edit->widget.y + y >= 0)
    {
        if (!show_right_margin || wrap_start > end_col)
            tty_draw_hline (edit->widget.y + y, edit->widget.x + x1, ' ', len);
        else if (wrap_start < 0)
        {
            tty_setcolor (EDITOR_RIGHT_MARGIN_COLOR);
            tty_draw_hline (edit->widget.y + y, edit->widget.x + x1, ' ', len);
        }
        else
        {
            if (wrap_start > 0)
                tty_draw_hline (edit->widget.y + y, edit->widget.x + x1, ' ', wrap_start);

            len -= wrap_start;
            if (len > 0)
            {
                tty_setcolor (EDITOR_RIGHT_MARGIN_COLOR);
                tty_draw_hline (edit->widget.y + y, edit->widget.x + x1 + wrap_start, ' ', len);
            }
        }
    }

    if (option_line_state)
    {
        tty_setcolor (LINE_STATE_COLOR);
        for (i = 0; i < LINE_STATE_WIDTH; i++)
        {
            edit_move (x1 + i - option_line_state_width, y);
            if (status[i] == '\0')
                status[i] = ' ';
            tty_print_char (status[i]);
        }
    }

    edit_move (x1, y);
    i = 1;
    for (p = line; p->ch != 0; p++)
    {
        int style;
        unsigned int textchar;
        int color;

        if (cols_to_skip != 0)
        {
            cols_to_skip--;
            continue;
        }

        style = p->style & 0xFF00;
        textchar = p->ch;
        color = p->style >> 16;

        if (style & MOD_ABNORMAL)
        {
            /* Non-printable - use black background */
            color = 0;
        }

        if (style & MOD_WHITESPACE)
        {
            if (style & MOD_MARKED)
            {
                textchar = ' ';
                tty_setcolor (EDITOR_MARKED_COLOR);
            }
            else
                tty_setcolor (EDITOR_WHITESPACE_COLOR);
        }
        else if (style & MOD_BOLD)
            tty_setcolor (EDITOR_BOLD_COLOR);
        else if (style & MOD_MARKED)
            tty_setcolor (EDITOR_MARKED_COLOR);
        else
            tty_lowlevel_setcolor (color);

        if (show_right_margin)
        {
            if (i > option_word_wrap_line_length + edit->start_col)
                tty_setcolor (EDITOR_RIGHT_MARGIN_COLOR);
            i++;
        }

        tty_print_anychar (textchar);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** b is a pointer to the beginning of the line */

static void
edit_draw_this_line (WEdit * edit, off_t b, long row, long start_col, long end_col)
{
    struct line_s line[MAX_LINE_LEN];
    struct line_s *p = line;

    off_t m1 = 0, m2 = 0, q;
    long c1, c2;
    int col, start_col_real;
    unsigned int c;
    int color;
    int abn_style;
    int i;
    unsigned int cur_line = 0;
    int book_mark = 0;
    char line_stat[LINE_STATE_WIDTH + 1] = "\0";

    if (row > edit->widget.lines - 1 - EDIT_TEXT_VERTICAL_OFFSET)
        return;

    if (book_mark_query_color (edit, edit->start_line + row, BOOK_MARK_COLOR))
        book_mark = BOOK_MARK_COLOR;
    else if (book_mark_query_color (edit, edit->start_line + row, BOOK_MARK_FOUND_COLOR))
        book_mark = BOOK_MARK_FOUND_COLOR;

    if (book_mark)
        abn_style = book_mark << 16;
    else
        abn_style = MOD_ABNORMAL;

    end_col -= EDIT_TEXT_HORIZONTAL_OFFSET + option_line_state_width;

    color = edit_get_syntax_color (edit, b - 1);
    q = edit_move_forward3 (edit, b, start_col - edit->start_col, 0);
    start_col_real = (col = (int) edit_move_forward3 (edit, b, 0, q)) + edit->start_col;

    if (option_line_state)
    {
        cur_line = edit->start_line + row;
        if (cur_line <= (unsigned int) edit->total_lines)
        {
            g_snprintf (line_stat, LINE_STATE_WIDTH + 1, "%7i ", cur_line + 1);
        }
        else
        {
            memset (line_stat, ' ', LINE_STATE_WIDTH);
            line_stat[LINE_STATE_WIDTH] = '\0';
        }
        if (book_mark_query_color (edit, cur_line, BOOK_MARK_COLOR))
        {
            g_snprintf (line_stat, 2, "*");
        }
    }

    if (col + 16 > -edit->start_col)
    {
        eval_marks (edit, &m1, &m2);

        if (row <= edit->total_lines - edit->start_line)
        {
            off_t tws = 0;
            if (tty_use_colors () && visible_tws)
            {
                tws = edit_eol (edit, b);
                while (tws > b && ((c = edit_get_byte (edit, tws - 1)) == ' ' || c == '\t'))
                    tws--;
            }

            while (col <= end_col - edit->start_col)
            {
                int cw = 1;
                int tab_over = 0;
                gboolean wide_width_char = FALSE;
                gboolean control_char = FALSE;

                p->ch = 0;
                p->style = 0;
                if (q == edit->curs1)
                    p->style |= MOD_CURSOR;
                if (q >= m1 && q < m2)
                {
                    if (edit->column_highlight)
                    {
                        long x;

                        x = (long) edit_move_forward3 (edit, b, 0, q);
                        c1 = min (edit->column1, edit->column2);
                        c2 = max (edit->column1, edit->column2);
                        if (x >= c1 && x < c2)
                            p->style |= MOD_MARKED;
                    }
                    else
                        p->style |= MOD_MARKED;
                }
                if (q == edit->bracket)
                    p->style |= MOD_BOLD;
                if (q >= edit->found_start && q < (off_t) (edit->found_start + edit->found_len))
                    p->style |= MOD_BOLD;

#ifdef HAVE_CHARSET
                if (edit->utf8)
                    c = edit_get_utf (edit, q, &cw);
                else
#endif
                    c = edit_get_byte (edit, q);
                /* we don't use bg for mc - fg contains both */
                if (book_mark)
                {
                    p->style |= book_mark << 16;
                }
                else
                {
                    color = edit_get_syntax_color (edit, q);
                    p->style |= color << 16;
                }
                switch (c)
                {
                case '\n':
                    col = end_col - edit->start_col + 1;        /* quit */
                    break;
                case '\t':
                    i = TAB_SIZE - ((int) col % TAB_SIZE);
                    tab_over = (end_col - edit->start_col) - (col + i - 1);
                    if (tab_over < 0)
                        i += tab_over;
                    col += i;
                    if (tty_use_colors () &&
                        ((visible_tabs || (visible_tws && q >= tws)) && enable_show_tabs_tws))
                    {
                        if (p->style & MOD_MARKED)
                            c = p->style;
                        else if (book_mark)
                            c |= book_mark << 16;
                        else
                            c = p->style | MOD_WHITESPACE;
                        if (i > 2)
                        {
                            p->ch = '<';
                            p->style = c;
                            p++;
                            while (--i > 1)
                            {
                                p->ch = '-';
                                p->style = c;
                                p++;
                            }
                            p->ch = '>';
                            p->style = c;
                            p++;
                        }
                        else if (i > 1)
                        {
                            p->ch = '<';
                            p->style = c;
                            p++;
                            p->ch = '>';
                            p->style = c;
                            p++;
                        }
                        else
                        {
                            p->ch = '>';
                            p->style = c;
                            p++;
                        }
                    }
                    else if (tty_use_colors () && visible_tws && q >= tws && enable_show_tabs_tws)
                    {
                        p->ch = '.';
                        p->style |= MOD_WHITESPACE;
                        c = p->style & ~MOD_CURSOR;
                        p++;
                        while (--i)
                        {
                            p->ch = ' ';
                            p->style = c;
                            p++;
                        }
                    }
                    else
                    {
                        p->ch |= ' ';
                        c = p->style & ~MOD_CURSOR;
                        p++;
                        while (--i)
                        {
                            p->ch = ' ';
                            p->style = c;
                            p++;
                        }
                    }
                    break;
                case ' ':
                    if (tty_use_colors () && visible_tws && q >= tws && enable_show_tabs_tws)
                    {
                        p->ch = '.';
                        p->style |= MOD_WHITESPACE;
                        p++;
                        col++;
                        break;
                    }
                    /* fallthrough */
                default:
#ifdef HAVE_CHARSET
                    if (mc_global.utf8_display)
                    {
                        if (!edit->utf8)
                        {
                            c = convert_from_8bit_to_utf_c ((unsigned char) c, edit->converter);
                        }
                        else
                        {
                            if (g_unichar_iswide (c))
                            {
                                wide_width_char = TRUE;
                                col++;
                            }
                        }
                    }
                    else if (edit->utf8)
                        c = convert_from_utf_to_current_c (c, edit->converter);
                    else
                        c = convert_to_display_c (c);
#endif

                    /* Caret notation for control characters */
                    if (c < 32)
                    {
                        p->ch = '^';
                        p->style = abn_style;
                        p++;
                        p->ch = c + 0x40;
                        p->style = abn_style;
                        p++;
                        col += 2;
                        control_char = TRUE;
                        break;
                    }
                    if (c == 127)
                    {
                        p->ch = '^';
                        p->style = abn_style;
                        p++;
                        p->ch = '?';
                        p->style = abn_style;
                        p++;
                        col += 2;
                        control_char = TRUE;
                        break;
                    }
#ifdef HAVE_CHARSET
                    if (edit->utf8)
                    {
                        if (g_unichar_isprint (c))
                            p->ch = c;
                        else
                        {
                            p->ch = '.';
                            p->style = abn_style;
                        }
                        p++;
                    }
                    else
#endif
                    {
                        if ((mc_global.utf8_display && g_unichar_isprint (c)) ||
                            (!mc_global.utf8_display && is_printable (c)))
                        {
                            p->ch = c;
                            p++;
                        }
                        else
                        {
                            p->ch = '.';
                            p->style = abn_style;
                            p++;
                        }
                    }
                    col++;
                    break;
                }               /* case */

                q++;
                if (cw > 1)
                {
                    q += cw - 1;
                }

                if (col > (end_col - edit->start_col + 1))
                {
                    if (wide_width_char)
                    {
                        p--;
                        break;
                    }
                    if (control_char)
                    {
                        p -= 2;
                        break;
                    }
                }
            }
        }
    }
    else
    {
        start_col_real = start_col = 0;
    }

    p->ch = 0;

    print_to_widget (edit, row, start_col, start_col_real, end_col, line, line_stat, book_mark);
}

/* --------------------------------------------------------------------------------------------- */

static inline void
edit_draw_this_char (WEdit * edit, off_t curs, long row, long start_column, long end_column)
{
    off_t b = edit_bol (edit, curs);

    edit_draw_this_line (edit, b, row, start_column, end_column);
}

/* --------------------------------------------------------------------------------------------- */
/** cursor must be in screen for other than REDRAW_PAGE passed in force */

static inline void
render_edit_text (WEdit * edit, long start_row, long start_column, long end_row, long end_column)
{
    static long prev_curs_row = 0;
    static off_t prev_curs = 0;

    Widget *w = (Widget *) edit;
    Dlg_head *h = w->owner;

    long row = 0, curs_row;
    int force = edit->force;
    long b;
    int y1, x1, y2, x2;

    int last_line;
    int last_column;

    /* draw only visible region */

    last_line = h->y + h->lines - 1;

    y1 = w->y;
    if (y1 > last_line - 1 /* buttonbar */ )
        return;

    last_column = h->x + h->cols - 1;

    x1 = w->x;
    if (x1 > last_column)
        return;

    y2 = w->y + w->lines - 1;
    if (y2 < h->y + 1 /* menubar */ )
        return;

    x2 = w->x + w->cols - 1;
    if (x2 < h->x)
        return;

    if ((force & REDRAW_IN_BOUNDS) == 0)
    {
        /* !REDRAW_IN_BOUNDS means to ignore bounds and redraw whole rows */
        /* draw only visible region */

        if (y2 <= last_line - 1 /* buttonbar */ )
            end_row = w->lines - 1;
        else if (y1 >= h->y + 1 /* menubar */ )
            end_row = h->lines - 1 - y1 - 1;
        else
            end_row = start_row + h->lines - 1 - 1;

        if (x2 <= last_column)
            end_column = w->cols - 1;
        else if (x1 >= h->x)
            end_column = h->cols - 1 - x1;
        else
            end_column = start_column + h->cols - 1;
    }

    /*
     * If the position of the page has not moved then we can draw the cursor
     * character only.  This will prevent line flicker when using arrow keys.
     */
    if ((force & REDRAW_CHAR_ONLY) == 0 || (force & REDRAW_PAGE) != 0)
    {
        if ((force & REDRAW_PAGE) != 0)
        {
            row = start_row;
            b = edit_move_forward (edit, edit->start_display, start_row, 0);
            while (row <= end_row)
            {
                if (key_pending (edit))
                    return;
                edit_draw_this_line (edit, b, row, start_column, end_column);
                b = edit_move_forward (edit, b, 1, 0);
                row++;
            }
        }
        else
        {
            curs_row = edit->curs_row;

            if ((force & REDRAW_BEFORE_CURSOR) != 0 && start_row < curs_row)
            {
                long upto = curs_row - 1 <= end_row ? curs_row - 1 : end_row;

                row = start_row;
                b = edit->start_display;
                while (row <= upto)
                {
                    if (key_pending (edit))
                        return;
                    edit_draw_this_line (edit, b, row, start_column, end_column);
                    b = edit_move_forward (edit, b, 1, 0);
                }
            }

            /*          if (force & REDRAW_LINE)          ---> default */
            b = edit_bol (edit, edit->curs1);
            if (curs_row >= start_row && curs_row <= end_row)
            {
                if (key_pending (edit))
                    return;
                edit_draw_this_line (edit, b, curs_row, start_column, end_column);
            }

            if ((force & REDRAW_AFTER_CURSOR) != 0 && end_row > curs_row)
            {
                row = curs_row + 1 < start_row ? start_row : curs_row + 1;
                b = edit_move_forward (edit, b, 1, 0);
                while (row <= end_row)
                {
                    if (key_pending (edit))
                        return;
                    edit_draw_this_line (edit, b, row, start_column, end_column);
                    b = edit_move_forward (edit, b, 1, 0);
                    row++;
                }
            }

            if ((force & REDRAW_LINE_ABOVE) != 0 && curs_row >= 1)
            {
                row = curs_row - 1;
                b = edit_move_backward (edit, edit_bol (edit, edit->curs1), 1);
                if (row >= start_row && row <= end_row)
                {
                    if (key_pending (edit))
                        return;
                    edit_draw_this_line (edit, b, row, start_column, end_column);
                }
            }

            if ((force & REDRAW_LINE_BELOW) != 0 && row < edit->widget.lines - 1)
            {
                row = curs_row + 1;
                b = edit_bol (edit, edit->curs1);
                b = edit_move_forward (edit, b, 1, 0);
                if (row >= start_row && row <= end_row)
                {
                    if (key_pending (edit))
                        return;
                    edit_draw_this_line (edit, b, row, start_column, end_column);
                }
            }
        }
    }
    else if (prev_curs_row < edit->curs_row)
    {
        /* with the new text highlighting, we must draw from the top down */
        edit_draw_this_char (edit, prev_curs, prev_curs_row, start_column, end_column);
        edit_draw_this_char (edit, edit->curs1, edit->curs_row, start_column, end_column);
    }
    else
    {
        edit_draw_this_char (edit, edit->curs1, edit->curs_row, start_column, end_column);
        edit_draw_this_char (edit, prev_curs, prev_curs_row, start_column, end_column);
    }

    edit->force = 0;

    prev_curs_row = edit->curs_row;
    prev_curs = edit->curs1;
}

/* --------------------------------------------------------------------------------------------- */

static inline void
edit_render (WEdit * edit, int page, int row_start, int col_start, int row_end, int col_end)
{
    if (page)                   /* if it was an expose event, 'page' would be set */
        edit->force |= REDRAW_PAGE | REDRAW_IN_BOUNDS;

    if (edit->force & REDRAW_COMPLETELY)
        buttonbar_redraw (find_buttonbar (edit->widget.owner));
    render_edit_text (edit, row_start, col_start, row_end, col_end);
    /*
     * edit->force != 0 means a key was pending and the redraw
     * was halted, so next time we must redraw everything in case stuff
     * was left undrawn from a previous key press.
     */
    if (edit->force)
        edit->force |= REDRAW_PAGE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** Draw the status line at the top of the screen. The size of the filename
 * field varies depending on the width of the screen and the length of
 * the filename. */
void
edit_status (WEdit * edit)
{
    const int w = edit->widget.owner->cols;
    const size_t status_size = w + 1;
    char *const status = g_malloc (status_size);
    int status_len;
    const char *fname = "";
    int fname_len;
    const int gap = 3;          /* between the filename and the status */
    const int right_gap = 5;    /* at the right end of the screen */
    const int preferred_fname_len = 16;

    status_string (edit, status, status_size);
    status_len = (int) str_term_width1 (status);

    if (edit->filename_vpath != NULL)
        fname = vfs_path_get_last_path_str (edit->filename_vpath);

    fname_len = str_term_width1 (fname);
    if (fname_len < preferred_fname_len)
        fname_len = preferred_fname_len;

    if (fname_len + gap + status_len + right_gap >= w)
    {
        if (preferred_fname_len + gap + status_len + right_gap >= w)
            fname_len = preferred_fname_len;
        else
            fname_len = w - (gap + status_len + right_gap);
        fname = str_trunc (fname, fname_len);
    }

    dlg_move (edit->widget.owner, 0, 0);
    tty_setcolor (STATUSBAR_COLOR);
    printwstr (fname, fname_len + gap);
    printwstr (status, w - (fname_len + gap));

    if (simple_statusbar && w > EDITOR_MINIMUM_TERMINAL_WIDTH)
    {
        size_t percent = 100;

        if (edit->total_lines + 1 != 0)
            percent = (edit->curs_line + 1) * 100 / (edit->total_lines + 1);
        dlg_move (edit->widget.owner, 0, w - 5);
        tty_printf (" %3d%%", percent);
    }

    g_free (status);
}

/* --------------------------------------------------------------------------------------------- */
/** this scrolls the text so that cursor is on the screen */
void
edit_scroll_screen_over_cursor (WEdit * edit)
{
    long p;
    long outby;
    int b_extreme, t_extreme, l_extreme, r_extreme;

    if (edit->widget.lines <= 0 || edit->widget.cols <= 0)
        return;

    edit->widget.cols -= EDIT_TEXT_HORIZONTAL_OFFSET + option_line_state_width;
    edit->widget.lines -= EDIT_TEXT_VERTICAL_OFFSET;

    r_extreme = EDIT_RIGHT_EXTREME;
    l_extreme = EDIT_LEFT_EXTREME;
    b_extreme = EDIT_BOTTOM_EXTREME;
    t_extreme = EDIT_TOP_EXTREME;
    if (edit->found_len != 0)
    {
        b_extreme = max (edit->widget.lines / 4, b_extreme);
        t_extreme = max (edit->widget.lines / 4, t_extreme);
    }
    if (b_extreme + t_extreme + 1 > edit->widget.lines)
    {
        int n;

        n = b_extreme + t_extreme;
        if (n == 0)
            n = 1;
        b_extreme = (b_extreme * (edit->widget.lines - 1)) / n;
        t_extreme = (t_extreme * (edit->widget.lines - 1)) / n;
    }
    if (l_extreme + r_extreme + 1 > edit->widget.cols)
    {
        int n;

        n = l_extreme + t_extreme;
        if (n == 0)
            n = 1;
        l_extreme = (l_extreme * (edit->widget.cols - 1)) / n;
        r_extreme = (r_extreme * (edit->widget.cols - 1)) / n;
    }
    p = edit_get_col (edit) + edit->over_col;
    edit_update_curs_row (edit);
    outby = p + edit->start_col - edit->widget.cols + 1 + (r_extreme + edit->found_len);
    if (outby > 0)
        edit_scroll_right (edit, outby);
    outby = l_extreme - p - edit->start_col;
    if (outby > 0)
        edit_scroll_left (edit, outby);
    p = edit->curs_row;
    outby = p - edit->widget.lines + 1 + b_extreme;
    if (outby > 0)
        edit_scroll_downward (edit, outby);
    outby = t_extreme - p;
    if (outby > 0)
        edit_scroll_upward (edit, outby);
    edit_update_curs_row (edit);

    edit->widget.lines += EDIT_TEXT_VERTICAL_OFFSET;
    edit->widget.cols += EDIT_TEXT_HORIZONTAL_OFFSET + option_line_state_width;
}

/* --------------------------------------------------------------------------------------------- */

void
edit_render_keypress (WEdit * edit)
{
    edit_render (edit, 0, 0, 0, 0, 0);
}

/* --------------------------------------------------------------------------------------------- */

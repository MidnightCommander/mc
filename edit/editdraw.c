/* editor text drawing.

   Copyright (C) 1996, 1997 the Free Software Foundation

   Authors: 1996, 1997 Paul Sheer

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.
*/

#include <config.h>
#include "edit.h"
#include "edit-widget.h"

#define MAX_LINE_LEN 1024

#include "src/color.h"		/* EDITOR_NORMAL_COLOR */
#include "src/tty.h"		/* attrset() */
#include "src/widget.h"		/* redraw_labels() */
#include "src/key.h"		/* is_idle() */

#include "src/charsets.h"

/* Text styles */
#define MOD_ABNORMAL		(1 << 8)
#define MOD_BOLD		(1 << 9)
#define MOD_MARKED		(1 << 10)
#define MOD_CURSOR		(1 << 11)

#define FONT_OFFSET_X 0
#define FONT_OFFSET_Y 0
#define FIXED_FONT 1
#define FONT_PIX_PER_LINE 1
#define FONT_MEAN_WIDTH 1


static void status_string (WEdit * edit, char *s, int w, int fill)
{
    char byte_str[16];

    /*
     * If we are at the end of file, print <EOF>,
     * otherwise print the current character as is (if printable),
     * as decimal and as hex.
     */
    if (edit->curs1 < edit->last_byte) {
	unsigned char cur_byte = edit_get_byte (edit, edit->curs1);
	g_snprintf (byte_str, sizeof(byte_str), "%c %3d 0x%02X",
		    is_printable(cur_byte) ? cur_byte : '.',
		    cur_byte,
		    cur_byte);
    } else {
	strcpy(byte_str, "<EOF>");
    }

    /* The field lengths just prevent the status line from shortening too much */
    g_snprintf (s, w,
		"[%c%c%c%c] %2ld L:[%3ld+%2ld %3ld/%3ld] *(%-4ld/%4ldb)= %s",
		edit->mark1 != edit->mark2 ? ( column_highlighting ? 'C' : 'B') : '-',
		edit->modified ? 'M' : '-',
		edit->macro_i < 0 ? '-' : 'R',
		edit->overwrite == 0 ? '-' : 'O',
		edit->curs_col,

		edit->start_line + 1,
		edit->curs_row,
		edit->curs_line + 1,
		edit->total_lines + 1,

		edit->curs1,
		edit->last_byte, 
		byte_str);
}

/* how to get as much onto the status line as is numerically possible :) */
void
edit_status (WEdit *edit)
{
    int w, i, t;
    char *s;
    w = edit->widget.cols;
    s = g_malloc (w + 16);
    if (w < 4)
	w = 4;
    memset (s, ' ', w);
    attrset (SELECTED_COLOR);
    if (w > 4) {
	widget_move (edit, 0, 0);
	if (edit->filename) {
	    i = w > 24 ? 18 : w - 6;
	    i = i < 13 ? 13 : i;
	    strcpy (s, name_trunc (edit->filename, i));
	    i = strlen (s);
	    s[i] = ' ';
	}
	t = w - 20;
	if (t > 1)		/* g_snprintf() must write at least '\000' */
	    status_string (edit, s + 20, t + 1 /* for '\000' */ , ' ');
    }
    s[w] = 0;

    printw ("%-*s", w, s);
    attrset (EDITOR_NORMAL_COLOR);
    g_free (s);
}

/* this scrolls the text so that cursor is on the screen */
void edit_scroll_screen_over_cursor (WEdit * edit)
{
    int p;
    int outby;
    int b_extreme, t_extreme, l_extreme, r_extreme;

    if (edit->num_widget_lines <= 0 || edit->num_widget_columns <= 0)
	return;

    r_extreme = EDIT_RIGHT_EXTREME;
    l_extreme = EDIT_LEFT_EXTREME;
    b_extreme = EDIT_BOTTOM_EXTREME;
    t_extreme = EDIT_TOP_EXTREME;
    if (edit->found_len) {
	b_extreme = max (edit->num_widget_lines / 4, b_extreme);
	t_extreme = max (edit->num_widget_lines / 4, t_extreme);
    }
    if (b_extreme + t_extreme + 1 > edit->num_widget_lines) {
	int n;
	n = b_extreme + t_extreme;
	b_extreme = (b_extreme * (edit->num_widget_lines - 1)) / n;
	t_extreme = (t_extreme * (edit->num_widget_lines - 1)) / n;
    }
    if (l_extreme + r_extreme + 1 > edit->num_widget_columns) {
	int n;
	n = l_extreme + t_extreme;
	l_extreme = (l_extreme * (edit->num_widget_columns - 1)) / n;
	r_extreme = (r_extreme * (edit->num_widget_columns - 1)) / n;
    }
    p = edit_get_col (edit);
    edit_update_curs_row (edit);
    outby = p + edit->start_col - edit->num_widget_columns + 1 + (r_extreme + edit->found_len);
    if (outby > 0)
	edit_scroll_right (edit, outby);
    outby = l_extreme - p - edit->start_col;
    if (outby > 0)
	edit_scroll_left (edit, outby);
    p = edit->curs_row;
    outby = p - edit->num_widget_lines + 1 + b_extreme;
    if (outby > 0)
	edit_scroll_downward (edit, outby);
    outby = t_extreme - p;
    if (outby > 0)
	edit_scroll_upward (edit, outby);
    edit_update_curs_row (edit);
}

#define set_color(font)    attrset (font)

#define edit_move(x,y) widget_move(edit, y, x);

/* Set colorpair by index, don't interpret S-Lang "emulated attributes" */
#ifdef HAVE_SLANG
#define lowlevel_set_color(x) SLsmg_set_color(x & 0x7F)
#else
#define lowlevel_set_color(x) attrset(MY_COLOR_PAIR(color))
#endif

static void
print_to_widget (WEdit *edit, long row, int start_col, int start_col_real,
		 long end_col, unsigned int line[])
{
    unsigned int *p;

    int x = start_col_real + EDIT_TEXT_HORIZONTAL_OFFSET;
    int x1 = start_col + EDIT_TEXT_HORIZONTAL_OFFSET;
    int y = row + EDIT_TEXT_VERTICAL_OFFSET;

    set_color (EDITOR_NORMAL_COLOR);
    edit_move (x1, y);
    hline (' ', end_col + 1 - EDIT_TEXT_HORIZONTAL_OFFSET - x1);

    edit_move (x + FONT_OFFSET_X, y + FONT_OFFSET_Y);
    p = line;

    while (*p) {
	int style = *p & 0xFF00;
	int textchar = *p & 0xFF;
	int color = *p >> 16;

	if (style & MOD_ABNORMAL) {
	    /* Non-printable - use black background */
	    color = 0;
	}

	if (style & MOD_BOLD) {
	    set_color (EDITOR_BOLD_COLOR);
	} else if (style & MOD_MARKED) {
	    set_color (EDITOR_MARKED_COLOR);
	} else {
	    lowlevel_set_color (color);
	}

	addch (textchar);
	p++;
    }
}

/* b is a pointer to the beginning of the line */
static void
edit_draw_this_line (WEdit *edit, long b, long row, long start_col,
		     long end_col)
{
    static unsigned int line[MAX_LINE_LEN];
    unsigned int *p = line;
    long m1 = 0, m2 = 0, q, c1, c2;
    int col, start_col_real;
    unsigned int c;
    int color;
    int i, book_mark = -1;

#if 0
    if (!book_mark_query (edit, edit->start_line + row, &book_mark))
	book_mark = -1;
#endif

    edit_get_syntax_color (edit, b - 1, &color);
    q = edit_move_forward3 (edit, b, start_col - edit->start_col, 0);
    start_col_real = (col =
		      (int) edit_move_forward3 (edit, b, 0,
						q)) + edit->start_col;
    c1 = min (edit->column1, edit->column2);
    c2 = max (edit->column1, edit->column2);

    if (col + 16 > -edit->start_col) {
	eval_marks (edit, &m1, &m2);

	if (row <= edit->total_lines - edit->start_line) {
	    while (col <= end_col - edit->start_col) {
		*p = 0;
		if (q == edit->curs1)
		    *p |= MOD_CURSOR;
		if (q >= m1 && q < m2) {
		    if (column_highlighting) {
			int x;
			x = edit_move_forward3 (edit, b, 0, q);
			if (x >= c1 && x < c2)
			    *p |= MOD_MARKED;
		    } else
			*p |= MOD_MARKED;
		}
		if (q == edit->bracket)
		    *p |= MOD_BOLD;
		if (q >= edit->found_start
		    && q < edit->found_start + edit->found_len)
		    *p |= MOD_BOLD;
		c = edit_get_byte (edit, q);
/* we don't use bg for mc - fg contains both */
		if (book_mark == -1) {
		    edit_get_syntax_color (edit, q, &color);
		    *p |= color << 16;
		} else {
		    *p |= book_mark << 16;
		}
		q++;
		switch (c) {
		case '\n':
		    col = end_col - edit->start_col + 1;	/* quit */
		    *(p++) |= ' ';
		    break;
		case '\t':
		    i = TAB_SIZE - ((int) col % TAB_SIZE);
		    *p |= ' ';
		    c = *(p++) & ~MOD_CURSOR;
		    col += i;
		    while (--i)
			*(p++) = c;
		    break;
		default:
		    c = convert_to_display_c (c);

		    /* Caret notation for control characters */
		    if (c < 32) {
			*(p++) = '^' | MOD_ABNORMAL;
			*(p++) = (c + 0x40) | MOD_ABNORMAL;
			col += 2;
			break;
		    }
		    if (c == 127) {
			*(p++) = '^' | MOD_ABNORMAL;
			*(p++) = '?' | MOD_ABNORMAL;
			col += 2;
			break;
		    }

		    if (is_printable (c)) {
			*(p++) |= c;
		    } else {
			*(p++) = '.' | MOD_ABNORMAL;
		    }
		    col++;
		    break;
		}
	    }
	}
    } else {
	start_col_real = start_col = 0;
    }
    *p = 0;

    print_to_widget (edit, row, start_col, start_col_real, end_col, line);
}

#define key_pending(x) (!is_idle())

static void edit_draw_this_char (WEdit * edit, long curs, long row)
{
    int b = edit_bol (edit, curs);
    edit_draw_this_line (edit, b, row, 0, edit->num_widget_columns - 1);
}

/* cursor must be in screen for other than REDRAW_PAGE passed in force */
static void
render_edit_text (WEdit * edit, long start_row, long start_column, long end_row,
		  long end_column)
{
    long row = 0, curs_row;
    static int prev_curs_row = 0;
    static int prev_start_col = 0;
    static long prev_curs = 0;
    static long prev_start = -1;

    int force = edit->force;
    long b;

/*
 * If the position of the page has not moved then we can draw the cursor
 * character only.  This will prevent line flicker when using arrow keys.
 */
    if ((!(force & REDRAW_CHAR_ONLY)) || (force & REDRAW_PAGE)) {
	if (!(force & REDRAW_IN_BOUNDS)) {	/* !REDRAW_IN_BOUNDS means to ignore bounds and redraw whole rows */
	    start_row = 0;
	    end_row = edit->num_widget_lines - 1;
	    start_column = 0;
	    end_column = edit->num_widget_columns - 1;
	}
	if (force & REDRAW_PAGE) {
	    row = start_row;
	    b = edit_move_forward (edit, edit->start_display, start_row, 0);
	    while (row <= end_row) {
		if (key_pending (edit))
		    goto exit_render;
		edit_draw_this_line (edit, b, row, start_column, end_column);
		b = edit_move_forward (edit, b, 1, 0);
		row++;
	    }
	} else {
	    curs_row = edit->curs_row;

	    if (force & REDRAW_BEFORE_CURSOR) {
		if (start_row < curs_row) {
		    long upto = curs_row - 1 <= end_row ? curs_row - 1 : end_row;
		    row = start_row;
		    b = edit->start_display;
		    while (row <= upto) {
			if (key_pending (edit))
			    goto exit_render;
			edit_draw_this_line (edit, b, row, start_column, end_column);
			b = edit_move_forward (edit, b, 1, 0);
		    }
		}
	    }
/*          if (force & REDRAW_LINE)          ---> default */
	    b = edit_bol (edit, edit->curs1);
	    if (curs_row >= start_row && curs_row <= end_row) {
		if (key_pending (edit))
		    goto exit_render;
		edit_draw_this_line (edit, b, curs_row, start_column, end_column);
	    }
	    if (force & REDRAW_AFTER_CURSOR) {
		if (end_row > curs_row) {
		    row = curs_row + 1 < start_row ? start_row : curs_row + 1;
		    b = edit_move_forward (edit, b, 1, 0);
		    while (row <= end_row) {
			if (key_pending (edit))
			    goto exit_render;
			edit_draw_this_line (edit, b, row, start_column, end_column);
			b = edit_move_forward (edit, b, 1, 0);
			row++;
		    }
		}
	    }
	    if (force & REDRAW_LINE_ABOVE && curs_row >= 1) {
		row = curs_row - 1;
		b = edit_move_backward (edit, edit_bol (edit, edit->curs1), 1);
		if (row >= start_row && row <= end_row) {
		    if (key_pending (edit))
			goto exit_render;
		    edit_draw_this_line (edit, b, row, start_column, end_column);
		}
	    }
	    if (force & REDRAW_LINE_BELOW && row < edit->num_widget_lines - 1) {
		row = curs_row + 1;
		b = edit_bol (edit, edit->curs1);
		b = edit_move_forward (edit, b, 1, 0);
		if (row >= start_row && row <= end_row) {
		    if (key_pending (edit))
			goto exit_render;
		    edit_draw_this_line (edit, b, row, start_column, end_column);
		}
	    }
	}
    } else {
	if (prev_curs_row < edit->curs_row) {	/* with the new text highlighting, we must draw from the top down */
	    edit_draw_this_char (edit, prev_curs, prev_curs_row);
	    edit_draw_this_char (edit, edit->curs1, edit->curs_row);
	} else {
	    edit_draw_this_char (edit, edit->curs1, edit->curs_row);
	    edit_draw_this_char (edit, prev_curs, prev_curs_row);
	}
    }

    edit->force = 0;

    prev_curs_row = edit->curs_row;
    prev_curs = edit->curs1;
    prev_start_col = edit->start_col;
  exit_render:
    edit->screen_modified = 0;
    prev_start = edit->start_line;
    return;
}

static void
edit_render (WEdit * edit, int page, int row_start, int col_start, int row_end, int col_end)
{
    if (page)			/* if it was an expose event, 'page' would be set */
	edit->force |= REDRAW_PAGE | REDRAW_IN_BOUNDS;

    if (edit->force & REDRAW_COMPLETELY)
	redraw_labels (edit->widget.parent);
    render_edit_text (edit, row_start, col_start, row_end, col_end);
    /*
     * edit->force != 0 means a key was pending and the redraw 
     * was halted, so next time we must redraw everything in case stuff
     * was left undrawn from a previous key press.
     */
    if (edit->force)
	edit->force |= REDRAW_PAGE;
}

void edit_render_keypress (WEdit * edit)
{
    edit_render (edit, 0, 0, 0, 0, 0);
}

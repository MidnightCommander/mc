/* editor text drawing.

   Copyright (C) 1996, 1997, 1998, 2001, 2002, 2003, 2004, 2005, 2006,
   2007 Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#include "../src/global.h"

#include "edit.h"
#include "edit-widget.h"

#define MAX_LINE_LEN 1024

#include "../src/color.h"	/* EDITOR_NORMAL_COLOR */
#include "../src/tty.h"		/* attrset() */
#include "../src/widget.h"	/* buttonbar_redraw() */
#include "../src/key.h"		/* is_idle() */
#include "../src/charsets.h"
#include "../src/strutil.h"	/* utf string functions */
#include "../src/main.h"	/* source_codepage */

/* Text styles */
#define MOD_ABNORMAL		(1 << 8)
#define MOD_BOLD		(1 << 9)
#define MOD_MARKED		(1 << 10)
#define MOD_CURSOR		(1 << 11)
#define MOD_WHITESPACE		(1 << 12)

#define FONT_OFFSET_X 0
#define FONT_OFFSET_Y 0
#define FIXED_FONT 1
#define FONT_PIX_PER_LINE 1
#define FONT_MEAN_WIDTH 1


static void status_string (WEdit * edit, char *s, int w)
{
    char byte_str[16];

    /*
     * If we are at the end of file, print <EOF>,
     * otherwise print the current character as is (if printable),
     * as decimal and as hex.
     */
    if (edit->curs1 < edit->last_byte) {
	unsigned char cur_byte = edit_get_byte (edit, edit->curs1);
	g_snprintf (byte_str, sizeof (byte_str), "%c %3d 0x%02X",
		    is_printable (cur_byte) ? cur_byte : '.',
		    (int) cur_byte,
		    (unsigned) cur_byte);
    } else {
	strcpy (byte_str, "<EOF>");
    }

    /* The field lengths just prevent the status line from shortening too much */
    g_snprintf (s, w,
		"[%c%c%c%c] %2ld L:[%3ld+%2ld %3ld/%3ld] *(%-4ld/%4ldb)= %s C:%s",
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
		byte_str,
		get_codepage_id( source_codepage ));
}

static inline void
printwstr (const char *s, int len)
{
    if (len > 0)
	tty_printf ("%-*.*s", len, len, s);
}

/* Draw the status line at the top of the widget. The size of the filename
 * field varies depending on the width of the screen and the length of
 * the filename. */
void
edit_status (WEdit *edit)
{
    const int w = edit->widget.cols;
    const size_t status_size = w + 1;
    char * const status = g_malloc (status_size);
    int status_len;
    const char *fname = "";
    int fname_len;
    const int gap = 3; /* between the filename and the status */
    const int right_gap = 2; /* at the right end of the screen */
    const int preferred_fname_len = 16;

    status_string (edit, status, status_size);
    status_len = (int) str_term_width1 (status);

    if (edit->filename)
        fname = edit->filename;
    fname_len = str_term_width1 (fname);
    if (fname_len < preferred_fname_len)
        fname_len = preferred_fname_len;

    if (fname_len + gap + status_len + right_gap >= w) {
	if (preferred_fname_len + gap + status_len + right_gap >= w)
	    fname_len = preferred_fname_len;
	else
            fname_len = w - (gap + status_len + right_gap);
	fname = str_trunc (fname, fname_len);
    }

    widget_move (edit, 0, 0);
    attrset (SELECTED_COLOR);
    printwstr (fname, fname_len + gap);
    printwstr (status, w - (fname_len + gap));
    attrset (EDITOR_NORMAL_COLOR);

    g_free (status);
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

struct line_s {
    unsigned int ch;
    unsigned int style;
};

static void
print_to_widget (WEdit *edit, long row, int start_col, int start_col_real,
		 long end_col, struct line_s line[])
{
    struct line_s *p;

    int x = start_col_real + EDIT_TEXT_HORIZONTAL_OFFSET;
    int x1 = start_col + EDIT_TEXT_HORIZONTAL_OFFSET;
    int y = row + EDIT_TEXT_VERTICAL_OFFSET;
    int cols_to_skip = abs (x);

    set_color (EDITOR_NORMAL_COLOR);
    edit_move (x1, y);
    hline (' ', end_col + 1 - EDIT_TEXT_HORIZONTAL_OFFSET - x1);

    edit_move (x1 + FONT_OFFSET_X, y + FONT_OFFSET_Y);
    p = line;

    while (p->ch) {
	int style;
	unsigned int textchar;
	int color;

	if (cols_to_skip) {
	    p++;
	    cols_to_skip--;
	    continue;
	}

	style = p->style & 0xFF00;
	textchar = p->ch;
	color = p->style >> 16;

	if (style & MOD_ABNORMAL) {
	    /* Non-printable - use black background */
	    color = 0;
	}

	if (style & MOD_WHITESPACE) {
	    if (style & MOD_MARKED) {
		textchar = ' ';
		set_color (EDITOR_MARKED_COLOR);
	    } else {
#if 0
		if (color != EDITOR_NORMAL_COLOR) {
		    textchar = ' ';
		    lowlevel_set_color (color);
		} else
#endif
		    set_color (EDITOR_WHITESPACE_COLOR);
	    }
	} else {
	    if (style & MOD_BOLD) {
		set_color (EDITOR_BOLD_COLOR);
	    } else if (style & MOD_MARKED) {
		set_color (EDITOR_MARKED_COLOR);
	    } else {
		lowlevel_set_color (color);
	    }
	}
	addch (textchar);
	p++;
    }
}

int visible_tabs = 1, visible_tws = 1;

/* b is a pointer to the beginning of the line */
static void
edit_draw_this_line (WEdit *edit, long b, long row, long start_col,
		     long end_col)
{
    struct line_s line[MAX_LINE_LEN];
    struct line_s *p = line;

    long m1 = 0, m2 = 0, q, c1, c2;
    int col, start_col_real;
    int cw;
    unsigned int c;
    int color;
    int i;

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
		long tws;
	    if (use_colors && visible_tws) {
		tws = edit_eol (edit, b);
		while (tws > b && ((c = edit_get_byte (edit, tws - 1)) == ' '
				   || c == '\t'))
		    tws--;
	    }

	    while (col <= end_col - edit->start_col) {
		p->ch = 0;
		p->style = 0;
		if (q == edit->curs1)
		    p->style |= MOD_CURSOR;
		if (q >= m1 && q < m2) {
		    if (column_highlighting) {
			int x;
			x = edit_move_forward3 (edit, b, 0, q);
			if (x >= c1 && x < c2)
			    p->style |= MOD_MARKED;
		    } else
			p->style |= MOD_MARKED;
		}
		if (q == edit->bracket)
		    p->style |= MOD_BOLD;
		if (q >= edit->found_start
		    && q < edit->found_start + edit->found_len)
		    p->style |= MOD_BOLD;
		cw = 1;
		if ( !edit->utf8 ) {
		    c = edit_get_byte (edit, q);
		} else {
		    c = edit_get_utf (edit, q, &cw);
		}
                /* we don't use bg for mc - fg contains both */
		edit_get_syntax_color (edit, q, &color);
		p->style |= color << 16;
		switch (c) {
		case '\n':
		    col = end_col - edit->start_col + 1;	/* quit */
		    p->ch = ' ';
		    p++;
		    break;
		case '\t':
		    i = TAB_SIZE - ((int) col % TAB_SIZE);
		    col += i;
		    if (use_colors && visible_tabs) {
			c = 0;
			if (i > 2) {
			    p->ch |= '<';
			    p->style = MOD_WHITESPACE;
			    p++;
			    while (--i > 1) {
				p->ch = c | '-';
				p->style = MOD_WHITESPACE;
				p++;
			    }
			    p->ch = c | '>';
			    p->style = MOD_WHITESPACE;
			    p++;
			} else if (i > 1) {
			    p->ch |= '<';
			    p->style = MOD_WHITESPACE;
			    p++;
			    p->ch = c | '>';
			    p->style = MOD_WHITESPACE;
			    p++;
			} else {
			    p->ch |= '>';
			    p->style = MOD_WHITESPACE;
			    p++;
			}
		    } else if (use_colors && visible_tws && q >= tws) {
			p->ch |= '.';
			p->style = MOD_WHITESPACE;
			c = p->style & ~MOD_CURSOR;
			p++;
			while (--i) {
			    p->ch = ' '; 
			    p->style = c;
			    p++;
			}
		    } else {
			p->ch |= ' ';
			c = p->style & ~MOD_CURSOR;
			p++;
			while (--i) {
			    p->ch = ' '; 
			    p->style = c;
			    p++;
			}
		    }
		    break;
		case ' ':
		    if (use_colors && visible_tws && q >= tws) {
			p->ch |= '.';
			p->style = MOD_WHITESPACE;
			p++;
			col++;
			break;
		    }
		    /* fallthrough */
		default:
		    if (!edit->utf8) {
		        c = convert_to_display_c (c);
		    } else {
		        //FIXME: if need
		    }
		    /* Caret notation for control characters */
		    if (c < 32) {
			p->ch = '^';
			p->style = MOD_ABNORMAL;
			p++;
			p->ch = c + 0x40;
			p->style = MOD_ABNORMAL;
			p++;
			col += 2;
			break;
		    }
		    if (c == 127) {
			p->ch = '^';
			p->style = MOD_ABNORMAL;
			p++;
			p->ch = '?';
			p->style = MOD_ABNORMAL;
			p++;
			col += 2;
			break;
		    }
		    if (!edit->utf8) {
			if (is_printable (c)) {
			    p->ch = c;
			    p++;
			} else {
			    p->ch = '.';
			    p->style = MOD_ABNORMAL;
			    p++;
			}
		    } else {
			p->ch = c;
			p++;
		        if ( cw > 1)
		            col += cw - 1;
		    }
		    col++;
		    break;
		}
		q++;
	    }
	}
    } else {
	start_col_real = start_col = 0;
    }
    p->ch = 0;

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
    static long prev_curs = 0;

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

  exit_render:
    edit->screen_modified = 0;
    return;
}

static void
edit_render (WEdit * edit, int page, int row_start, int col_start, int row_end, int col_end)
{
    if (page)			/* if it was an expose event, 'page' would be set */
	edit->force |= REDRAW_PAGE | REDRAW_IN_BOUNDS;

    if (edit->force & REDRAW_COMPLETELY)
	buttonbar_redraw (edit->widget.parent);
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

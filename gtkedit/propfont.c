/* propfont.c - editor text drawing for proportional fonts.
   Copyright (C) 1997 Paul Sheer

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
#if defined (HAVE_MAD) && ! defined (MIDNIGHT) && ! defined (GTK)
#include "mad.h"
#endif

/* this file definatively relies on int being 32 bits or more */

int option_long_whitespace = 0;

#define MAX_LINE_LEN 1024
#define CACHE_WIDTH 256
#define CACHE_HEIGHT 128


struct cache_line {
    int x0, x1;
    cache_type data[CACHE_WIDTH];
};

extern unsigned char per_char[256];

/* background colors: marked is refers to mouse highlighting, highlighted refers to a found string. */
extern unsigned long edit_abnormal_color, edit_marked_abnormal_color;
extern unsigned long edit_highlighted_color, edit_marked_color;
extern unsigned long edit_normal_background_color;

/* foreground colors */
extern unsigned long edit_normal_foreground_color, edit_bold_color;
extern unsigned long edit_italic_color;

/* cursor color */
extern unsigned long edit_cursor_color;

extern int EditExposeRedraw;
extern int EditClear;

int set_style_color (
#ifdef GTK
			 Window win,
#endif
		      cache_type s, unsigned long *fg, unsigned long *bg)
{
    int fgp, bgp, underlined = 0;
    fgp = (s & 0xFF000000UL) >> 24;
/* NO_COLOR would give fgp == 255 */
    if (fgp < 0xFF)
	*fg = color_palette (fgp);
    else
	*fg = edit_normal_foreground_color;
    bgp = (s & 0x00FF0000) >> 16;
    if (bgp == 0xFE)
	underlined = 1;
    if (bgp < 0xFD)
	*bg = color_palette (bgp);
    else
	*bg = edit_normal_background_color;
    if (!(s & 0xFFFFFF00UL))	/* check this first as an optimization */
	return underlined;
    if (s & (MOD_ABNORMAL * 256)) {
	*bg = edit_abnormal_color;
	if (s & (MOD_MARKED * 256))
	    *bg = edit_marked_abnormal_color;
    } else if (s & (MOD_HIGHLIGHTED * 256)) {
	*bg = edit_highlighted_color;
    } else if (s & (MOD_MARKED * 256)) {
	*bg = edit_marked_color;
    }
    if (s & (MOD_BOLD * 256))
	*fg = edit_bold_color;
    if (s & (MOD_ITALIC * 256))
	*fg = edit_italic_color;
    if (s & (MOD_INVERSE * 256)) {
	unsigned long t;
	t = *fg;
	*fg = *bg;
	*bg = t;
	if (*bg == COLOR_BLACK)
	    *bg = color_palette (1);
    }
    return underlined;
}

#ifdef GTK
#define set_style_color(s,f,b) set_style_color(win,s,f,b)
#endif

int tab_width = 1;

static inline int next_tab_pos (int x)
{
    return x += tab_width - x % tab_width;
}

/* this now properly uses ctypes */
static inline int convert_to_long_printable (int c, unsigned char *t)
{
    if (isgraph (c)) {
	t[0] = c;
	t[1] = 0;
	return per_char[c];
    }
    if (c == ' ') {
	if (option_long_whitespace) {
	    t[0] = ' ';
	    t[1] = ' ';
	    t[2] = 0;
	    return per_char[' '] + per_char[' '];
	} else {
	    t[0] = ' ';
	    t[1] = 0;
	    return per_char[' '];
	}
    }
    if (option_international_characters && per_char[c]) {
	t[0] = c;
	t[1] = 0;
	return per_char[c];
    }
    if (c > '~') {
	t[0] = ("0123456789ABCDEF")[c >> 4];
	t[1] = ("0123456789ABCDEF")[c & 0xF];
	t[2] = 'h';
	t[3] = 0;
	return per_char[t[0]] + per_char[t[1]] + per_char[t[2]];
    }
    t[0] = '^';
    t[1] = c + '@';
    t[2] = 0;
    return per_char[t[0]] + per_char[t[1]];
}

/* same as above but just gets the length */
static inline int width_of_long_printable (int c)
{
    if (isgraph (c))
	return per_char[c];
    if (c == ' ') {
	if (option_long_whitespace)
	    return per_char[' '] + per_char[' '];
	else
	    return per_char[' '];
    }
    if (option_international_characters && per_char[c])
	return per_char[c];
    if (c > '~')
	return per_char[(unsigned char) ("0123456789ABCDEF")[c >> 4]] + per_char[(unsigned char) ("0123456789ABCDEF")[c & 0xF]] + per_char[(unsigned char) 'h'];
    return per_char['^'] + per_char[c + '@'];
}

int edit_width_of_long_printable (int c)
{
    return width_of_long_printable (c);
}

/* returns x pixel pos of char at offset *q with x not more than l */
int calc_text_pos (WEdit * edit, long b, long *q, int l)
{
    int x = 0, c, xn;
    for (;;) {
	c = edit_get_byte (edit, b);
	switch (c) {
	case '\n':
	    *q = b;
	    if (x > edit->max_column)
		edit->max_column = x;
	    return x;
	case '\t':
	    xn = next_tab_pos (x);
	    break;
	default:
	    xn = x + width_of_long_printable (c);
	    break;
	}
	if (xn > l)
	    break;
	x = xn;
	b++;
    }
    *q = b;
    if (x > edit->max_column)
	edit->max_column = x;
    return x;
}


/* calcs pixel length of the line beginning at b up to upto */
int calc_text_len (WEdit * edit, long b, long upto)
{
    int x = 0, c;
    for (;;) {
	if (b == upto) {
	    if (x > edit->max_column)
		edit->max_column = x;
	    return x;
	}
	c = edit_get_byte (edit, b);
	switch (c) {
	case '\n':{
		if (x > edit->max_column)
		    edit->max_column = x;
		return x;
	    }
	case '\t':
	    x = next_tab_pos (x);
	    break;
	default:
	    x += width_of_long_printable (c);
	    break;
	}
	b++;
    }
}

/* If pixels is zero this returns the count of pixels from current to upto. */
/* If upto is zero returns index of pixels across from current. */
long edit_move_forward3 (WEdit * edit, long current, int pixels, long upto)
{
    if (upto) {
	return calc_text_len (edit, current, upto);
    } else if (pixels) {
	long q;
	calc_text_pos (edit, current, &q, pixels);
	return q;
    }
    return current;
}

extern int column_highlighting;

/* gets the characters style (eg marked, highlighted) from its position in the edit buffer */
static inline cache_type get_style_fast (WEdit * edit, long q, int c)
{
    cache_type s = 0;
    unsigned int fg, bg;
    if (!(isprint (c) || (option_international_characters && per_char[c])))
	if (c != '\n' && c != '\t')
	    s |= MOD_ABNORMAL * 256;
    edit_get_syntax_color (edit, q, (int *) &fg, (int *) &bg);
    return s | ((fg & 0xFF) << 24) | ((bg & 0xFF) << 16);
}

/* gets the characters style (eg marked, highlighted) from its position in the edit buffer */
static inline cache_type get_style (WEdit * edit, long q, int c, long m1, long m2, int x)
{
    cache_type s = 0;
    unsigned int fg, bg;
    if (q == edit->curs1)
	s |= MOD_CURSOR * 256;
    if (q >= m1 && q < m2) {
	if (column_highlighting) {
	    if ((x >= edit->column1 && x < edit->column2)
		|| (x >= edit->column2 && x < edit->column1))
		s |= MOD_INVERSE * 256;
	} else {
	    s |= MOD_MARKED * 256;
	}
    }
    if (q == edit->bracket)
	s |= MOD_BOLD * 256;
    if (q >= edit->found_start && q < edit->found_start + edit->found_len)
	s |= MOD_HIGHLIGHTED * 256;
    if (!(isprint (c) || (option_international_characters && per_char[c])))
	if (c != '\n' && c != '\t')
	    s |= MOD_ABNORMAL * 256;
    edit_get_syntax_color (edit, q, (int *) &fg, (int *) &bg);
    return s | ((fg & 0xFF) << 24) | ((bg & 0xFF) << 16);
}

void convert_text (WEdit * edit, long q, cache_type * p, int x, int x_max, int row)
{
    int c;
    cache_type s;
    long m1, m2, last;
    unsigned char *r, text[4];
    int book_mark_colors[10], book_mark;
    eval_marks (edit, &m1, &m2);
    book_mark = book_mark_query_all (edit, edit->start_line + row, book_mark_colors);
    last = q + (x_max - x) / 2 + 2;	/* for optimization, we say that the last character 
					   of this line cannot have an offset greater than this.
					   This can be used to rule out uncommon text styles,
					   like a character with a cursor, or selected text */
    if (book_mark) {
	int the_end = 0, book_mark_cycle = 0;
	for (;;) {
	    c = edit_get_byte (edit, q);
	    if (!the_end)
		*p = get_style (edit, q, c, m1, m2, x);
	    if (the_end)
		*p = 0;
	    *p = (*p & 0x0000FFFF) | (book_mark_colors[book_mark_cycle++ % book_mark] << 16);
	    switch (c) {
	    case '\n':
		the_end = 1;
		c = ' ';
		q--;
		goto the_default;
	    case '\t':
		if (fixed_font) {
		    int t;
		    t = next_tab_pos (x);
		    t = min (t, x_max);
		    s = *p;
		    while (x < t) {
			x += per_char[' '];
			*p++ = s | ' ';
		    }
		} else {
		    *p++ |= '\t';
		    x = next_tab_pos (x);
		}
		break;
	    default:
	      the_default:
		x += convert_to_long_printable (c, text);
		r = text;
		s = *p;
		*p++ = s | *r++;
		if (!*r)
		    break;
		*p++ = s | *r++;
		if (!*r)
		    break;
		*p++ = s | *r++;
		break;
	    }
	    if (x >= x_max)
		break;
	    q++;
	}
    } else if ((m2 < q || m1 > last) && (edit->curs1 < q || edit->curs1 > last) && \
	       (edit->found_start + edit->found_len < q || edit->found_start > last) &&
	       (edit->bracket < q || edit->bracket > last)) {
	for (;;) {
	    c = edit_get_byte (edit, q);
	    *p = get_style_fast (edit, q, c);
	    switch (c) {
	    case '\n':
		*p++ |= ' ';
		*p = 0;
		if (x > edit->max_column)
		    edit->max_column = x;
		return;
	    case '\t':
		if (fixed_font) {
		    int t;
		    t = next_tab_pos (x);
		    t = min (t, x_max);
		    s = *p;
		    while (x < t) {
			x += per_char[' '];
			*p++ = s | ' ';
		    }
		} else {
		    *p++ |= '\t';
		    x = next_tab_pos (x);
		}
		break;
	    default:
		x += convert_to_long_printable (c, text);
		r = text;
		s = *p;
		*p++ = s | *r++;
		if (!*r)
		    break;
		*p++ = s | *r++;
		if (!*r)
		    break;
		*p++ = s | *r++;
		break;
	    }
	    if (x >= x_max)
		break;
	    q++;
	}
    } else {
	for (;;) {
	    c = edit_get_byte (edit, q);
	    *p = get_style (edit, q, c, m1, m2, x);
	    switch (c) {
	    case '\n':
		*p++ |= ' ';
		*p = 0;
		if (x > edit->max_column)
		    edit->max_column = x;
		return;
	    case '\t':
		if (fixed_font) {
		    int t;
		    t = next_tab_pos (x);
		    t = min (t, x_max);
		    s = *p;
		    while (x < t) {
			x += per_char[' '];
			*p++ = s | ' ';
		    }
		} else {
		    *p++ |= '\t';
		    x = next_tab_pos (x);
		}
		break;
	    default:
		x += convert_to_long_printable (c, text);
		r = text;
		s = *p;
		*p++ = s | *r++;
		if (!*r)
		    break;
		*p++ = s | *r++;
		if (!*r)
		    break;
		*p++ = s | *r++;
		break;
	    }
	    if (x >= x_max)
		break;
	    q++;
	}
    }
    if (x > edit->max_column)
	edit->max_column = x;
    *p = 0;
}

void edit_set_cursor (Window win, int x, int y, int bg, int fg, int width, char t)
{
#ifdef GTK
    gdk_gc_set_foreground (win->gc, &win->color[18]);
    gdk_draw_rectangle (win->text_area, win->gc, 0, x, y + FONT_OVERHEAD, width - 1, FONT_HEIGHT - 1);
#else
    CSetColor (edit_cursor_color);
    CLine (win, x, y + FONT_OVERHEAD,
	   x, y + FONT_HEIGHT - 1);	/* non focussed cursor form */
    CLine (win, x + 1, y + FONT_OVERHEAD,
	   x + width - 1, y + FONT_OVERHEAD);
    set_cursor_position (win, x, y, width, FONT_HEIGHT, CURSOR_TYPE_EDITOR, t, bg, fg);	/* widget library's flashing cursor */
#endif
}

static inline int next_tab (int x, int scroll_right)
{
    return next_tab_pos (x - scroll_right - EDIT_TEXT_HORIZONTAL_OFFSET) - x + scroll_right + EDIT_TEXT_HORIZONTAL_OFFSET;
}

int draw_tab (Window win, int x, int y, cache_type s, int scroll_right)
{
    int l;
#ifdef GTK
    GdkColor fg, bg;
#else
    unsigned long fg, bg;
#endif
    l = next_tab (x, scroll_right);
#ifdef GTK
    set_style_color (s, &fg.pixel, &bg.pixel);
    gdk_gc_set_foreground (win->gc, &bg);
    gdk_draw_rectangle (win->text_area, win->gc, 1, x, y + FONT_OVERHEAD, l, FONT_HEIGHT);
/* if we printed a cursor: */
    if (s & (MOD_CURSOR * 256))
	edit_set_cursor (win, x, y, bg.pixel, fg.pixel, per_char[' '], ' ');
#else
    set_style_color (s, &fg, &bg);
    CSetColor (bg);
    CRectangle (win, x, y + FONT_OVERHEAD, l, FONT_HEIGHT);
/* if we printed a cursor: */
    if (s & (MOD_CURSOR * 256))
	edit_set_cursor (win, x, y, bg, fg, per_char[' '], ' ');
#endif
    return x + l;
}

#ifdef GTK
#include <gdk/gdk.h>
#endif

static inline void draw_space (Window win, int x, int y, cache_type s, int l)
{
#ifdef GTK
    GdkColor fg, bg;
#else
    unsigned long fg, bg;
#endif
#ifdef GTK
    set_style_color (s, &fg.pixel, &bg.pixel);
    gdk_gc_set_foreground (win->gc, &bg);
    gdk_draw_rectangle (win->text_area, win->gc, 1, x, y + FONT_OVERHEAD, l, FONT_HEIGHT);
#else
    set_style_color (s, &fg, &bg);
    CSetColor (bg);
    CRectangle (win, x, y + FONT_OVERHEAD, l, FONT_HEIGHT);
/* if we printed a cursor: */
    if (s & (MOD_CURSOR * 256))
	edit_set_cursor (win, x, y, bg, fg, per_char[' '], ' ');
#endif
}

#ifdef GTK

void
gdk_draw_image_text (GdkDrawable *drawable,
	       GdkFont     *font,
	       GdkGC       *gc,
	       gint         x,
	       gint         y,
	       const gchar *text,
	       gint         text_length)
{
  GdkWindowPrivate *drawable_private;
  GdkFontPrivate *font_private;
  GdkGCPrivate *gc_private;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (font != NULL);
  g_return_if_fail (gc != NULL);
  g_return_if_fail (text != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  gc_private = (GdkGCPrivate*) gc;
  font_private = (GdkFontPrivate*) font;

  if (font->type == GDK_FONT_FONT)
    {
      XFontStruct *xfont = (XFontStruct *) font_private->xfont;
      XSetFont(drawable_private->xdisplay, gc_private->xgc, xfont->fid);
      if ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0))
	{
	  XDrawImageString (drawable_private->xdisplay, drawable_private->xwindow,
		       gc_private->xgc, x, y, text, text_length);
	}
      else
	{
	  XDrawImageString16 (drawable_private->xdisplay, drawable_private->xwindow,
			 gc_private->xgc, x, y, (XChar2b *) text, text_length / 2);
	}
    }
  else if (font->type == GDK_FONT_FONTSET)
    {
      XFontSet fontset = (XFontSet) font_private->xfont;
      XmbDrawImageString (drawable_private->xdisplay, drawable_private->xwindow,
		     fontset, gc_private->xgc, x, y, text, text_length);
    }
  else
    g_error("undefined font type\n");
}


#endif

int draw_string (Window win, int x, int y, cache_type s, unsigned char *text, int length)
{
#ifdef GTK
    GdkColor fg, bg;
#else
    unsigned long fg, bg;
    int underlined, l;
#endif
#ifdef GTK
    set_style_color (s, &fg.pixel, &bg.pixel);
    gdk_gc_set_background (win->gc, &bg);
    gdk_gc_set_foreground (win->gc, &fg);
    gdk_draw_image_text (win->text_area, GTK_WIDGET (win)->style->font, win->gc, x + FONT_OFFSET_X, y + FONT_OFFSET_Y, text, length);
#else
    underlined = set_style_color (s, &fg, &bg);
    CSetBackgroundColor (bg);
    CSetColor (fg);
    CImageString (win, x + FONT_OFFSET_X, y + FONT_OFFSET_Y, (char *) text, length);
    l = CTextWidth (win, (char *) text, length);
    if (underlined) {
	int i, h, inc;
	inc = FONT_MEAN_WIDTH * 2 / 3;
	CSetColor (color_palette (18));
	h = (x / inc) & 1;
	CLine (win, x, y + FONT_HEIGHT + FONT_OVERHEAD - 1 - h, x + min (l, inc - (x % inc) - 1), y + FONT_HEIGHT + FONT_OVERHEAD - 1 - h);
	h = h ^ 1;
	for (i = inc - min (l, (x % inc)); i < l; i += inc) {
	    CLine (win, x + i, y + FONT_HEIGHT + FONT_OVERHEAD - 1 - h, x + min (l, i + inc - 1), y + FONT_HEIGHT + FONT_OVERHEAD - 1 - h);
	    h = h ^ 1;
	}
    }
#endif
/* if we printed a cursor: */
#ifdef GTK
    if (s & (MOD_CURSOR * 256))
	edit_set_cursor (win, x, y, bg.pixel, fg.pixel, per_char[*text], *text);
    return x + gdk_text_width (GTK_WIDGET (win)->style->font, text, length);
#else
    if (s & (MOD_CURSOR * 256))
	edit_set_cursor (win, x, y, bg, fg, per_char[*text], *text);
    return x + l;
#endif
}

#define STYLE_DIFF (*cache != *line \
	    || ((*cache | *line) & (MOD_CURSOR * 256)) \
	    || !*cache || !*line)

int get_ignore_length (cache_type *cache, cache_type *line)
{
    int i;
    for (i = 0; i < CACHE_WIDTH; i++, line++, cache++) {
	if (STYLE_DIFF)
	    return i;
    }
    return CACHE_WIDTH;
}

static inline size_t lwstrnlen (const cache_type *s, size_t count)
{
    const cache_type *sc;
    for (sc = s; count-- && *sc != 0; ++sc);
    return sc - s;
}

static inline size_t lwstrlen (const cache_type *s)
{
    const cache_type *sc;
    for (sc = s; *sc != 0; ++sc);
    return sc - s;
}

int get_ignore_trailer (cache_type *cache, cache_type *line, int length)
{
    int i;
    int cache_len, line_len;
    cache_len = lwstrnlen (cache, CACHE_WIDTH);
    line_len = lwstrlen (line);

    if (line_len > cache_len)
	for (i = line_len - 1; i >= cache_len && i >= length; i--)
	    if (line[i] != ' ')
		return i + 1;

    for (i = cache_len - 1, line = line + i, cache = cache + i; i > length; i--, line--, cache--)
	if (STYLE_DIFF)
	    return i + 1;

    return length + 1;
}

/* erases trailing bit of old line if a new line is printed over a longer old line */
static void cover_trail (Window win, int x_start, int x_new, int x_old, int y)
{
    if (x_new < EDIT_TEXT_HORIZONTAL_OFFSET)
	x_new = EDIT_TEXT_HORIZONTAL_OFFSET;
    if (x_new < x_old) {	/* no need to print */
#ifdef GTK
	gdk_gc_set_foreground (win->gc, &win->color[1]);
	gdk_draw_rectangle (win->text_area, win->gc, 1, x_new, y + FONT_OVERHEAD, x_old - x_new, FONT_HEIGHT);
#else
	CSetColor (edit_normal_background_color);
	CRectangle (win, x_new, y + FONT_OVERHEAD, x_old - x_new, FONT_HEIGHT + (FONT_OVERHEAD != 0 && !fixed_font));
#endif
    } else {
#ifdef GTK
	gdk_gc_set_foreground (win->gc, &win->color[1]);
#else
	CSetColor (edit_normal_background_color);
#endif
    }
/* true type fonts print stuff out of the bounding box (aaaaaaaaarrrgh!!) */
    if (!fixed_font)
	if (FONT_OVERHEAD && x_new > EDIT_TEXT_HORIZONTAL_OFFSET)
#ifdef GTK
	    gdk_draw_line (win->text_area, win->gc, max (x_start, EDIT_TEXT_HORIZONTAL_OFFSET), y + FONT_HEIGHT + FONT_OVERHEAD, x_new - 1, y + FONT_HEIGHT + FONT_OVERHEAD);
#else
	    CLine (win, max (x_start, EDIT_TEXT_HORIZONTAL_OFFSET), y + FONT_HEIGHT + FONT_OVERHEAD, x_new - 1, y + FONT_HEIGHT + FONT_OVERHEAD);
#endif
}

cache_type mode_spacing = 0;

#define NOT_VALID (-2000000000)

void edit_draw_proportional (void *data,
	   void (*converttext) (void *, long, cache_type *, int, int, int),
	   int calctextpos (void *, long, long *, int),
				int scroll_right,
				Window win,
				int x_max,
				long b,
				int row,
				int y,
				int x_offset,
				int tabwidth)
{
    static struct cache_line lines[CACHE_HEIGHT];
    static Window last = 0;
    cache_type style, line[MAX_LINE_LEN], *p;
    unsigned char text[128];
    int x0, x, ignore_text = 0, ignore_trailer = 2000000000, j, i;
    long q;

    tab_width = tabwidth;
    if (option_long_whitespace)
	tab_width = tabwidth *= 2;

    x_max -= 3;

/* if its not the same window, reset the screen rememberer */
    if (last != win) {
	last = win;
	for (i = 0; i < CACHE_HEIGHT; i++) {
	    lines[i].x0 = NOT_VALID;
	    lines[i].x1 = x_max;
	}
    }
/* get point to start drawing */
    x0 = (*calctextpos) (data, b, &q, -scroll_right + x_offset);
/* q contains the offset in the edit buffer */

/* translate this line into printable characters with a style (=color) high byte */
    (*converttext) (data, q, line, x0, x_max - scroll_right - EDIT_TEXT_HORIZONTAL_OFFSET, row);

/* adjust for the horizontal scroll and border */
    x0 += scroll_right + EDIT_TEXT_HORIZONTAL_OFFSET;
    x = x0;

/* is some of the line identical to that already printed so that we can ignore it? */
    if (!EditExposeRedraw) {
	if (lines[row].x0 == x0 && row < CACHE_HEIGHT) {	/* i.e. also  && lines[row].x0 != NOT_VALID */
	    ignore_text = get_ignore_length (lines[row].data, line);
	    if (fixed_font)
		ignore_trailer = get_ignore_trailer (lines[row].data, line, ignore_text);
	}
    }
    p = line;
    j = 0;
    while (*p) {
	if (mode_spacing) {
	    if ((*p & 0x80) && (*p & mode_spacing)) {
#ifdef STILL_TO_BE_SUPPORTED
		x += edit_insert_pixmap (win, x, y, *p & 0x7F);
		/* the pixmap will be clipped, if it's taller than the
		   current font, else centred top to bottom */
#endif
		p++;
		continue;
	    }
	    goto do_text;
	}
	if ((*p & 0xFF) == '\t') {
	    j++;
	    if (j > ignore_text && j < ignore_trailer + 1)
		x = draw_tab (win, x, y, *p, scroll_right);
	    else
		x += next_tab (x, scroll_right);
	    p++;
	} else {
	  do_text:
	    style = *p & 0xFFFFFF00UL;
	    i = 0;
	    do {
		text[i++] = (unsigned char) *p++;
		j++;
		if (j == ignore_text || j == ignore_trailer)
		    break;
	    } while (i < 128 && *p && style == (*p & 0xFFFFFF00UL) && (*p & 0xFF) != '\t');

	    if (style & mode_spacing) {
		int k;
		for (k = 0; k < i; k++) {
		    draw_space (win, x, y, (0xFFFFFF00UL - mode_spacing) & style, text[k]);
		    x += text[k];
		}
	    } else {
		if (j > ignore_text && j < ignore_trailer + 1)
		    x = draw_string (win, x, y, style, text, i);
		else
#ifdef GTK
		    x += gdk_text_width (GTK_WIDGET(win)->style->font, text, i);
#else
		    x += CTextWidth (win, (char *) text, i);
#endif
	    }
	}
    }

    x = min (x, x_max);
    if (!EditExposeRedraw || EditClear)
	cover_trail (win, x0, x, lines[row].x1, y);
    memcpy (&(lines[row].data[ignore_text]),
	    &(line[ignore_text]),
	 (min (j, CACHE_WIDTH) - ignore_text) * sizeof (cache_type));

    lines[row].data[min (j, CACHE_WIDTH)] = 0;

    lines[row].x0 = x0;
    lines[row].x1 = x;
    if (EditExposeRedraw)
	last = 0;
    else
	last = win;
}


void edit_draw_this_line_proportional (WEdit * edit, long b, int row, int start_column, int end_column)
{
    int fg, bg;
    if (row < 0 || row >= edit->num_widget_lines)
	return;

    if (row + edit->start_line > edit->total_lines)
	b = edit->last_byte + 1;		/* force b out of range of the edit buffer for blanks lines */

    if (end_column > CWidthOf (edit->widget))
	end_column = CWidthOf (edit->widget);

    edit_get_syntax_color (edit, b - 1, &fg, &bg);

    edit_draw_proportional (edit,
			    (void (*) (void *, long, cache_type *, int, int, int)) convert_text,
			    (int (*) (void *, long, long *, int)) calc_text_pos,
			    edit->start_col, CWindowOf (edit->widget),
			    end_column, b, row, row * FONT_PIX_PER_LINE + EDIT_TEXT_VERTICAL_OFFSET,
			    EditExposeRedraw ? start_column : 0, per_char[' '] * TAB_SIZE);
}


/*********************************************************************************/
/*         The remainder is for the text box widget                              */
/*********************************************************************************/

static inline int nroff_printable (int c)
{
    return isprint (c);
}


int calc_text_pos_str (unsigned char *text, long b, long *q, int l)
{
    int x = 0, c = 0, xn = 0, d;
    for (;;) {
	d = c;
	c = text[b];
	switch (c) {
	case '\0':
	case '\n':
	    *q = b;
	    return x;
	case '\t':
	    xn = next_tab_pos (x);
	    break;
	case '\r':
	    break;
	case '\b':
	    if (d)
		xn = x - per_char[d];
	    break;
	default:
	    if (!nroff_printable (c))
		c = ' ';
	    xn = x + per_char[c];
	    break;
	}
	if (xn > l)
	    break;
	x = xn;
	b++;
    }
    *q = b;
    return x;
}

int prop_font_strcolmove (unsigned char *str, int i, int column)
{
    long q;
    calc_text_pos_str (str, i, &q, column * FONT_MEAN_WIDTH);
    return q;
}

#ifndef GTK

/* b is the beginning of the line. l is the length in pixels up to a point 
   on some character which is unknown. The character pos is returned in
   *q and the characters pixel x pos from b is return'ed. */
int calc_text_pos2 (CWidget * w, long b, long *q, int l)
{
    return calc_text_pos_str ((unsigned char *) w->text, b, q, l);
}

/* calcs pixel length of the line beginning at b up to upto */
int calc_text_len2 (CWidget *w, long b, long upto)
{
    int x = 0, c = 0, d;
    for (;;) {
	if (b == upto)
	    return x;
	d = c;
	c = w->text[b];
	switch (c) {
	case '\n':
	case '\0':
	    return x;
	case '\t':
	    x = next_tab_pos (x);
	    break;
	case '\r':
	    break;
	case '\b':
	    if (d)
		x -= per_char[d];
	    break;
	default:
	    if (!nroff_printable (c))
		c = ' ';
	    x += per_char[c];
	    break;
	}
	b++;
    }
}


int highlight_this_line;

/* this is for the text widget (i.e. nroff formatting) */
void convert_text2 (CWidget * w, long q, cache_type *line, int x, int x_max, int row)
{
    int c = 0, d;
    cache_type s, *p;
    long m1, m2;

    m1 = min (w->mark1, w->mark2);
    m2 = max (w->mark1, w->mark2);

    p = line;
    *p = 0;
    for (;;) {
	d = c;
	c = w->text[q];
	*(p + 1) = 0;
	*p |= 0xFFFF0000UL;	/* default background colors */
	if (highlight_this_line)
	    *p |= MOD_HIGHLIGHTED * 256;
	if (q >= m1 && q < m2)
	    *p |= MOD_MARKED * 256;
	switch (c) {
	case '\0':
	case '\n':
	    *p++ |= ' ';
	    if (highlight_this_line) {
	        q--;
		x += per_char[' '];
	    } else
	        return;
	    break;
	case '\t':
	    if (fixed_font) {
		int i;
		i = next_tab_pos (x) - x;
		x += i;
		s = *p;
		while (i > 0) {
		    i -= per_char[' '];
		    *p++ = s | ' ';
		    *p = 0;
		}
	    } else {
		*p++ |= '\t';
		x = next_tab_pos (x);
	    }
	    break;
	case '\r':
	    break;
	case '\b':
	    if (d) {
		--p;
		x -= per_char[d];
		if (d == '_')
		    *p |= MOD_ITALIC * 256;
		else
		    *p |= MOD_BOLD * 256;
	    }
	    break;
	default:
	    if (!nroff_printable (c)) {
		c = ' ';
		*p |= MOD_ABNORMAL * 256;
	    }
	    x += per_char[c];
	    *p &= 0xFFFFFF00UL;
	    *p |= c;
	    p++;
	    break;
	}
	if (x > x_max)
	    break;
	q++;
    }
    *p = 0;
}


#endif



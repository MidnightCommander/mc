/* Module for creating a shaped window with text (for desktop icons)
 *
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <gnome.h>
#include <string.h>
#include "gdesktop.h"
#include "gcache.h"
#include <gdk/gdkx.h>

/* The spacing between the cute little icon and the text */
#define SPACING 2

int want_transparent_icons = 1;
int want_transparent_text = 0;


/* Most of the word-wrapping code was yanked from the gtktooltips
 * module in Gtk+.  I have tweaked it a bit for MC's purposes
 * - Federico
 */

struct text_info {
	GList *rows;
	GdkFont *font;
	int width;
	int height;
	int baseline_skip;
};


static void
free_string (gpointer data, gpointer user_data)
{
	if (data)
		g_free (data);
}

static void
text_info_free (struct text_info *ti)
{
	g_list_foreach (ti->rows, free_string, NULL);
	g_list_free (ti->rows);
	g_free (ti);
}

static struct text_info *
layout_text (GtkWidget *widget, char *text)
{
	struct text_info *ti;
	char *row_end, *row_text, *break_pos;
	int i, row_width, window_width;
	size_t len;

	ti = g_new (struct text_info, 1);

	ti->rows = NULL;
	ti->font = widget->style->font;
	ti->width = 0;
	ti->height = 0;

	ti->baseline_skip = ti->font->ascent + ti->font->descent;

	window_width = 0;

	while (*text) {
		row_end = strchr (text, '\n');
		if (!row_end)
			row_end = strchr (text, '\0');

		len = row_end - text + 1;
		row_text = g_new (char, len);
		memcpy (row_text, text, len - 1);
		row_text[len - 1] = '\0';

		/* Adjust the window's width or shorten the row until
		 * it fits in the window.
		 */

		while (1) {
			row_width = gdk_string_width (ti->font, row_text);
			if (!window_width) {
				/* make an initial guess at window's width */

				if (row_width > SNAP_X)
					window_width = SNAP_X;
				else
					window_width = row_width;
			}

			if (row_width <= window_width)
				break;

			if (strchr (row_text, ' ')) {
				/* the row is currently too wide, but we have
				 * blanks in the row so we can break it into
				 * smaller pieces
				 */

				int avg_width = row_width / strlen (row_text);

				i = window_width;

				if (avg_width != 0)
					i /= avg_width;

				if ((size_t) i >= len)
					i = len - 1;

				break_pos = strchr (row_text + i, ' ');
				if (!break_pos) {
					break_pos = row_text+ i;
					while (*--break_pos != ' ');
				}

				*break_pos = '\0';
			} else {
				/* We can't break this row into any smaller
				 * pieces, so we have no choice but to widen
				 * the window
				 *
				 * For MC, we may want to modify the code above
				 * so that it can also split the string on the
				 * slahes of filenames.
				 */

				window_width = row_width;
				break;
			}
		}

		if (row_width > ti->width)
			ti->width = row_width;

		ti->rows = g_list_append (ti->rows, row_text);
		ti->height += ti->baseline_skip;

		text += strlen (row_text);
		if (!*text)
			break;

		if (text[0] == '\n' && text[1]) {
			/* end of paragraph and there is more text to come */
			ti->rows = g_list_append (ti->rows, NULL);
			ti->height += ti->baseline_skip / 2;
		}

		text++; /* skip blank or newline */
	}

	return ti;
}

static void
paint_text (struct text_info *ti, GdkDrawable *drawable, GdkGC *gc, int x_ofs, int y_ofs)
{
	int y, w;
	GList *item;

	y = y_ofs + ti->font->ascent;

	for (item = ti->rows; item; item = item->next) {
		if (item->data) {
			w = gdk_string_width (ti->font, item->data);
			gdk_draw_string (drawable, ti->font, gc, x_ofs + (ti->width - w) / 2, y, item->data);
			y += ti->baseline_skip;
		} else
			y += ti->baseline_skip / 2;
	}
}

static void
set_window_text (GtkWidget *window, GdkImlibImage *im, char *text)
{
	GdkPixmap *pixmap;
	GdkPixmap *im_pixmap;
	GdkBitmap *mask;
	GdkBitmap *im_mask;
	struct text_info *ti;
	GdkColor color;
	GdkGC *p_gc, *m_gc;
	int width, height;

	ti = layout_text (window, text);

	width = MAX (ti->width, im->rgb_width);
	height = im->rgb_height + SPACING + ti->height;

	/* Create pixmap, mask, and gc's */

	pixmap = gdk_pixmap_new (window->window, width, height, gdk_imlib_get_visual ()->depth);
	mask = gdk_pixmap_new (window->window, width, height, 1);

	p_gc = gdk_gc_new (pixmap);
	m_gc = gdk_gc_new (mask);

	/* Fill mask with transparent */

	color.pixel = 0;
	gdk_gc_set_foreground (m_gc, &color);
	gdk_draw_rectangle (mask,
			    m_gc,
			    TRUE,
			    0, 0,
			    width, height);

	/* Icon */

	im_pixmap = gdk_imlib_move_image (im);
	im_mask = gdk_imlib_move_mask (im);

	if (!want_transparent_icons) {
		/* black background */

		gdk_color_black (gdk_imlib_get_colormap (), &color);
		gdk_gc_set_foreground (p_gc, &color);
		gdk_draw_rectangle (pixmap,
				    p_gc,
				    TRUE,
				    (width - im->rgb_width) / 2,
				    0,
				    im->rgb_width,
				    im->rgb_height);

		/* opaque mask */

		color.pixel = 1;
		gdk_gc_set_foreground (m_gc, &color);
		gdk_draw_rectangle (mask,
				    m_gc,
				    TRUE,
				    (width - im->rgb_width) / 2,
				    0,
				    im->rgb_width,
				    im->rgb_height);
	} else if (im_mask)
		gdk_draw_pixmap (mask,
				 m_gc,
				 im_mask,
				 0, 0,
				 (width - im->rgb_width) / 2,
				 0,
				 im->rgb_width,
				 im->rgb_height);

	if (im_mask) {
		gdk_gc_set_clip_mask (p_gc, im_mask);
		gdk_gc_set_clip_origin (p_gc, (width - im->rgb_width) / 2, 0);
	}

	gdk_draw_pixmap (pixmap,
			 p_gc,
			 im_pixmap,
			 0, 0,
			 (width - im->rgb_width) / 2,
			 0,
			 im->rgb_width,
			 im->rgb_height);

	if (im_mask) {
		gdk_gc_set_clip_mask (p_gc, NULL);
		gdk_imlib_free_bitmap (im_mask);
	}

	gdk_imlib_free_pixmap (im_pixmap);
	
	/* Text */

	if (!want_transparent_text) {
		/* black background */

		gdk_color_black (gdk_imlib_get_colormap (), &color);
		gdk_gc_set_foreground (p_gc, &color);
		gdk_draw_rectangle (pixmap,
				    p_gc,
				    TRUE,
				    (width - ti->width) / 2,
				    im->rgb_height + SPACING,
				    ti->width,
				    ti->height);

		/* opaque mask */

		color.pixel = 1;
		gdk_gc_set_foreground (m_gc, &color);
		gdk_draw_rectangle (mask,
				    m_gc,
				    TRUE,
				    (width - ti->width) / 2,
				    im->rgb_height + SPACING,
				    ti->width,
				    ti->height);
	} else {
		color.pixel = 1;
		gdk_gc_set_foreground (m_gc, &color);
		paint_text (ti, mask, m_gc,
			    (width - ti->width) / 2,
			    im->rgb_height + SPACING);
	}
	
	gdk_color_white (gdk_imlib_get_colormap (), &color);
	gdk_gc_set_foreground (p_gc, &color);
	paint_text (ti, pixmap, p_gc,
		    (width - ti->width) / 2,
		    im->rgb_height + SPACING);

	/* Set contents of window */

	gtk_widget_set_usize (window, width, height);
	gdk_window_set_back_pixmap (window->window, pixmap, FALSE);
	gdk_window_shape_combine_mask (window->window, mask, 0, 0);

	gdk_gc_destroy (p_gc);
	gdk_gc_destroy (m_gc);

	gdk_pixmap_unref (pixmap);
	gdk_pixmap_unref (mask);

	text_info_free (ti);
}

GtkWidget *
create_transparent_text_window (char *file, char *text, int extra_events)
{
	GtkWidget *window;
	GdkImlibImage *im;
	GdkCursor *cursor;

	if (!g_file_exists (file))
		return NULL;

	im = image_cache_load_image (file);
	if (!im)
		return NULL;

	gtk_widget_push_visual (gdk_imlib_get_visual ());
	gtk_widget_push_colormap (gdk_imlib_get_colormap ());
	
	window = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_widget_set_events (window, gtk_widget_get_events (window) | extra_events);

	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();

	gtk_widget_realize (window);
	
	gdk_imlib_render (im, im->rgb_width, im->rgb_height);

	set_window_text (window, im, text);

	cursor = gdk_cursor_new (GDK_TOP_LEFT_ARROW);
	gdk_window_set_cursor (window->window, cursor);
	gdk_cursor_destroy (cursor);

	return window;
}

GtkWidget *
make_transparent_window (char *file)
{
	GdkImlibImage *im;
	GtkWidget *window;
	XSetWindowAttributes xwa;
	
	if (!g_file_exists (file))
		return NULL;

	im = image_cache_load_image (file);
	if (!im)
		return NULL;

	gtk_widget_push_visual (gdk_imlib_get_visual ());
	gtk_widget_push_colormap (gdk_imlib_get_colormap ());

	window = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();

	gtk_widget_realize (window);

	xwa.save_under = True;
	XChangeWindowAttributes (GDK_WINDOW_XDISPLAY (window->window),
				 GDK_WINDOW_XWINDOW (window->window),
				 CWSaveUnder, &xwa);

	gtk_widget_set_usize (window, im->rgb_width, im->rgb_height);

	
	/* All of the following 3 lines should not be required, only
	 * gdk_imlib_apply_image, but is is buggy.
	 */
	gdk_imlib_render (im, im->rgb_width, im->rgb_height);
	gdk_window_set_back_pixmap (window->window, gdk_imlib_move_image (im), FALSE);
	gdk_window_shape_combine_mask (window->window, gdk_imlib_move_mask (im), 0, 0);
	
	return window;
}

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


static void
set_window_text (GtkWidget *window, GdkImlibImage *im, char *text)
{
	GdkPixmap *pixmap;
	GdkPixmap *im_pixmap;
	GdkBitmap *mask;
	GdkBitmap *im_mask;
	GnomeIconTextInfo *ti;
	GdkColor color;
	GdkGC *p_gc, *m_gc;
	int width, height;

	ti = gnome_icon_layout_text (window->style->font, text, " /.-_", SNAP_X, FALSE);

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

	if (!want_transparent_icons || im_mask == NULL) {
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
		gnome_icon_paint_text (ti, mask, m_gc,
				       (width - ti->width) / 2,
				       im->rgb_height + SPACING,
				       GTK_JUSTIFY_CENTER);
	}
	
	gdk_color_white (gdk_imlib_get_colormap (), &color);
	gdk_gc_set_foreground (p_gc, &color);
	gnome_icon_paint_text (ti, pixmap, p_gc,
			       (width - ti->width) / 2,
			       im->rgb_height + SPACING,
			       GTK_JUSTIFY_CENTER);

	/* Set contents of window */

	gtk_widget_set_usize (window, width, height);
	gdk_window_set_back_pixmap (window->window, pixmap, FALSE);
	gdk_window_shape_combine_mask (window->window, mask, 0, 0);

	gdk_gc_destroy (p_gc);
	gdk_gc_destroy (m_gc);

	gdk_pixmap_unref (pixmap);
	gdk_pixmap_unref (mask);

	gnome_icon_text_info_free (ti);
}

static void
lower_icon_window(GtkWidget *widget, GdkEventExpose *event)
{
	gdk_window_lower(widget->window);
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

#if 0
	/* Do not enable this code */
	/* We do this so the desktop icons appear to really be part of the
	   desktop */

        /* This is wrong.  If you have two icons, one on top of the other,
	 * guess what is the result.
	 */
	gtk_signal_connect(window, "expose_event", GTK_SIGNAL_FUNC(lower_icon_window), NULL);
#endif
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

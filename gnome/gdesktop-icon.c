/* Desktop icon widget for the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include <gnome.h>
#include "desktop-icon.h"
#include "gdesktop.h"


/* Spacing between icon and text */
#define SPACING 2


static void desktop_icon_class_init (DesktopIconClass *class);
static void desktop_icon_init       (DesktopIcon      *dicon);
static void desktop_icon_realize    (GtkWidget        *widget);


static GtkWindowClass *parent_class;


GtkType
desktop_icon_get_type (void)
{
	static GtkType desktop_icon_type = 0;

	if (!desktop_icon_type) {
		GtkTypeInfo desktop_icon_info = {
			"DesktopIcon",
			sizeof (DesktopIcon),
			sizeof (DesktopIconClass),
			(GtkClassInitFunc) desktop_icon_class_init,
			(GtkObjectInitFunc) desktop_icon_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		desktop_icon_type = gtk_type_unique (gtk_window_get_type (), &desktop_icon_info);
	}

	return desktop_icon_type;
}

static void
desktop_icon_class_init (DesktopIconClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (gtk_window_get_type ());

	widget_class->realize = desktop_icon_realize;
}

/* Computes and sets a new window shape for the desktop icon */
static void
create_window_shape (DesktopIcon *dicon, int icon_width, int icon_height, int text_width, int text_height)
{
	GdkImlibImage *im;
	GdkBitmap *mask;
	GdkBitmap *im_mask;
	GdkGC *mgc;
	GdkColor c;

	/* Create the initial mask and clear it */

	mask = gdk_pixmap_new (GTK_WIDGET (dicon)->window, dicon->width, dicon->height, 1);

	mgc = gdk_gc_new (mask);
	c.pixel = 0;
	gdk_gc_set_foreground (mgc, &c);
	gdk_draw_rectangle (mask, mgc, TRUE, 0, 0, dicon->width, dicon->height);

	c.pixel = 1;
	gdk_gc_set_foreground (mgc, &c);

	/* Paint the mask of the image */

	im = GNOME_CANVAS_IMAGE (dicon->icon)->im;
	gdk_imlib_render (im, im->rgb_width, im->rgb_height);
	im_mask = gdk_imlib_move_mask (im);

	if (im_mask) {
		gdk_draw_pixmap (mask,
				 mgc,
				 im_mask,
				 0, 0,
				 (dicon->width - icon_width) / 2, 0,
				 icon_width, icon_height);
		gdk_imlib_free_bitmap (im_mask);
	} else
		gdk_draw_rectangle (mask, mgc, TRUE,
				    (dicon->width - icon_width) / 2, 0,
				    icon_width, icon_height);

	/* Fill the area for the text */

	gdk_draw_rectangle (mask, mgc, TRUE,
			    (dicon->width - text_width) / 2,
			    icon_height + SPACING,
			    text_width, text_height);

	if (!GTK_WIDGET_REALIZED (dicon))
		gtk_widget_realize (GTK_WIDGET (dicon));

	gtk_widget_shape_combine_mask (GTK_WIDGET (dicon), mask, 0, 0);
	gdk_bitmap_unref (mask);
	gdk_gc_unref (mgc);
}

/* Resets the positions of the desktop icon's child items and recomputes the window's shape mask */
static void
reshape (DesktopIcon *dicon)
{
	GtkArg args[2];
	int icon_width, icon_height;
	int text_width, text_height;
	double x1, y1, x2, y2;

	/* Get size of icon image */

	args[0].name = "width";
	args[1].name = "height";
	gtk_object_getv (GTK_OBJECT (dicon->icon), 2, args);
	icon_width = GTK_VALUE_DOUBLE (args[0]);
	icon_height = GTK_VALUE_DOUBLE (args[0]);

	/* Get size of icon text */

	gnome_canvas_item_get_bounds (dicon->text, &x1, &y1, &x2, &y2);

	text_width = x2 - x1 + 1;
	text_height = y2 - y1 + 1;

	/* Calculate new size of widget */

	dicon->width = MAX (icon_width, text_width);
	dicon->height = icon_height + SPACING + text_height;

	/* Set new position of children */

	gnome_canvas_item_set (dicon->icon,
			       "x", (dicon->width - icon_width) / 2.0,
			       "y", 0.0,
			       NULL);

	gnome_icon_text_item_setxy (GNOME_ICON_TEXT_ITEM (dicon->text),
				    (dicon->width - text_width) / 2,
				    icon_height + SPACING);

	/* Create and set the window shape */

	gtk_widget_set_usize (GTK_WIDGET (dicon), dicon->width, dicon->height);
	create_window_shape (dicon, icon_width, icon_height, text_width, text_height);
}

/* Callback used when the size of the icon text item changes */
static void
size_changed (GnomeIconTextItem *text, gpointer data)
{
	DesktopIcon *dicon;

	dicon = DESKTOP_ICON (data);

	reshape (dicon);
}

static void
desktop_icon_init (DesktopIcon *dicon)
{
	/* Create the canvas */

	gtk_widget_push_visual (gdk_imlib_get_visual ());
	gtk_widget_push_colormap (gdk_imlib_get_colormap ());

	dicon->canvas = gnome_canvas_new ();

	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();

	gtk_container_add (GTK_CONTAINER (dicon), dicon->canvas);
	gtk_widget_show (dicon->canvas);

	/* Create the icon and the text items */

	dicon->icon = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (dicon->canvas)),
					     gnome_canvas_image_get_type (),
					     "anchor", GTK_ANCHOR_NW,
					     NULL);

	dicon->text = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (dicon->canvas)),
					     gnome_icon_text_item_get_type (),
					     NULL);
	dicon->w_changed_id = gtk_signal_connect (GTK_OBJECT (dicon->text), "width_changed",
						  (GtkSignalFunc) size_changed,
						  dicon);
	dicon->h_changed_id = gtk_signal_connect (GTK_OBJECT (dicon->text), "height_changed",
						  (GtkSignalFunc) size_changed,
						  dicon);
}

static void
desktop_icon_realize (GtkWidget *widget)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_DESKTOP_ICON (widget));

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

	/* Set the window decorations to none and hints to the appropriate combination */

	gdk_window_set_decorations (widget->window, 0);
	gdk_window_set_functions (widget->window, 0);

	gnome_win_hints_init ();

	if (gnome_win_hints_wm_exists ()) {
		gnome_win_hints_set_layer (widget, WIN_LAYER_DESKTOP);
		gnome_win_hints_set_hints (widget,
					   (WIN_HINTS_SKIP_FOCUS
					    | WIN_HINTS_SKIP_WINLIST
					    | WIN_HINTS_SKIP_TASKBAR));
	}
}

/* Sets the icon from the specified image file.  Does not re-create the window shape for the desktop
 * icon.
 */
static void
set_icon (DesktopIcon *dicon, char *image_file)
{
	GdkImlibImage *im, *old_im;
	GtkArg arg;

	/* Load the new image */

	if (!g_file_exists (image_file))
		return;

	im = gdk_imlib_load_image (image_file);
	if (!im)
		return;

	/* Destroy the old image if it exists */

	arg.name = "image";
	gtk_object_getv (GTK_OBJECT (dicon->icon), 1, &arg);

	old_im = GTK_VALUE_POINTER (arg);

	gnome_canvas_item_set (dicon->icon,
			       "image", im,
			       "width", (double) im->rgb_width,
			       "height", (double) im->rgb_height,
			       NULL);

	if (old_im)
		gdk_imlib_destroy_image (old_im);
}

/* Sets the text in the desktop icon, but does not re-create its window shape.  */
static void
set_text (DesktopIcon *dicon, char *text)
{
	GtkArg arg;
	int icon_width;

	arg.name = "width";
	gtk_object_getv (GTK_OBJECT (dicon->icon), 1, &arg);
	icon_width = GTK_VALUE_DOUBLE (arg);

	gtk_signal_handler_block (GTK_OBJECT (dicon->text), dicon->w_changed_id);
	gtk_signal_handler_block (GTK_OBJECT (dicon->text), dicon->h_changed_id);

	gnome_icon_text_item_configure (GNOME_ICON_TEXT_ITEM (dicon->text),
					0, 0,
					MAX (SNAP_X, icon_width),
					DESKTOP_ICON_FONT,
					text,
					TRUE);

	gtk_signal_handler_unblock (GTK_OBJECT (dicon->text), dicon->w_changed_id);
	gtk_signal_handler_unblock (GTK_OBJECT (dicon->text), dicon->h_changed_id);
}

GtkWidget *
desktop_icon_new (char *image_file, char *text)
{
	DesktopIcon *dicon;

	g_return_val_if_fail (image_file != NULL, NULL);
	g_return_val_if_fail (text != NULL, NULL);

	dicon = gtk_type_new (desktop_icon_get_type ());

	set_icon (dicon, image_file);
	set_text (dicon, text);
	reshape (dicon);

	return GTK_WIDGET (dicon);
}

void
desktop_icon_set_icon (DesktopIcon *dicon, char *image_file)
{
	g_return_if_fail (dicon != NULL);
	g_return_if_fail (IS_DESKTOP_ICON (dicon));
	g_return_if_fail (image_file != NULL);

	set_icon (dicon, image_file);
	reshape (dicon);
}

void
desktop_icon_set_text (DesktopIcon *dicon, char *text)
{
	g_return_if_fail (dicon != NULL);
	g_return_if_fail (IS_DESKTOP_ICON (dicon));
	g_return_if_fail (text != NULL);

	set_text (dicon, text);
	reshape (dicon);
}

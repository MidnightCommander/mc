/* Desktop icon widget for the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include <gnome.h>
#include "gdesktop-icon.h"
#include "gdesktop.h"


/* Spacing between icon and text */
#define SPACING 2


/* The 50% gray stipple for selected icons */
#define gray50_width 2
#define gray50_height 2
static char gray50_bits[] = {
  0x02, 0x01, };


static void desktop_icon_class_init (DesktopIconClass *class);
static void desktop_icon_init       (DesktopIcon      *dicon);
static void desktop_icon_realize    (GtkWidget        *widget);


static GtkWindowClass *parent_class;


/**
 * desktop_icon_get_type
 *
 * Returns the Gtk type assigned to the DesktopIcon class.
 */
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

/* Callback used when the size of the icon text item changes */
static void
size_changed (GnomeIconTextItem *text, gpointer data)
{
	DesktopIcon *dicon;

	dicon = DESKTOP_ICON (data);

	desktop_icon_reshape (dicon);
}

/* Callback used when the desktop icon's canvas is size_allocated.  We reset the canvas scrolling
 * region here, instead of doing it when the window shape changes, to avoid flicker.
 */
static void
canvas_size_allocated (GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
	gnome_canvas_set_scroll_region (GNOME_CANVAS (widget), 0, 0, allocation->width, allocation->height);
}

static void
desktop_icon_init (DesktopIcon *dicon)
{
	GdkBitmap *stipple;

	/* Set the window policy */

	gtk_window_set_policy (GTK_WINDOW (dicon), TRUE, TRUE, TRUE);
	gtk_window_set_wmclass (GTK_WINDOW (dicon), "desktop_icon", "gmc");
	/* Create the canvas */

	dicon->canvas = gnome_canvas_new ();
	gtk_signal_connect (GTK_OBJECT (dicon->canvas), "size_allocate",
			    (GtkSignalFunc) canvas_size_allocated,
			    NULL);

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

	stipple = gdk_bitmap_create_from_data (NULL, gray50_bits, gray50_width, gray50_height);
	dicon->stipple = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (dicon->canvas)),
						gnome_canvas_rect_get_type (),
						"fill_stipple", stipple,
						NULL);
	gnome_canvas_item_hide (dicon->stipple);
	gdk_bitmap_unref (stipple);

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
	DesktopIcon *dicon;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_DESKTOP_ICON (widget));

	dicon = DESKTOP_ICON (widget);

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

	/* Set the window decorations to none and hints to the appropriate combination */

	gdk_window_set_decorations (widget->window, 0);
	gdk_window_set_functions (widget->window, 0);
	
	gnome_win_hints_init ();

	if (gnome_win_hints_wm_exists ()) {
		gnome_win_hints_set_layer (widget, WIN_LAYER_DESKTOP);
		gnome_win_hints_set_state (widget, WIN_STATE_FIXED_POSITION | WIN_STATE_ARRANGE_IGNORE);
		gnome_win_hints_set_hints (widget,
					   (WIN_HINTS_SKIP_FOCUS
					    | WIN_HINTS_SKIP_WINLIST
					    | WIN_HINTS_SKIP_TASKBAR));
	}

	/* Set the stipple color now that we have a style */

	gnome_canvas_item_set (dicon->stipple,
			       "fill_color_gdk", &widget->style->bg[GTK_STATE_SELECTED],
			       NULL);
}

/* Sets the icon from the specified image.  Does not re-create the window shape for the desktop
 * icon.
 */
static void
set_icon (DesktopIcon *dicon, GdkImlibImage *im)
{
	GdkImlibImage *old_im;
	GtkArg arg;

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
	int icon_height;

	arg.name = "height";
	gtk_object_getv (GTK_OBJECT (dicon->icon), 1, &arg);
	icon_height = GTK_VALUE_DOUBLE (arg);

	gtk_signal_handler_block (GTK_OBJECT (dicon->text), dicon->w_changed_id);
	gtk_signal_handler_block (GTK_OBJECT (dicon->text), dicon->h_changed_id);

	gnome_icon_text_item_configure (GNOME_ICON_TEXT_ITEM (dicon->text),
					0, icon_height + SPACING,
					DESKTOP_SNAP_X,
					DESKTOP_ICON_FONT,
					text,
					TRUE);

	gtk_signal_handler_unblock (GTK_OBJECT (dicon->text), dicon->w_changed_id);
	gtk_signal_handler_unblock (GTK_OBJECT (dicon->text), dicon->h_changed_id);
}

/**
 * desktop_icon_new
 * @image:	Imlib image with the icon.
 * @text:	Text to use for the icon.
 *
 * Creates a new desktop icon widget with the specified icon image and text.  The icon has to be
 * positioned at the desired place.
 *
 * Returns the newly-created desktop icon widget.
 */
GtkWidget *
desktop_icon_new (GdkImlibImage *image, char *text)
{
	DesktopIcon *dicon;

	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (text != NULL, NULL);

	dicon = gtk_type_new (desktop_icon_get_type ());

	set_icon (dicon, image);
	set_text (dicon, text);
	desktop_icon_reshape (dicon);

	return GTK_WIDGET (dicon);
}

/**
 * desktop_icon_set_icon
 * @dicon:	The desktop icon to set the icon for
 * @image:	Imlib image with the icon.
 *
 * Sets a new icon for an existing desktop icon.
 */
void
desktop_icon_set_icon (DesktopIcon *dicon, GdkImlibImage *image)
{
	g_return_if_fail (dicon != NULL);
	g_return_if_fail (IS_DESKTOP_ICON (dicon));
	g_return_if_fail (image != NULL);

	set_icon (dicon, image);
	desktop_icon_reshape (dicon);
}

/**
 * desktop_icon_set_text
 * @dicon:	The desktop icon to set the text for
 * @text:	The new text to use for the icon's title
 *
 * Sets a new title for an existing desktop icon.
 */
void
desktop_icon_set_text (DesktopIcon *dicon, char *text)
{
	g_return_if_fail (dicon != NULL);
	g_return_if_fail (IS_DESKTOP_ICON (dicon));
	g_return_if_fail (text != NULL);

	set_text (dicon, text);
	desktop_icon_reshape (dicon);
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
	gdk_imlib_render (im, icon_width, icon_height);
	im_mask = gdk_imlib_move_mask (im);

	if (im_mask && desktop_use_shaped_icons) {
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

/**
 * desktop_icon_reshape
 * @dicon:	The desktop icon whose shape will be recalculated
 *
 * Recomputes the window shape for the specified desktop icon.  This needs to be called only when
 * the global desktop_use_shaped_icons flag changes.
 */
void
desktop_icon_reshape (DesktopIcon *dicon)
{
	GtkArg args[2];
	int icon_width, icon_height;
	int text_width, text_height;
	double x1, y1, x2, y2;

	g_return_if_fail (dicon != NULL);
	g_return_if_fail (IS_DESKTOP_ICON (dicon));

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

	dicon->width = MAX (icon_width, DESKTOP_SNAP_X);
	dicon->height = icon_height + SPACING + text_height;

	/* Set new position of children */

	gnome_canvas_item_set (dicon->icon,
			       "x", (dicon->width - icon_width) / 2.0,
			       "y", 0.0,
			       NULL);
	gnome_canvas_item_set (dicon->stipple,
			       "x1", 0.0,
			       "y1", 0.0,
			       "x2", (double) dicon->width,
			       "y2", (double) icon_height,
			       NULL);

	gnome_icon_text_item_setxy (GNOME_ICON_TEXT_ITEM (dicon->text), 0, icon_height + SPACING);

	/* Create and set the window shape */

	gtk_widget_set_usize (GTK_WIDGET (dicon), dicon->width, dicon->height);
	create_window_shape (dicon, icon_width, icon_height, text_width, text_height);

	dicon->icon_x = (int) ((dicon->width - icon_width) / 2.0 + 0.5);
	dicon->icon_y = 0;
	dicon->icon_w = icon_width;
	dicon->icon_h = icon_height;

	dicon->text_x = x1;
	dicon->text_y = y1;
	dicon->text_w = text_width;
	dicon->text_h = text_height;
}

/**
 * desktop_icon_select
 * @dicon:	The desktop icon which will be selected/unselected
 * @sel:	TRUE if icon should be selected, FALSE if it should be unselected
 *
 * Selects or unselects the icon.  This means setting the selection flag of the icon text item as
 * appropriate, and displaying the icon image as selected or not.
 */
void
desktop_icon_select (DesktopIcon *dicon, int sel)
{
	g_return_if_fail (dicon != NULL);
	g_return_if_fail (IS_DESKTOP_ICON (dicon));

	if (sel)
		gnome_canvas_item_show (dicon->stipple);
	else
		gnome_canvas_item_hide (dicon->stipple);

	gnome_icon_text_item_select (GNOME_ICON_TEXT_ITEM (dicon->text), sel);
}

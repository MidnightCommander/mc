/* Desktop icon widget for the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include "desktop-icon.h"
#include "gdesktop.h"


static void desktop_icon_class_init (DesktopIconClass *class);
static void desktop_icon_init       (DesktopIcon      *dicon);


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

	objct_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (gtk_window_get_type ());
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

	dicon->icon = gnome_canvas_item_new (gnome_canvas_root (dicon->canvas),
					     gnome_canvas_image_get_type (),
					     "anchor", GTK_ANCHOR_NW,
					     NULL);

	dicon->text = gnome_canvas_item_new (gnome_canvas_root (dicon->canvas),
					     gnome_icon_text_item_get_type (),
					     NULL);
}

GtkWidget *
desktop_icon_new (char *image_file, char *text)
{
	DesktopIcon *dicon;

	g_return_val_if_fail (image_file != NULL, NULL);
	g_return_val_if_fail (text != NULL, NULL);

	dicon = gtk_type_new (desktop_icon_get_type ());

	desktop_icon_set_icon (dicon, image_file);
	desktop_icon_set_text (dicon, text);

	return GTK_WIDGET (dicon);
}

/* Resets the positions of the desktop icon's child items and recomputes the window's shape mask */
static void
reshape (DesktopIcon *dicon)
{
	/* FIXME */
}

void
desktop_icon_set_icon (DesktopIcon *dicon, char *image_file)
{
	GdkImlibImage *im, *old_im;
	GtkArg arg;

	g_return_if_fail (dicon != NULL);
	g_return_if_fail (IS_DESKTOP_ICON (dicon));
	g_return_if_fail (image_file != NULL);

	/* Load the new image */

	if (!g_file_exists (image_file))
		return;

	im = gdk_imlib_load_image (image_file);
	if (!im)
		return;

	/* Destroy the old image if it exists */

	arg.name = "image";
	gtk_object_getv (GTK_OBJECT (dicon), 1, &arg);

	old_im = GTK_VALUE_POINTER (arg);

	gnome_canvas_item_set (dicon->icon,
			       "image", im,
			       NULL);

	if (old_im)
		gdk_imlib_destroy_image (old_im);

	reshape (dicon);
}

void
desktop_icon_set_text (DesktopIcon *dicon, char *text)
{
	g_return_if_fail (dicon != NULL);
	g_return_if_fail (IS_DESKTOP_ICON (dicon));
	g_return_if_fail (text != NULL);

	/* FIXME */
}

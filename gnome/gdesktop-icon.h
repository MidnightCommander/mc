/* Desktop icon widget for the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef GDESKTOP_ICON_H
#define GDESKTOP_ICON_H

#include <libgnome/gnome-defs.h>
#include <gtk/gtkwindow.h>
#include <libgnomeui/gnome-canvas.h>
#include <libgnomeui/gnome-icon-text.h>

BEGIN_GNOME_DECLS


#define DESKTOP_ICON_FONT "-*-helvetica-medium-r-normal--10-*-*-*-p-*-*-*"


#define TYPE_DESKTOP_ICON            (desktop_icon_get_type ())
#define DESKTOP_ICON(obj)            (GTK_CHECK_CAST ((obj), TYPE_DESKTOP_ICON, DesktopIcon))
#define DESKTOP_ICON_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_DESKTOP_ICON, DesktopIconClass))
#define IS_DESKTOP_ICON(obj)         (GTK_CHECK_TYPE ((obj), TYPE_DESKTOP_ICON))
#define IS_DESKTOP_ICON_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_DESKTOP_ICON))


typedef struct _DesktopIcon DesktopIcon;
typedef struct _DesktopIconClass DesktopIconClass;

struct _DesktopIcon {
	GtkWindow window;

	GtkWidget *canvas;		/* The canvas that holds the icon and the icon text item */

	GnomeCanvasItem *icon;		/* The item that contains the icon */
	GnomeCanvasItem *text;		/* The item that contains the editable text */

	int width, height;		/* Total size of the window */

	int w_changed_id;		/* Signal connection ID for "width_changed" from the icon text item */
	int h_changed_id;		/* Signal connection ID for "height_changed" from the icon text item */
};

struct _DesktopIconClass {
	GtkWindowClass parent_class;
};


/* Standard Gtk function */
GtkType desktop_icon_get_type (void);

/* Creates a new desktop icon from the specified image file, and with the specified title */
GtkWidget *desktop_icon_new (char *image_file, char *text);

/* Sets the icon from the specified file name */
void desktop_icon_set_icon (DesktopIcon *dicon, char *image_file);

/* Sets the icon's text */
void desktop_icon_set_text (DesktopIcon *dicon, char *text);

/* Makes the desktop icon reshape itself (for when the global desktop_use_shaped_icons flag changes) */
void desktop_icon_reshape (DesktopIcon *dicon);


END_GNOME_DECLS

#endif

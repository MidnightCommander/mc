/* gnome-open-dialog.h
 * Copyright (C) 1999  Red Hat Software. Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GNOME_OPEN_DIALOG_H__
#define __GNOME_OPEN_DIALOG_H__

#include <gnome.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define GNOME_TYPE_OPEN_DIALOG			(gnome_open_dialog_get_type ())
#define GNOME_OPEN_DIALOG(obj)			(GTK_CHECK_CAST ((obj), GNOME_TYPE_OPEN_DIALOG, GnomeOpenDialog))
#define GNOME_OPEN_DIALOG_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_OPEN_DIALOG, GnomeOpenDialogClass))
#define GNOME_IS_OPEN_DIALOG(obj)			(GTK_CHECK_TYPE ((obj), GNOME_TYPE_OPEN_DIALOG))
#define GNOME_IS_OPEN_DIALOG_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), GNOME_TYPE_OPEN_DIALOG))


typedef struct _GnomeOpenDialog       GnomeOpenDialog;
typedef struct _GnomeOpenDialogClass  GnomeOpenDialogClass;

struct _GnomeOpenDialog
{
	GnomeDialog parent;

	gchar *file_name;
	GtkWidget *ctree;
	GtkWidget *entry;
};
struct _GnomeOpenDialogClass
{
	GnomeDialogClass parent_class;
};


GtkType    gnome_open_dialog_get_type (void);
GtkWidget *gnome_open_dialog_new      (gchar *file_name);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GNOME_OPEN_DIALOG_H__ */

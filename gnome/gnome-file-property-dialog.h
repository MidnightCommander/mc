/* gnome-file-property-dialog.h
 * Copyright (C) 1999  Free Software Foundation
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
#ifndef __GNOME_FILE_PROPERTY_DIALOG_H__
#define __GNOME_FILE_PROPERTY_DIALOG_H__

#include <gnome.h>
#include "gdesktop.h"
#include <sys/stat.h>
#include <unistd.h>
#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define GNOME_TYPE_FILE_PROPERTY_DIALOG			(gnome_file_property_dialog_get_type ())
#define GNOME_FILE_PROPERTY_DIALOG(obj)			(GTK_CHECK_CAST ((obj), GNOME_TYPE_FILE_PROPERTY_DIALOG, GnomeFilePropertyDialog))
#define GNOME_FILE_PROPERTY_DIALOG_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_FILE_PROPERTY_DIALOG, GnomeFilePropertyDialogClass))
#define GNOME_IS_FILE_PROPERTY_DIALOG(obj)			(GTK_CHECK_TYPE ((obj), GNOME_TYPE_FILE_PROPERTY_DIALOG))
#define GNOME_IS_FILE_PROPERTY_DIALOG_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), GNOME_TYPE_FILE_PROPERTY_DIALOG))

typedef struct _GnomeFilePropertyDialog       GnomeFilePropertyDialog;
typedef struct _GnomeFilePropertyDialogClass  GnomeFilePropertyDialogClass;

struct _GnomeFilePropertyDialog
{
	GnomeDialog parent;

	gchar *file_name;
	gchar *user_name;
	gchar *group_name;
	gchar *mode_name;
	
	struct stat st;
	gboolean executable;
	gboolean modifyable;
	GtkWidget *file_entry;

	/* Permissions stuff */
	GtkWidget *mode_label;
	
	GtkWidget *suid, *sgid, *svtx;
	GtkWidget *rusr, *wusr, *xusr;
	GtkWidget *rgrp, *wgrp, *xgrp;
	GtkWidget *roth, *woth, *xoth;

	GtkWidget *owner_entry;
	GtkWidget *group_entry;
	gint euid;

	/* Settings Stuff */
	GtkWidget *open_label, *open_entry, *open_cbox;
	GtkWidget *prop1_label, *prop1_entry, *prop1_cbox;
	GtkWidget *prop2_label, *prop2_entry, *prop2_cbox, *prop2_hline;
	GtkWidget *button;
	GtkWidget *desktop_entry;
	GtkWidget *caption_entry;
	GtkWidget *needs_terminal_check;
	
	gchar *fm_open;
	gchar *fm_view;
	gchar *drop_target;
	gchar *edit;
	const gchar *mime_fm_open;
	const gchar *mime_fm_view;
	const gchar *mime_drop_target;
	const gchar *mime_edit;
	gchar *icon_filename;
	gchar *desktop_url;
	gchar *caption;
	
	gboolean can_set_icon;
	gboolean needs_terminal;
	
	GdkImlibImage *im;

	/* Private Data */
	gboolean changing;
};
struct _GnomeFilePropertyDialogClass
{
	GnomeDialogClass parent_class;
};


GtkType    gnome_file_property_dialog_get_type     (void);
GtkWidget *gnome_file_property_dialog_new          (gchar *file_name, gboolean can_set_icon);
gint       gnome_file_property_dialog_make_changes (GnomeFilePropertyDialog *file_property_dialog);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GNOME_FILE_PROPERTY_DIALOG_H__ */


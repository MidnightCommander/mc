/* gnome-file-property-dialog.c
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
#include <config.h>
#include "dir.h"
#include "util.h"
#include <gnome.h>
#include <time.h>
#include <string.h>
#include "gnome-file-property-dialog.h"
#include "gdesktop.h"
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fileopctx.h"
#include "file.h"
#include "../vfs/vfs.h"
#include "gicon.h"
#include "dialog.h"

static void gnome_file_property_dialog_init		(GnomeFilePropertyDialog	 *file_property_dialog);
static void gnome_file_property_dialog_class_init	(GnomeFilePropertyDialogClass	 *klass);
static void gnome_file_property_dialog_finalize         (GtkObject *object);

static GnomeDialogClass *parent_class = NULL;


GtkType
gnome_file_property_dialog_get_type (void)
{
  static GtkType file_property_dialog_type = 0;

  if (!file_property_dialog_type)
    {
      static const GtkTypeInfo file_property_dialog_info =
      {
        "GnomeFilePropertyDialog",
        sizeof (GnomeFilePropertyDialog),
        sizeof (GnomeFilePropertyDialogClass),
        (GtkClassInitFunc) gnome_file_property_dialog_class_init,
        (GtkObjectInitFunc) gnome_file_property_dialog_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
      };

      file_property_dialog_type = gtk_type_unique (gnome_dialog_get_type (), &file_property_dialog_info);
    }

  return file_property_dialog_type;
}

static void
gnome_file_property_dialog_class_init (GnomeFilePropertyDialogClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) klass;

	parent_class = gtk_type_class (gnome_dialog_get_type ());
	object_class->finalize = gnome_file_property_dialog_finalize;


}

static void
gnome_file_property_dialog_init (GnomeFilePropertyDialog *file_property_dialog)
{
	gnome_dialog_close_hides (GNOME_DIALOG (file_property_dialog), TRUE);

	file_property_dialog->file_name = NULL;
	file_property_dialog->file_entry = NULL;
	file_property_dialog->group_name = NULL;
	file_property_dialog->modifyable = TRUE;
	file_property_dialog->user_name = NULL;
	file_property_dialog->prop1_label = NULL;
	file_property_dialog->prop2_label = NULL;
	file_property_dialog->prop1_entry = NULL;
	file_property_dialog->prop2_entry = NULL;
	file_property_dialog->prop1_cbox = NULL;
	file_property_dialog->prop2_cbox = NULL;
	file_property_dialog->fm_open = NULL;
	file_property_dialog->fm_view = NULL;
	file_property_dialog->edit = NULL;
	file_property_dialog->drop_target = NULL;
	file_property_dialog->im = NULL;
}

static void
gnome_file_property_dialog_finalize (GtkObject *object)
{
	GnomeFilePropertyDialog *gfpd;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_FILE_PROPERTY_DIALOG (object));

	gfpd = GNOME_FILE_PROPERTY_DIALOG (object);

	if (gfpd->file_name)
		g_free (gfpd->file_name);
	if (gfpd->fm_open)
		g_free (gfpd->fm_open);
	if (gfpd->fm_view)
		g_free (gfpd->fm_view);
	if (gfpd->edit)
		g_free (gfpd->edit);
	if (gfpd->drop_target)
		g_free (gfpd->drop_target);
	if (gfpd->icon_filename)
		g_free (gfpd->icon_filename);
	if (gfpd->desktop_url)
		g_free (gfpd->desktop_url);
	if (gfpd->caption)
		g_free (gfpd->caption);

	if (gfpd->mode_name)
		g_free (gfpd->mode_name);

	(* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Create the pane */

static GtkWidget *
create_general_properties (GnomeFilePropertyDialog *fp_dlg)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *align;
	gchar *direc;
	gchar *gen_string;
	gchar buf[MC_MAXPATHLEN];
	gchar buf2[MC_MAXPATHLEN];
	file_entry *fe;
	GtkWidget *icon;
	struct tm *time;
	GtkWidget *table;
	int n;

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD);

        /* first, we set the icon */
	direc = g_strdup (fp_dlg->file_name);
	strrchr (direc, '/')[0] = '\0';
	fe = file_entry_from_file (fp_dlg->file_name);
	fp_dlg->im = gicon_get_icon_for_file (direc, fe, FALSE);
	file_entry_free (fe);
	g_free (direc);
	icon = gnome_pixmap_new_from_imlib (fp_dlg->im);
	gtk_box_pack_start (GTK_BOX (vbox), icon, FALSE, FALSE, 0);

	/* we set the file part */
	gen_string = g_strconcat (_("Full Name: "), fp_dlg->file_name, NULL);
	label = gtk_label_new (gen_string);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	g_free (gen_string);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	/* if it's a symlink */
	align = gtk_alignment_new (0.0, 0.5, 1.0, 1.0);
	hbox = gtk_hbox_new (FALSE, 2);
	label = gtk_label_new (_("File Name"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	fp_dlg->file_entry = gtk_entry_new ();
	gen_string = strrchr (fp_dlg->file_name, '/');
	if (gen_string)
		gtk_entry_set_text (GTK_ENTRY (fp_dlg->file_entry), gen_string + 1);
	else
		/* I don't think this should happen anymore, but it can't hurt in
		 * case this ever gets used outside gmc */
		gtk_entry_set_text (GTK_ENTRY (fp_dlg->file_entry), fp_dlg->file_name);
	/* We want to prevent editing of the file if
	 * the thing is a BLK, CHAR, or we don't own it */
	/* FIXME: We can actually edit it if we're in the same grp of the file. */
	if (fp_dlg->modifyable == FALSE)
		gtk_widget_set_sensitive (fp_dlg->file_entry, FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), fp_dlg->file_entry, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (align), hbox);
	gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), gtk_hseparator_new (), FALSE, FALSE, 8);

	/* File statistics */
	/* File type first */
  	if (S_ISREG (fp_dlg->st.st_mode)) {
		gen_string = g_strconcat (_("File Type: "),
					  gnome_mime_type (fp_dlg->file_name),
					  NULL);
		label = gtk_label_new (gen_string);
		g_free (gen_string);
	} else if (S_ISLNK (fp_dlg->st.st_mode)) {
		label = gtk_label_new (_("File Type: Symbolic Link"));
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		n = mc_readlink (fp_dlg->file_name, buf, MC_MAXPATHLEN);
		if (n < 0)
			label = gtk_label_new (_("Target Name: INVALID LINK"));
		else {
			buf [n] = 0;
			gen_string = g_strconcat (_("Target Name: "), buf, NULL);
			label = gtk_label_new (gen_string);
			g_free (gen_string);
		}
	}else if (S_ISDIR (fp_dlg->st.st_mode))
		label = gtk_label_new (_("File Type: Directory"));
	else if (S_ISCHR (fp_dlg->st.st_mode))
		label = gtk_label_new (_("File Type: Character Device"));
	else if (S_ISBLK (fp_dlg->st.st_mode))
		label = gtk_label_new (_("File Type: Block Device"));
	else if (S_ISSOCK (fp_dlg->st.st_mode))
		label = gtk_label_new (_("File Type: Socket"));
	else if (S_ISFIFO (fp_dlg->st.st_mode))
		label = gtk_label_new (_("File Type: FIFO"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	/* Now file size */
	if (S_ISDIR (fp_dlg->st.st_mode)
	    || S_ISREG (fp_dlg->st.st_mode)
	    || S_ISLNK (fp_dlg->st.st_mode)) {
		if ((gint)fp_dlg->st.st_size < 1024) {
			snprintf (buf, MC_MAXPATHLEN, "%d", (gint) fp_dlg->st.st_size);
			gen_string = g_strconcat (_("File Size: "), buf, _(" bytes"), NULL);
		} else if ((gint)fp_dlg->st.st_size < 1024 * 1024) {
			snprintf (buf, MC_MAXPATHLEN, "%.1f", (gfloat) fp_dlg->st.st_size / 1024.0);
			snprintf (buf2, MC_MAXPATHLEN, "%d", (gint) fp_dlg->st.st_size);
			gen_string = g_strconcat (_("File Size: "), buf, _(" KBytes  ("),
						  buf2, _(" bytes)"), NULL);
		} else {
			snprintf (buf, MC_MAXPATHLEN, "%.1f",
				  (gfloat) fp_dlg->st.st_size / (1024.0 * 1024.0));
			snprintf (buf2, MC_MAXPATHLEN, "%d", (gint) fp_dlg->st.st_size);
			gen_string = g_strconcat (_("File Size: "), buf, _(" MBytes  ("),
						  buf2, _(" bytes)"), NULL);
		}
		label = gtk_label_new (gen_string);
		g_free (gen_string);
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	} else {
		label = gtk_label_new (_("File Size: N/A"));
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	}

	gtk_box_pack_start (GTK_BOX (vbox), gtk_hseparator_new (), FALSE, FALSE, 8);

	/* Time Fields */
	table = gtk_table_new (3, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 2);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
	label = gtk_label_new (_("File Created on: "));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);
	time = localtime (&(fp_dlg->st.st_ctime));
	strftime (buf, MC_MAXPATHLEN, "%a, %b %d %Y,  %I:%M:%S %p", time);
	label = gtk_label_new (buf);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 1, 2, 0, 1);

	label = gtk_label_new (_("Last Modified on: "));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);
	time = localtime (&(fp_dlg->st.st_mtime));
	strftime (buf, MC_MAXPATHLEN, "%a, %b %d %Y,  %I:%M:%S %p", time);
	label = gtk_label_new (buf);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 1, 2, 1, 2);

	label = gtk_label_new (_("Last Accessed on: "));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);
	time = localtime (&(fp_dlg->st.st_atime));
	strftime (buf, MC_MAXPATHLEN, "%a, %b %d %Y,  %I:%M:%S %p", time);
	label = gtk_label_new (buf);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 1, 2, 2, 3);
	return vbox;
}

static GtkWidget *
create_url_properties (GnomeFilePropertyDialog *fp_dlg)
{
	GtkWidget *table, *label;

	table = gtk_table_new (0, 0, 0);

	label = gtk_label_new (_("URL:"));
	gtk_table_attach (GTK_TABLE (table), label,
			  1, 2, 1, 2, 0, 0, GNOME_PAD, GNOME_PAD);
	fp_dlg->desktop_entry = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), fp_dlg->desktop_entry,
			  2, 3, 1, 2, GTK_FILL | GTK_EXPAND, 0, GNOME_PAD, GNOME_PAD);

	label = gtk_label_new (_("Caption:"));
	gtk_table_attach (GTK_TABLE (table), label,
			  1, 2, 2, 3, 0, 0, GNOME_PAD, GNOME_PAD);
	fp_dlg->caption_entry = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), fp_dlg->caption_entry,
			  2, 3, 2, 3, GTK_FILL | GTK_EXPAND, 0, GNOME_PAD, GNOME_PAD);

	gtk_widget_show_all (table);
	return table;
}

/* Settings Pane */
static void
metadata_toggled (GtkWidget *cbox, GnomeFilePropertyDialog *fp_dlg)
{
	if (fp_dlg->changing)
		return;
	if (cbox == fp_dlg->open_cbox) {
		if (GTK_TOGGLE_BUTTON (cbox)->active) {
			gtk_widget_set_sensitive (fp_dlg->open_entry, FALSE);
			if (fp_dlg->mime_fm_open)
				gtk_entry_set_text (GTK_ENTRY (fp_dlg->open_entry), fp_dlg->mime_fm_open);
		} else {
			gtk_widget_set_sensitive (fp_dlg->open_entry, TRUE);
			if (fp_dlg->fm_open) {
				gtk_entry_set_text (GTK_ENTRY (fp_dlg->open_entry), fp_dlg->fm_open);
			}
		}
	} else if (cbox == fp_dlg->prop1_cbox) {
		if (GTK_TOGGLE_BUTTON (cbox)->active) {
			gtk_widget_set_sensitive (fp_dlg->prop1_entry, FALSE);
			if (fp_dlg->executable && fp_dlg->mime_drop_target) {
				gtk_entry_set_text (GTK_ENTRY (fp_dlg->prop1_entry), fp_dlg->mime_drop_target);
			} else if (!fp_dlg->executable &&  fp_dlg->mime_fm_view) {
				gtk_entry_set_text (GTK_ENTRY (fp_dlg->prop1_entry), fp_dlg->mime_fm_view);
			} else {
				gtk_entry_set_text (GTK_ENTRY (fp_dlg->prop1_entry), "");
			}
		} else {
			gtk_widget_set_sensitive (fp_dlg->prop1_entry, TRUE);
			if (fp_dlg->executable && fp_dlg->drop_target) {
				gtk_entry_set_text (GTK_ENTRY (fp_dlg->prop1_entry), fp_dlg->drop_target);
			} else if (!fp_dlg->executable &&  fp_dlg->fm_view) {
				gtk_entry_set_text (GTK_ENTRY (fp_dlg->prop1_entry), fp_dlg->fm_view);
			}
		}
	} else {
		if (GTK_TOGGLE_BUTTON (cbox)->active) {
			gtk_widget_set_sensitive (fp_dlg->prop2_entry, FALSE);
			if (fp_dlg->mime_edit) {
				gtk_entry_set_text (GTK_ENTRY (fp_dlg->prop2_entry), fp_dlg->mime_edit);
			} else {
				gtk_entry_set_text (GTK_ENTRY (fp_dlg->prop2_entry), "");
			}
		} else {
			gtk_widget_set_sensitive (fp_dlg->prop2_entry, TRUE);
		        if (fp_dlg->edit) {
				gtk_entry_set_text (GTK_ENTRY (fp_dlg->prop2_entry), fp_dlg->edit);
			}
		}
	}
}

static void
switch_metadata_box (GnomeFilePropertyDialog *fp_dlg)
{
	if (NULL == fp_dlg->prop1_label)
		return;
	fp_dlg->changing = TRUE;

	if (fp_dlg->desktop_url){
		gtk_entry_set_text (GTK_ENTRY (fp_dlg->desktop_entry), fp_dlg->desktop_url);

		gtk_entry_set_text (GTK_ENTRY (fp_dlg->caption_entry), fp_dlg->caption);
	}

	if (fp_dlg->executable) {
		gtk_label_set_text (GTK_LABEL (fp_dlg->prop1_label), _("Drop Action"));
		gtk_label_set_text (GTK_LABEL (GTK_BIN (fp_dlg->prop1_cbox)->child), _("Use default Drop Action options"));
		if (fp_dlg->drop_target) {
			gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON  (fp_dlg->prop1_cbox), FALSE);
			gtk_entry_set_text (GTK_ENTRY (fp_dlg->prop1_entry), fp_dlg->drop_target);
			gtk_widget_set_sensitive (fp_dlg->prop1_entry, TRUE);
		} else if (fp_dlg->mime_drop_target) {
			gtk_entry_set_text (GTK_ENTRY (fp_dlg->prop1_entry), fp_dlg->mime_drop_target);
			gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON  (fp_dlg->prop1_cbox), TRUE);
			gtk_widget_set_sensitive (fp_dlg->prop1_entry, FALSE);
		} else {
			gtk_entry_set_text (GTK_ENTRY (fp_dlg->prop1_entry), "");
			gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON  (fp_dlg->prop1_cbox), TRUE);
			gtk_widget_set_sensitive (fp_dlg->prop1_entry, FALSE);
		}
	} else {
		gtk_label_set_text (GTK_LABEL (fp_dlg->prop1_label), _("View"));
		gtk_label_set_text (GTK_LABEL (GTK_BIN (fp_dlg->prop1_cbox)->child), _("Use default View options"));
		if (fp_dlg->fm_view) {
			gtk_entry_set_text (GTK_ENTRY (fp_dlg->prop1_entry), fp_dlg->fm_view);
			gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON  (fp_dlg->prop1_cbox), FALSE);
			gtk_widget_set_sensitive (fp_dlg->prop1_entry, TRUE);
		} else if (fp_dlg->mime_fm_view) {
			gtk_entry_set_text (GTK_ENTRY (fp_dlg->prop1_entry), fp_dlg->mime_fm_view);
			gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON  (fp_dlg->prop1_cbox), TRUE);
			gtk_widget_set_sensitive (fp_dlg->prop1_entry, FALSE);
		} else {
			gtk_entry_set_text (GTK_ENTRY (fp_dlg->prop1_entry), "");
			gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON  (fp_dlg->prop1_cbox), TRUE);
			gtk_widget_set_sensitive (fp_dlg->prop1_entry, FALSE);
		}
	}
	if (fp_dlg->executable) {
		gtk_widget_hide (fp_dlg->prop2_label);
		gtk_widget_hide (fp_dlg->prop2_entry);
		gtk_widget_hide (fp_dlg->prop2_cbox);
		gtk_widget_hide (fp_dlg->prop2_hline);
	} else {
		gtk_widget_show (fp_dlg->prop2_label);
		gtk_widget_show (fp_dlg->prop2_entry);
		gtk_widget_show (fp_dlg->prop2_cbox);
		gtk_widget_show (fp_dlg->prop2_hline);
	}
	fp_dlg->changing = FALSE;
}

static GtkWidget *
generate_icon_sel (GnomeFilePropertyDialog *fp_dlg)
{
	GtkWidget *retval;
	gchar *icon;

	retval = gnome_icon_entry_new ("gmc_file_icon", _("Select an Icon"));
	icon = g_strdup (gicon_get_filename_for_icon (fp_dlg->im));

	if (icon == NULL)
		return retval;
	gnome_icon_entry_set_icon (GNOME_ICON_ENTRY (retval), icon);
	fp_dlg->icon_filename = icon;
	return retval;
}

static GtkWidget *
generate_actions_box (GnomeFilePropertyDialog *fp_dlg)
{
	GtkWidget *vbox;
	GtkWidget *table;

	/* Here's the Logic: */
	/* All tops of files (other then folders) should let us edit "open" */
	/* If we are a file, and an executable, we want to edit our "drop-action" */
	/* Metadata, as it is meaningful to us.  */
	/* If we are non-executable, we want to edit our "edit" and "view" fields. */
	/* Sym links want to have the same options as above, but use their Target's */
	/* Executable/set bit in order to determine which one. */
	/* Note, symlinks can set their own metadata, independent from their */
	/* targets. */

	table = gtk_table_new (8, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), GNOME_PAD_SMALL);

	/* we do open first */
	fp_dlg->open_label = gtk_label_new (_("Open"));
	gtk_misc_set_alignment (GTK_MISC (fp_dlg->open_label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (fp_dlg->open_label), 2, 0);
	gtk_table_attach_defaults (GTK_TABLE (table),
				   fp_dlg->open_label,
				   0, 1, 0, 1);
	fp_dlg->open_entry = gtk_entry_new ();
	gtk_table_attach_defaults (GTK_TABLE (table),
				   fp_dlg->open_entry,
				   1, 2, 0, 1);
	fp_dlg->open_cbox = gtk_check_button_new_with_label (_("Use default Open action"));
	gtk_signal_connect (GTK_OBJECT (fp_dlg->open_cbox), "toggled", metadata_toggled, fp_dlg);
	gtk_table_attach_defaults (GTK_TABLE (table), fp_dlg->open_cbox, 0, 2, 1, 2);


	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), gtk_hseparator_new (), FALSE, FALSE, GNOME_PAD_SMALL);
	gtk_table_attach_defaults (GTK_TABLE (table), vbox, 0, 2, 2, 3);

	if (fp_dlg->executable)
		fp_dlg->prop1_label = gtk_label_new (_("Drop Action"));
	else
		fp_dlg->prop1_label = gtk_label_new (_("View"));
	gtk_misc_set_alignment (GTK_MISC (fp_dlg->prop1_label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (fp_dlg->prop1_label), 2, 0);
	gtk_table_attach_defaults (GTK_TABLE (table),
				   fp_dlg->prop1_label,
				   0, 1, 3, 4);
	fp_dlg->prop1_entry = gtk_entry_new ();
	gtk_table_attach_defaults (GTK_TABLE (table),
				   fp_dlg->prop1_entry,
				   1, 2, 3, 4);
	if (fp_dlg->executable)
		fp_dlg->prop1_cbox = gtk_check_button_new_with_label (_("Use default Drop action"));
	else
		fp_dlg->prop1_cbox = gtk_check_button_new_with_label (_("Use default View action"));
	gtk_signal_connect (GTK_OBJECT (fp_dlg->prop1_cbox), "toggled", metadata_toggled, fp_dlg);
	gtk_table_attach_defaults (GTK_TABLE (table), fp_dlg->prop1_cbox, 0, 2, 4, 5);

	vbox = gtk_vbox_new (FALSE, 0);
	fp_dlg->prop2_hline = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (vbox), fp_dlg->prop2_hline, FALSE, FALSE, GNOME_PAD_SMALL);
	gtk_table_attach_defaults (GTK_TABLE (table), vbox, 0, 2, 5, 6);

	fp_dlg->prop2_label = gtk_label_new (_("Edit"));
	gtk_misc_set_alignment (GTK_MISC (fp_dlg->prop2_label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (fp_dlg->prop2_label), 2, 0);
	gtk_table_attach_defaults (GTK_TABLE (table),
				   fp_dlg->prop2_label,
				   0, 1, 6, 7);
	fp_dlg->prop2_entry = gtk_entry_new ();
	gtk_table_attach_defaults (GTK_TABLE (table),
				   fp_dlg->prop2_entry,
				   1, 2, 6, 7);
	fp_dlg->prop2_cbox = gtk_check_button_new_with_label (_("Use default Edit action"));
	gtk_signal_connect (GTK_OBJECT (fp_dlg->prop2_cbox), "toggled", metadata_toggled, fp_dlg);
	gtk_table_attach_defaults (GTK_TABLE (table), fp_dlg->prop2_cbox, 0, 2, 7, 8);

	/* we set the open field */
	fp_dlg->changing = TRUE;
	if (fp_dlg->fm_open) {
		gtk_entry_set_text (GTK_ENTRY (fp_dlg->open_entry), fp_dlg->fm_open);
		gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON  (fp_dlg->open_cbox), FALSE);
		gtk_widget_set_sensitive (fp_dlg->open_entry, TRUE);
	} else if (fp_dlg->mime_fm_open) {
		gtk_entry_set_text (GTK_ENTRY (fp_dlg->open_entry), fp_dlg->mime_fm_open);
		gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON  (fp_dlg->open_cbox), TRUE);
		gtk_widget_set_sensitive (fp_dlg->open_entry, FALSE);
	} else {
		gtk_entry_set_text (GTK_ENTRY (fp_dlg->open_entry), "");
		gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON  (fp_dlg->open_cbox), TRUE);
		gtk_widget_set_sensitive (fp_dlg->open_entry, FALSE);
	}
	if (fp_dlg->edit) {
		gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON  (fp_dlg->prop2_cbox), FALSE);
		gtk_entry_set_text (GTK_ENTRY (fp_dlg->prop2_entry), fp_dlg->edit);
		gtk_widget_set_sensitive (fp_dlg->prop2_entry, TRUE);
	} else if (fp_dlg->mime_edit) {
		gtk_entry_set_text (GTK_ENTRY (fp_dlg->prop2_entry), fp_dlg->mime_edit);
		gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON  (fp_dlg->prop2_cbox), TRUE);
		gtk_widget_set_sensitive (fp_dlg->prop2_entry, FALSE);
	} else {
		gtk_entry_set_text (GTK_ENTRY (fp_dlg->prop2_entry), "");
		gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON  (fp_dlg->prop2_cbox), TRUE);
		gtk_widget_set_sensitive (fp_dlg->prop2_entry, FALSE);
	}
	fp_dlg->changing = FALSE;

	return table;
}

static GtkWidget *
create_settings_pane (GnomeFilePropertyDialog *fp_dlg)
{
	GtkWidget *vbox = NULL;
	GtkWidget *hbox;
	GtkWidget *vbox2;
	GtkWidget *frame;
	GtkWidget *align;
	GtkWidget *table;
	struct stat linkstat;
	int finish;

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD);

	if (fp_dlg->can_set_icon) {

		frame = gtk_frame_new (_("Icon"));
		vbox2 = gtk_vbox_new (FALSE, 0);
		hbox = gtk_hbox_new (FALSE, 0);
		align = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
		gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
		fp_dlg->button = generate_icon_sel (fp_dlg);
		gtk_container_add (GTK_CONTAINER (frame), vbox2);
		gtk_container_set_border_width (GTK_CONTAINER (vbox2), GNOME_PAD_SMALL);
		gtk_box_pack_start (GTK_BOX (vbox2), hbox, TRUE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (hbox), align, TRUE, FALSE, 0);
		gtk_container_add (GTK_CONTAINER (align), fp_dlg->button);
	}
	/* Are we a directory or device?  If so, we do nothing else. */
	if (S_ISLNK (fp_dlg->st.st_mode))
		mc_stat (fp_dlg->file_name, &linkstat);

	finish = 0;
	if (!S_ISREG (fp_dlg->st.st_mode)){
		if (S_ISLNK (fp_dlg->st.st_mode)){
			if (!S_ISREG (linkstat.st_mode))
				finish = 1;
		} else
			finish = 1;
	}

	if (finish){
		if (!fp_dlg->can_set_icon) {
			gtk_widget_unref (vbox);
			vbox = NULL;
		}
		return vbox;
	}

        /* We must be a file or a link to a file. */
	frame = gtk_frame_new (_("File Actions"));
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
	table = generate_actions_box (fp_dlg);
	gtk_container_add (GTK_CONTAINER (frame), table);

        frame = gtk_frame_new (_("Open action"));
	fp_dlg->needs_terminal_check = gtk_check_button_new_with_label (_("Needs terminal to run"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fp_dlg->needs_terminal_check),
				      fp_dlg->needs_terminal);
	gtk_container_add (GTK_CONTAINER (frame), fp_dlg->needs_terminal_check);

	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
	return vbox;
}

/* Permissions Pane */
static GtkWidget *
label_new (char *text, double xalign, double yalign)
{
	GtkWidget *label;

	label = gtk_label_new (text);
	gtk_misc_set_alignment (GTK_MISC (label), xalign, yalign);
	gtk_widget_show (label);

	return label;
}

static mode_t
perm_get_umode (GnomeFilePropertyDialog *fp_dlg)
{
	mode_t umode;

#define SETBIT(widget, flag) umode |= GTK_TOGGLE_BUTTON (widget)->active ? flag : 0

	umode = 0;

	SETBIT (fp_dlg->suid, S_ISUID);
	SETBIT (fp_dlg->sgid, S_ISGID);
	SETBIT (fp_dlg->svtx, S_ISVTX);

	SETBIT (fp_dlg->rusr, S_IRUSR);
	SETBIT (fp_dlg->wusr, S_IWUSR);
	SETBIT (fp_dlg->xusr, S_IXUSR);

	SETBIT (fp_dlg->rgrp, S_IRGRP);
	SETBIT (fp_dlg->wgrp, S_IWGRP);
	SETBIT (fp_dlg->xgrp, S_IXGRP);

	SETBIT (fp_dlg->roth, S_IROTH);
	SETBIT (fp_dlg->woth, S_IWOTH);
	SETBIT (fp_dlg->xoth, S_IXOTH);

	return umode;

#undef SETBIT
}

static void
perm_set_mode_label (GtkWidget *widget, gpointer data)
{
	GnomeFilePropertyDialog *fp_dlg;
	mode_t umode;
	char s_mode[5];

	fp_dlg = GNOME_FILE_PROPERTY_DIALOG (data);
	umode = perm_get_umode (fp_dlg);
	if (!fp_dlg->executable && (umode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
		fp_dlg->executable = TRUE;
		switch_metadata_box (fp_dlg);
	} else if (fp_dlg->executable && !(umode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
		fp_dlg->executable = FALSE;
		switch_metadata_box (fp_dlg);
	}

	s_mode[0] = '0' + ((umode & (S_ISUID | S_ISGID | S_ISVTX)) >> 9);
	s_mode[1] = '0' + ((umode & (S_IRUSR | S_IWUSR | S_IXUSR)) >> 6);
	s_mode[2] = '0' + ((umode & (S_IRGRP | S_IWGRP | S_IXGRP)) >> 3);
	s_mode[3] = '0' + ((umode & (S_IROTH | S_IWOTH | S_IXOTH)) >> 0);
	s_mode[4] = 0;
	gtk_label_set_text (GTK_LABEL (fp_dlg->mode_label), s_mode);
}

static GtkWidget *
perm_check_new (char *text, int state, GnomeFilePropertyDialog *fp_dlg)
{
	GtkWidget *check;

	if (text)
		check = gtk_check_button_new_with_label (text);
	else
		check = gtk_check_button_new ();

	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (check), FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), state ? TRUE : FALSE);

	gtk_signal_connect (GTK_OBJECT (check), "toggled",
			    (GtkSignalFunc) perm_set_mode_label,
			    fp_dlg);

	gtk_widget_show (check);
	return check;
}

#define ATTACH(table, widget, left, right, top, bottom)	\
gtk_table_attach (GTK_TABLE (table), widget,		\
		  left, right, top, bottom,		\
		  GTK_FILL | GTK_SHRINK,		\
		  GTK_FILL | GTK_SHRINK,		\
		  0, 0);

#define PERMSET(name, r, w, x, rmask, wmask, xmask, y) do {		\
	r = perm_check_new (NULL, fp_dlg->st.st_mode & rmask, fp_dlg);			\
	w = perm_check_new (NULL, fp_dlg->st.st_mode & wmask, fp_dlg);			\
	x = perm_check_new (NULL, fp_dlg->st.st_mode & xmask, fp_dlg);			\
									\
	ATTACH (table, label_new (name, 0.0, 0.5), 0, 1, y, y + 1);	\
	ATTACH (table, r, 1, 2, y, y + 1);				\
	ATTACH (table, w, 2, 3, y, y + 1);				\
	ATTACH (table, x, 3, 4, y, y + 1);				\
} while (0);

static GtkWidget *
perm_mode_new (GnomeFilePropertyDialog *fp_dlg)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *table;
	gchar *mode;

	frame = gtk_frame_new (_("File Permissions"));

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_widget_show (vbox);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	gtk_box_pack_start (GTK_BOX (hbox), label_new (_("Current mode: "), 0.0, 0.5), FALSE, FALSE, 0);

	fp_dlg->mode_label = label_new ("0000", 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), fp_dlg->mode_label, FALSE, FALSE, 0);

	table = gtk_table_new (4, 5, FALSE);
	if (!fp_dlg->modifyable || S_ISLNK (fp_dlg->st.st_mode))
		gtk_widget_set_sensitive (table, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (table), 4);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
	gtk_widget_show (table);

	/* Headings */

	ATTACH (table, label_new (_("Read"), 0.0, 0.5),     1, 2, 0, 1);
	ATTACH (table, label_new (_("Write"), 0.0, 0.5),    2, 3, 0, 1);
	ATTACH (table, label_new (_("Exec"), 0.0, 0.5),     3, 4, 0, 1);
	ATTACH (table, label_new (_("Special"), 0.0, 0.5),  4, 5, 0, 1);

	/* Permissions */

	PERMSET (_("User"),  fp_dlg->rusr, fp_dlg->wusr, fp_dlg->xusr, S_IRUSR, S_IWUSR, S_IXUSR, 1);
	PERMSET (_("Group"), fp_dlg->rgrp, fp_dlg->wgrp, fp_dlg->xgrp, S_IRGRP, S_IWGRP, S_IXGRP, 2);
	PERMSET (_("Other"), fp_dlg->roth, fp_dlg->woth, fp_dlg->xoth, S_IROTH, S_IWOTH, S_IXOTH, 3);

	/* Special */

	fp_dlg->suid = perm_check_new (_("Set UID"), fp_dlg->st.st_mode & S_ISUID, fp_dlg);
	fp_dlg->sgid = perm_check_new (_("Set GID"), fp_dlg->st.st_mode & S_ISGID, fp_dlg);
	fp_dlg->svtx = perm_check_new (_("Sticky"),  fp_dlg->st.st_mode & S_ISVTX, fp_dlg);

	ATTACH (table, fp_dlg->suid, 4, 5, 1, 2);
	ATTACH (table, fp_dlg->sgid, 4, 5, 2, 3);
	ATTACH (table, fp_dlg->svtx, 4, 5, 3, 4);

	perm_set_mode_label (NULL, fp_dlg);
	gtk_label_get (GTK_LABEL (fp_dlg->mode_label), &mode);
	fp_dlg->mode_name = g_strdup (mode);
	return frame;
}

#undef ATTACH
#undef PERMSET

static GtkWidget *
perm_owner_new (GnomeFilePropertyDialog *fp_dlg)
{
	GtkWidget *gentry;
	struct passwd *passwd;

	gentry = gtk_entry_new ();

	if (fp_dlg->euid != 0)
		gtk_widget_set_sensitive (gentry, FALSE);
	passwd = getpwuid (fp_dlg->st.st_uid);
	if (passwd) {
		fp_dlg->user_name = passwd->pw_name;
		gtk_entry_set_text (GTK_ENTRY (gentry), passwd->pw_name);
	} else {
		char buf[100];

		g_snprintf (buf, sizeof (buf), "%d", (int) fp_dlg->st.st_uid);
		gtk_entry_set_text (GTK_ENTRY (gentry), buf);
	}

	return gentry;
}

static GtkWidget *
perm_group_new (GnomeFilePropertyDialog *fp_dlg)
{
	GtkWidget *gentry;
	struct group *grp;
	gboolean grp_flag = FALSE;
	gchar *grpname = NULL;
	GList *templist;
	GList *list = NULL;

	/* Are we root?  Do we own the file? */
	/* In this case we can change it. */
	/* A little bit of this was swiped from kfm. */

	if ((fp_dlg->euid == 0) || (fp_dlg->st.st_uid == fp_dlg->euid)) {
		gentry = gtk_combo_new ();
		grp = getgrgid (fp_dlg->st.st_gid);
		if (grp){
			if (grp->gr_name)
				fp_dlg->group_name = g_strdup (grp->gr_name);
			else {
				fp_dlg->group_name = g_strdup_printf ("%d", (int) grp->gr_gid);
			}
		} else {
			fp_dlg->group_name = g_strdup_printf ("%d", (int) fp_dlg->st.st_gid);
		}

		/* we change this, b/c we are able to set the egid, if we aren't in the group already */
		grp = getgrgid (getegid());
		if (grp) {
			if (grp->gr_name)
				grpname = g_strdup (grp->gr_name);
			else
				grpname = g_strdup_printf ("%d", (int) grp->gr_gid);
		} else
			grpname = g_strdup_printf ("%d", (int) fp_dlg->st.st_gid);

		if (fp_dlg->euid != 0)
			gtk_editable_set_editable (GTK_EDITABLE (GTK_COMBO (gentry)->entry), FALSE);
		for (setgrent (); (grp = getgrent ()) != NULL;) {
			if (!grp_flag && grpname && !strcmp (grp->gr_name, grpname)) {
				list = g_list_insert_sorted (list, g_strdup (grp->gr_name), (GCompareFunc)strcmp);
				grp_flag = TRUE;
				continue;
			}
			if (fp_dlg->euid == 0) {
				list = g_list_insert_sorted (list, g_strdup (grp->gr_name), (GCompareFunc)strcmp);
			} else {
				gchar **members = grp->gr_mem;
				gchar *member;
				while ((member = *members) != 0L) {
					if (!strcmp (member, fp_dlg->user_name)) {
						list = g_list_insert_sorted (list, g_strdup (grp->gr_name), (GCompareFunc)strcmp);
						break;
					}
					++members;
				}
			}
		}
		endgrent ();

		/* we also might want to add the egid to the list. */
		if (!grp_flag)
			list = g_list_insert_sorted (list, g_strdup (grpname), (GCompareFunc)strcmp);

		/* Now we have a list.  We should make our option menu. */
		gtk_combo_set_popdown_strings (GTK_COMBO (gentry), list);
		for (templist = list; templist; templist = templist->next) {
			g_free (templist->data);
		}
		gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (gentry)->entry), fp_dlg->group_name);
		g_list_free (list);
	} else {
		/* we're neither so we just put an entry down */
		gentry = gtk_entry_new ();
		gtk_widget_set_sensitive (gentry, FALSE);

		grp = getgrgid (fp_dlg->st.st_gid);
		if (grp)
			gtk_entry_set_text (GTK_ENTRY (gentry), grp->gr_name);
		else {
			char buf[100];

			g_snprintf (buf, sizeof (buf), _("<Unknown> (%d)"), fp_dlg->st.st_gid);
			gtk_entry_set_text (GTK_ENTRY (gentry), buf);
		}
	}
	g_free (grpname);
	return gentry;
}

static GtkWidget *
perm_ownership_new (GnomeFilePropertyDialog *fp_dlg)
{
	GtkWidget *frame;
	GtkWidget *table;

	frame = gtk_frame_new (_("File ownership"));

	table = gtk_table_new (2, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);
	gtk_table_set_row_spacings (GTK_TABLE (table), 4);
	gtk_container_add (GTK_CONTAINER (frame), table);
	gtk_widget_show (table);

	/* Owner */

	gtk_table_attach (GTK_TABLE (table), label_new (_("Owner"), 0.0, 0.5),
			  0, 1, 0, 1,
			  GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK,
			  0, 0);

	fp_dlg->owner_entry = perm_owner_new (fp_dlg);
	gtk_table_attach (GTK_TABLE (table), fp_dlg->owner_entry,
			  1, 2, 0, 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_widget_show (fp_dlg->owner_entry);

	/* Group */

	gtk_table_attach (GTK_TABLE (table), label_new (_("Group"), 0.0, 0.5),
			  0, 1, 1, 2,
			  GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK,
			  0, 0);

	fp_dlg->group_entry = perm_group_new (fp_dlg);
	gtk_table_attach (GTK_TABLE (table), fp_dlg->group_entry,
			  1, 2, 1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_widget_show (fp_dlg->group_entry);
	return frame;
}

static GtkWidget *
create_perm_properties (GnomeFilePropertyDialog *fp_dlg)
{
	GtkWidget *vbox;

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (vbox), perm_mode_new (fp_dlg), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), perm_ownership_new (fp_dlg), TRUE, TRUE, 0);
	return vbox;
}

/* finally the new dialog */
static void
init_metadata (GnomeFilePropertyDialog *fp_dlg)
{
	gint size;
	char *mime_type, *str;
	gchar link_name[MC_MAXPATHLEN];
	gint n;
	gchar *file_name, *desktop_url;

	if (gnome_metadata_get (fp_dlg->file_name, "fm-open", &size, &fp_dlg->fm_open) != 0)
		gnome_metadata_get (fp_dlg->file_name, "open", &size, &fp_dlg->fm_open);
	if (gnome_metadata_get (fp_dlg->file_name, "fm-view", &size, &fp_dlg->fm_view) != 0)
		gnome_metadata_get (fp_dlg->file_name, "view", &size, &fp_dlg->fm_view);
	gnome_metadata_get (fp_dlg->file_name, "edit", &size, &fp_dlg->edit);
	gnome_metadata_get (fp_dlg->file_name, "drop-action", &size, &fp_dlg->drop_target);

	/*
	 * Fetch the needs-terminal setting
	 */
	if (gnome_metadata_get (fp_dlg->file_name, "flags", &size, &str) == 0){
		fp_dlg->needs_terminal = strstr (str, "needsterminal") != 0;
		g_free (str);
	} else
		fp_dlg->needs_terminal = 0;

	/* Mime stuff */
	file_name = fp_dlg->file_name;
	if (S_ISLNK (fp_dlg->st.st_mode)) {
		n = mc_readlink (fp_dlg->file_name, link_name, MC_MAXPATHLEN);
		if (n > 0){
			link_name [n] = 0;
			file_name = link_name;
		}
	}

	if (gnome_metadata_get (fp_dlg->file_name, "desktop-url", &size, &desktop_url) == 0)
		fp_dlg->desktop_url = desktop_url;
	else
		fp_dlg->desktop_url = NULL;

	if (gnome_metadata_get (fp_dlg->file_name, "icon-caption", &size, &fp_dlg->caption))
		fp_dlg->caption = g_strdup (desktop_url);

	/*
	 * Mime type.
	 */
	mime_type = (char *) gnome_mime_type_or_default (file_name, NULL);
	if (!mime_type)
		return;
	fp_dlg->mime_fm_open = gnome_mime_get_value (mime_type, "fm-open");
	if (!fp_dlg->mime_fm_open)
		fp_dlg->mime_fm_open = gnome_mime_get_value (mime_type, "open");
	fp_dlg->mime_fm_view = gnome_mime_get_value (mime_type, "fm-view");
	if (!fp_dlg->mime_fm_view)
		fp_dlg->mime_fm_view = gnome_mime_get_value (mime_type, "view");
	fp_dlg->mime_edit = gnome_mime_get_value (mime_type, "edit");
	fp_dlg->mime_drop_target = gnome_mime_get_value (mime_type, "drop-action");
}

GtkWidget *
gnome_file_property_dialog_new (gchar *file_name, gboolean can_set_icon)
{
	GnomeFilePropertyDialog *fp_dlg;
	GtkWidget *notebook;
	GtkWidget *new_page;
	gchar *title_string;
	mode_t mode;

	g_return_val_if_fail (file_name != NULL, NULL);
	fp_dlg = gtk_type_new (gnome_file_property_dialog_get_type ());

	/* We set non-gui specific things first */
	if (mc_lstat (file_name, &fp_dlg->st) == -1) {
		/* Bad things man, bad things */
		gtk_object_unref (GTK_OBJECT (fp_dlg));
		return NULL;
	}

	mode = fp_dlg->st.st_mode;
	if (S_ISLNK (mode)){
		struct stat s;

		if (mc_stat (file_name, &s) != -1)
			mode = s.st_mode;
	}

	if (fp_dlg->st.st_mode & (S_IXUSR | S_IXGRP |S_IXOTH)) {
		fp_dlg->executable = TRUE;
	} else {
		fp_dlg->executable = FALSE;
	}

	fp_dlg->file_name = g_strdup (file_name);
	fp_dlg->euid = geteuid ();
	fp_dlg->can_set_icon = can_set_icon;
	if (S_ISCHR (fp_dlg->st.st_mode) || S_ISBLK (fp_dlg->st.st_mode)
	    || ((fp_dlg->euid != fp_dlg->st.st_uid) && (fp_dlg->euid != 0)))
		fp_dlg->modifyable = FALSE;
	init_metadata (fp_dlg);

	/* and now, we set up the gui. */
	notebook = gtk_notebook_new ();

	if (fp_dlg->desktop_url)
		gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
					  create_url_properties (fp_dlg),
					  gtk_label_new (_("URL")));
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
				  create_general_properties (fp_dlg),
				  gtk_label_new (_("Statistics")));

	new_page = create_settings_pane (fp_dlg);
	if (new_page)
		gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
					  new_page,
					  gtk_label_new (_("Options")));
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
				  create_perm_properties (fp_dlg),
				  gtk_label_new (_("Permissions")));
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (fp_dlg)->vbox),
			    notebook, TRUE, TRUE, 0);
	title_string = g_strconcat (strrchr (file_name, '/') + 1, _(" Properties"), NULL);
	gtk_window_set_title (GTK_WINDOW (fp_dlg), title_string);
	g_free (title_string);

	gnome_dialog_append_button ( GNOME_DIALOG(fp_dlg),
				     GNOME_STOCK_BUTTON_OK);
	gnome_dialog_append_button ( GNOME_DIALOG(fp_dlg),
				     GNOME_STOCK_BUTTON_CANCEL);
	gtk_widget_show_all (GNOME_DIALOG (fp_dlg)->vbox);

	/* It's okay to do this now... */
	/* and set the rest of the fields */
	switch_metadata_box (fp_dlg);

	return GTK_WIDGET (fp_dlg);
}

static gint
apply_mode_change (GnomeFilePropertyDialog *fpd)
{
	gchar *new_mode;
	gtk_label_get (GTK_LABEL (fpd->mode_label), &new_mode);
	if (strcmp (new_mode, fpd->mode_name)) {
		mc_chmod (fpd->file_name, (mode_t) strtol(new_mode, (char **)NULL, 8));
		return 1;
	}
	return 0;
}

static gint
apply_uid_group_change (GnomeFilePropertyDialog *fpd)
{
	uid_t uid;
	gid_t gid;
	struct passwd *p;
	struct group  *g;
	gchar *new_user_name = NULL;
	gchar *new_group_name = NULL;

	uid = fpd->st.st_uid;
	gid = fpd->st.st_gid;

	/* we only check if our euid == 0 */
	if (fpd->euid == 0) {
		new_user_name = gtk_entry_get_text (GTK_ENTRY (fpd->owner_entry));

		if (fpd->user_name && new_user_name && strcmp (fpd->user_name, new_user_name)) {
			/* now we need to get the new uid */
			p = getpwnam (new_user_name);
			if (!p) {
				uid = atoi (new_user_name);
				if (uid == 0) {
					message (1, "Error", _("You entered an invalid username"));
					uid = fpd->st.st_uid;
				}
			} else
				uid = p->pw_uid;
		} else {
			if (new_user_name){
				p = getpwnam (new_user_name);
				if (!p) {
					uid = atoi (new_user_name);
					if (uid == 0) {
						message (1, "Error", _("You entered an invalid username"));
						uid = fpd->st.st_uid;
					}
				} else
					uid = p->pw_uid;
			}
		}
	}

	/* now we check the group */
	/* We are only a combo if we are sensitive, and we only want to check if we are
	 * sensitive. */
	if (GTK_WIDGET_IS_SENSITIVE (fpd->group_entry))
		new_group_name = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (fpd->group_entry)->entry));

	if (fpd->group_name && new_group_name && strcmp (fpd->group_name, new_group_name)) {
		g = getgrnam (new_group_name);
		if (!g) {
			gid = atoi (new_group_name);
			if (gid == 0) {
				message (1, "Error", "You entered an invalid group name");
				gid = fpd->st.st_gid;
			}
		} else
			gid = g->gr_gid;
	}
	if ((uid != fpd->st.st_uid) || (gid != fpd->st.st_gid)) {
		mc_chown (fpd->file_name, uid, gid);
		return 1;
	}
	return 0;
}

static gint
apply_name_change (GnomeFilePropertyDialog *fpd)
{
	char *new_name;
	char *base_name;
	char *full_target;
	FileOpContext *ctx;
	long   count = 0;
	double bytes = 0;

	new_name = gtk_entry_get_text (GTK_ENTRY (fpd->file_entry));
	if (!*new_name) {
		message (1, "Error", _("You must rename your file to something"));
		return 0;
	}
	/* has it changed? */
	if (strcmp (strrchr(fpd->file_name, '/') + 1, new_name)) {
		if (strchr (new_name, '/')) {
			message (1, "Error", _("You cannot rename a file to something containing a '/' character"));
			return 0;
		} else {
			char *p;
			int s;

			/* create the files. */
			base_name = g_strdup (fpd->file_name);

			p = strrchr (base_name, '/');

			if (p)
				*p = '\0';

			full_target = concat_dir_and_file (base_name, new_name);

			ctx = file_op_context_new ();
			file_op_context_create_ui (ctx, OP_MOVE, FALSE);
			s = move_file_file (ctx, fpd->file_name, full_target, &count, &bytes);
			file_op_context_destroy (ctx);

			if (s == FILE_CONT){
				g_free (fpd->file_name);
				fpd->file_name = full_target;
			} else
				g_free (full_target);
		}
	}
	return 1;
}

static gint
apply_metadata_change (GnomeFilePropertyDialog *fpd)
{
	gchar *text;

	/* If we don't have an open_cbox, that means we have no metadata
	 * to set.
	 */
	if (fpd->open_cbox != NULL) {
		if (!GTK_TOGGLE_BUTTON (fpd->open_cbox)->active) {
			text = gtk_entry_get_text (GTK_ENTRY (fpd->open_entry));
			if (text && text[0])
				gnome_metadata_set (fpd->file_name,
						    "fm-open",
						    strlen (text) + 1,
						    text);
			else
				gnome_metadata_remove (fpd->file_name,
						       "fm-open");
		} else {
			if (fpd->fm_open)
				gnome_metadata_remove (fpd->file_name,
						       "fm-open");
		}
		if (fpd->executable) {
			if (!GTK_TOGGLE_BUTTON (fpd->prop1_cbox)->active) {
				text = gtk_entry_get_text (GTK_ENTRY (fpd->prop1_entry));
				if (text && text[0])
					gnome_metadata_set (fpd->file_name,
							    "drop-action",
							    strlen (text) + 1,
							    text);
				else
					gnome_metadata_remove (fpd->file_name,
							       "drop-action");
			} else {
				if (fpd->drop_target)
					gnome_metadata_remove (fpd->file_name,
							       "drop-action");
			}
		} else {
			if (!GTK_TOGGLE_BUTTON (fpd->prop1_cbox)->active) {
				text = gtk_entry_get_text (GTK_ENTRY (fpd->prop1_entry));
				if (text && text[0])
					gnome_metadata_set (fpd->file_name,
							    "fm-view",
							    strlen (text) + 1,
							    text);
				else
					gnome_metadata_remove (fpd->file_name,
							       "fm-view");
			} else {
				if (fpd->fm_view)
					gnome_metadata_remove (fpd->file_name,
							       "fm-view");
			}
			if (!GTK_TOGGLE_BUTTON (fpd->prop2_cbox)->active) {
				text = gtk_entry_get_text (GTK_ENTRY (fpd->prop2_entry));
				if (text && text[0])
					gnome_metadata_set (fpd->file_name,
							    "edit",
							    strlen (text) + 1,
							    text);
				else
					gnome_metadata_remove (fpd->file_name,
							       "edit");
			} else {
				if (fpd->edit)
					gnome_metadata_remove (fpd->file_name,
							       "edit");
			}
		}
	}

	if (fpd->desktop_url){
		char *new_desktop_url = gtk_entry_get_text (GTK_ENTRY (fpd->desktop_entry));
		char *new_caption     = gtk_entry_get_text (GTK_ENTRY (fpd->caption_entry));

		gnome_metadata_set (fpd->file_name, "desktop-url",
				    strlen (new_desktop_url)+1, new_desktop_url);
		gnome_metadata_set (fpd->file_name, "icon-caption",
				    strlen (new_caption)+1, new_caption);
	}

	/*
	 * "Needs terminal" check button
	 *
	 * This piece of code is more complex than it should, as the "flags"
	 * metadata value is actually a list of comma separated flags.  So
	 * we need to edit this list when removing the "needsterminal" flag.
	 */
	if (fpd->needs_terminal_check)
	{
		int toggled = 0 != GTK_TOGGLE_BUTTON (fpd->needs_terminal_check)->active;
		int size;
		char *buf;

		if (gnome_metadata_get (fpd->file_name, "flags", &size, &buf) == 0){
			char *p;

			p = strstr (buf, "needsterminal");
			if (toggled){
				if (!p){
					p = g_strconcat (buf, ",needsterminal", NULL);
					gnome_metadata_set (fpd->file_name, "flags", strlen (p)+1, p);
					g_free (p);
				}
			} else {
				if (p){
					char *p1, *p2;

					if (p != buf){
						p1 = g_malloc (p - buf + 1);
						strncpy (p1, buf, p - buf);
						p1 [p - buf ] = 0;
					} else
						p1 = g_strdup ("");

					p += strlen ("needsterminal");

					p2 = g_strconcat (p1, p, NULL);
					if (strcmp (p2, ",") == 0)
						gnome_metadata_remove (fpd->file_name, "flags");
					else
						gnome_metadata_set (fpd->file_name, "flags",
								    strlen (p2)+1, p2);
					g_free (p2);
					g_free (p1);

				}
			}
		} else {
			if (toggled){
				gnome_metadata_set (fpd->file_name, "flags",
						    sizeof ("needsterminal"), "needsterminal");
			}
		}
	}

	if (!fpd->can_set_icon)
		return 1;
	/* And finally, we set the metadata on the icon filename */
	text = gnome_icon_entry_get_filename (GNOME_ICON_ENTRY (fpd->button));
	/*gtk_entry_get_text (GTK_ENTRY (gnome_icon_entry_gtk_entry (GNOME_ICON_ENTRY (fpd->button))));*/

	if (text) {
		if (fpd->icon_filename == NULL || strcmp (fpd->icon_filename, text) != 0)
			gnome_metadata_set (fpd->file_name, "icon-filename", strlen (text) + 1, text);
		g_free (text);
	}
	/* I suppose we should only do this if we know there's been a change -- I'll try to figure it
	 * out later if it turns out to be important. */
	return 1;
}

/* This function will apply what changes it can do.  It is meant to be used in conjunction
 * with a gnome_dialog_run_and_hide.  Please note that doing a gnome_dialog_run
 * will (obviously) cause greivious bogoness. */

gint
gnome_file_property_dialog_make_changes (GnomeFilePropertyDialog *file_property_dialog)
{
	gint retval = 0;

	g_return_val_if_fail (file_property_dialog != NULL, 1);
	g_return_val_if_fail (GNOME_IS_FILE_PROPERTY_DIALOG (file_property_dialog), 1);

	retval += apply_mode_change (file_property_dialog);
	retval += apply_uid_group_change (file_property_dialog);
	retval += apply_name_change (file_property_dialog);
	retval += apply_metadata_change (file_property_dialog);

	return retval;
}

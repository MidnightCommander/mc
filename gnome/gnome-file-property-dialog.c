/* gnome-file-property-dialog.c
 * Copyright (C) 1999  J. Arthur Random
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

#include <gnome.h>
#include <time.h>
#include "gnome-file-property-dialog.h"
#include "dir.h"
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fileopctx.h"
#include "file.h"

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
	file_property_dialog->file_name = NULL;
	file_property_dialog->file_entry = NULL;
	file_property_dialog->group_name = NULL;
	file_property_dialog->user_name = NULL;
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
	gchar size[50]; /* this is a HUGE file. (: */
	gchar size2[20]; /* this is a HUGE file. (: */
	file_entry *fe;
	GdkImlibImage *im;
	GtkWidget *icon;
	struct tm *time;
	GtkWidget *table;
	int n;

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD);

        /* first, we set the icon */
	direc = g_strdup (fp_dlg->file_name);
	rindex (direc, '/')[0] = NULL;
	fe = file_entry_from_file (fp_dlg->file_name);
	im = gicon_get_icon_for_file (direc, fe);
	g_free (direc);
	icon = gnome_pixmap_new_from_imlib (im);
	gdk_imlib_destroy_image (im);
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
	gen_string = rindex (fp_dlg->file_name, '/');
	if (gen_string)
		gtk_entry_set_text (GTK_ENTRY (fp_dlg->file_entry), gen_string + 1);
	else
		/* I don't think this should happen anymore, but it can't hurt in
		 * case this ever gets used outside gmc */
		gtk_entry_set_text (GTK_ENTRY (fp_dlg->file_entry), fp_dlg->file_name);
	/* We want to prevent editing of the file if
	 * the thing is a BLK, CHAR, or we don't own it */
	/* FIXME: We can actually edit it if we're in the same grp of the file. */
	if (S_ISCHR (fp_dlg->st.st_mode) || S_ISBLK (fp_dlg->st.st_mode)
	    || ((fp_dlg->euid != fp_dlg->st.st_uid) && (fp_dlg->euid != 0)))
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
		n = readlink (fp_dlg->file_name, size, 49);
		if (n < 0)
			label = gtk_label_new (_("Target Name: INVALID LINK"));
		else {
			size[n] = '\0';
			gen_string = g_strconcat (_("Target Name: "), size, NULL);
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
			snprintf (size, 19, "%d", (gint) fp_dlg->st.st_size);
			gen_string = g_strconcat (_("File Size: "), size, _(" bytes"), NULL);
		} else if ((gint)fp_dlg->st.st_size < 1024 * 1024) {
			snprintf (size, 19, "%.1f", (gfloat) fp_dlg->st.st_size / 1024.0);
			snprintf (size2, 19, "%d", (gint) fp_dlg->st.st_size);
			gen_string = g_strconcat (_("File Size: "), size, _(" KBytes  ("),
						  size2, _(" bytes)"), NULL);
		} else {
			snprintf (size, 19, "%.1f", (gfloat) fp_dlg->st.st_size / (1024.0 * 1024.0));
			snprintf (size2, 19, "%d", (gint) fp_dlg->st.st_size);
			gen_string = g_strconcat (_("File Size: "), size, _(" MBytes  ("),
						  size2, _(" bytes)"), NULL);
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
	time = gmtime (&(fp_dlg->st.st_ctime));
	strftime (size, 49, "%a, %b %d %Y,  %I:%M:%S %p", time);
	label = gtk_label_new (size);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 1, 2, 0, 1);

	label = gtk_label_new (_("Last Modified on: "));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);
	time = gmtime (&(fp_dlg->st.st_mtime));
	strftime (size, 49, "%a, %b %d %Y,  %I:%M:%S %p", time);
	label = gtk_label_new (size);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 1, 2, 1, 2);

	label = gtk_label_new (_("Last Accessed on: "));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);
	time = gmtime (&(fp_dlg->st.st_atime));
	strftime (size, 49, "%a, %b %d %Y,  %I:%M:%S %p", time);
	label = gtk_label_new (size);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 1, 2, 2, 3);
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

	frame = gtk_frame_new (_("File mode (permissions)"));

	vbox = gtk_vbox_new (FALSE, 4);
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
	} else
		gtk_entry_set_text (GTK_ENTRY (gentry), "<Unknown>");

	return gentry;
}
static GtkWidget *
perm_group_new (GnomeFilePropertyDialog *fp_dlg)
{
	GtkWidget *gentry;
	struct group *grp;
	gchar grpnum [10];
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
		if (grp->gr_name)
			fp_dlg->group_name = g_strdup (grp->gr_name);
		else {
			g_snprintf (grpnum, 9, "%d", grp->gr_gid);
			fp_dlg->group_name = g_strdup (grpnum);
		}

		/* we change this, b/c we are able to set the egid, if we aren't in the group already */
		grp = getgrgid (getegid());
		if (grp) {
			if (grp->gr_name)
				grpname = grp->gr_name;
			else {
				g_snprintf (grpnum, 9, "%d", grp->gr_gid);
				grpname = grpnum;
			}
		}
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
		grp = getgrgid (fp_dlg->st.st_gid);
		gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (gentry)->entry), grp->gr_name);
		g_list_free (list);
	} else {
		/* we're neither so we just put an entry down */
		gentry = gtk_entry_new ();
		gtk_widget_set_sensitive (gentry, FALSE);
		grp = getgrgid (fp_dlg->st.st_gid);
		gtk_entry_set_text (GTK_ENTRY (gentry), grp->gr_name);
	}
	return gentry;
}

static GtkWidget *
perm_ownership_new (GnomeFilePropertyDialog *fp_dlg)
{
	GtkWidget *frame;
	GtkWidget *table;

	frame = gtk_frame_new ("File ownership");

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
	gtk_box_pack_start (GTK_BOX (vbox), perm_ownership_new (fp_dlg), FALSE, FALSE, 0);
	return vbox;
} 
/* finally the new dialog */
GtkWidget *
gnome_file_property_dialog_new (gchar *file_name)
{
	GnomeFilePropertyDialog *fp_dlg;
	GtkWidget *notebook;
	gchar *title_string;
	g_return_val_if_fail (file_name != NULL, NULL);
	
	fp_dlg = gtk_type_new (gnome_file_property_dialog_get_type ());
	if (mc_lstat (file_name, &fp_dlg->st) == -1) {
		/* Bad things man, bad things */
		gtk_object_unref (GTK_OBJECT (fp_dlg));
		return NULL;
	}

	fp_dlg->file_name = g_strdup (file_name);
	fp_dlg->euid = geteuid ();
	/* ugh:  This is sort of messy -- we need to standardize on fe's by next mc. */
	notebook = gtk_notebook_new ();
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
				  create_general_properties (fp_dlg),
				  gtk_label_new (_("General")));
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
				  create_perm_properties (fp_dlg),
				  gtk_label_new (_("Permissions")));
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (fp_dlg)->vbox),
			    notebook, TRUE, TRUE, 0);
	title_string = g_strconcat (rindex (file_name, '/') + 1, _(" Properties"), NULL);
	gtk_window_set_title (GTK_WINDOW (fp_dlg), title_string);
	g_free (title_string);
	gnome_dialog_append_button ( GNOME_DIALOG(fp_dlg), 
				     GNOME_STOCK_BUTTON_OK);
	gnome_dialog_append_button ( GNOME_DIALOG(fp_dlg), 
				     GNOME_STOCK_BUTTON_CANCEL);
	gtk_widget_show_all (GNOME_DIALOG (fp_dlg)->vbox);
	return GTK_WIDGET (fp_dlg);
}
static gint
apply_mode_change (GnomeFilePropertyDialog *fpd)
{
	gchar *new_mode;
	gtk_label_get (GTK_LABEL (fpd->mode_label), &new_mode);
	if (strcmp (new_mode, fpd->mode_name)) {
		g_print ("changed mode:%d:\n", (mode_t) atoi (new_mode));
		mc_chmod (fpd->file_name, (mode_t) atoi (new_mode));
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
		if (new_user_name && strcmp (fpd->user_name, new_user_name)) {
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
		}

	}

	/* now we check the group */
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
	gchar *new_name;
	gchar *base_name;
	gchar *full_target;
	FileOpContext *ctx;
	long   count = 0;
	double bytes = 0;

	new_name = gtk_entry_get_text (GTK_ENTRY (fpd->file_entry));
	if (!*new_name) {
		message (1, "Error", _("You must rename your file to something"));
		return 0;
	}
	/* has it changed? */
	if (strcmp (rindex(fpd->file_name, '/') + 1, new_name)) {
		if (strchr (new_name, '/')) {
			message (1, "Error", _("You cannot rename a file to something containing a '/' character"));
			return 0;
		} else {
			/* create the files. */
			base_name = g_strdup (fpd->file_name);
			rindex (base_name, '/')[0] = '\0';
			full_target = concat_dir_and_file (base_name, new_name);

			ctx = file_op_context_new ();
			file_op_context_create_ui (ctx, OP_MOVE, FALSE);
			move_file_file (ctx, fpd->file_name, full_target, &count, &bytes);
			file_op_context_destroy (ctx);
			g_free (full_target);
		}
	}
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

	return retval;
}

/* Properties dialog for the Gnome edition of the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <gnome.h>
#include "gprop.h"


static GtkWidget *
label_new (char *text, double xalign, double yalign)
{
	GtkWidget *label;

	label = gtk_label_new (text);
	gtk_misc_set_alignment (GTK_MISC (label), xalign, yalign);
	gtk_widget_show (label);
	return label;
}

/***** General *****/

GpropGeneral *
gprop_general_new (char *complete_filename, char *filename)
{
	GpropGeneral *gpg;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	char *s;

	g_return_val_if_fail (complete_filename != NULL, NULL);
	g_return_val_if_fail (filename != NULL, NULL);

	gpg = g_new (GpropGeneral, 1);

	gpg->top = gtk_vbox_new (FALSE, 6);

	frame = gtk_frame_new ("Name");
	gtk_box_pack_start (GTK_BOX (gpg->top), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_border_width (GTK_CONTAINER (vbox), 6);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_widget_show (vbox);

	s = g_copy_strings ("Full name: ", complete_filename, NULL);
	gtk_box_pack_start (GTK_BOX (vbox), label_new (s, 0.0, 0.5), FALSE, FALSE, 0);
	g_free (s);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	gtk_box_pack_start (GTK_BOX (hbox), label_new ("Name", 0.0, 0.5), FALSE, FALSE, 0);

	gpg->filename = gnome_entry_new ("gprop_general_filename");
	gtk_entry_set_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (gpg->filename))), filename);
	gtk_box_pack_start (GTK_BOX (hbox), gpg->filename, TRUE, TRUE, 0);
	gtk_widget_show (gpg->filename);

	return gpg;
}

void
gprop_general_get_data (GpropGeneral *gpg, char **filename)
{
	GtkWidget *entry;

	g_return_if_fail (gpg != NULL);

	if (filename) {
		entry = gnome_entry_gtk_entry (GNOME_ENTRY (gpg->filename));
		*filename = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
	}
}

/***** Permissions *****/

static umode_t
perm_get_umode (GpropPerm *gpp)
{
	umode_t umode;

#define SETBIT(widget, flag) umode |= GTK_TOGGLE_BUTTON (widget)->active ? flag : 0

	umode = 0;

	SETBIT (gpp->suid, S_ISUID);
	SETBIT (gpp->sgid, S_ISGID);
	SETBIT (gpp->svtx, S_ISVTX);
	
	SETBIT (gpp->rusr, S_IRUSR);
	SETBIT (gpp->wusr, S_IWUSR);
	SETBIT (gpp->xusr, S_IXUSR);
	
	SETBIT (gpp->rgrp, S_IRGRP);
	SETBIT (gpp->wgrp, S_IWGRP);
	SETBIT (gpp->xgrp, S_IXGRP);
	
	SETBIT (gpp->roth, S_IROTH);
	SETBIT (gpp->woth, S_IWOTH);
	SETBIT (gpp->xoth, S_IXOTH);

	return umode;
	
#undef SETBIT
}

static void
perm_set_mode_label (GtkWidget *widget, gpointer data)
{
	umode_t umode;
	GpropPerm *gpp;
	char s_mode[5];
	
	gpp = data;

	umode = perm_get_umode (gpp);

	s_mode[0] = '0' + ((umode & (S_ISUID | S_ISGID | S_ISVTX)) >> 9);
	s_mode[1] = '0' + ((umode & (S_IRUSR | S_IWUSR | S_IXUSR)) >> 6);
	s_mode[2] = '0' + ((umode & (S_IRGRP | S_IWGRP | S_IXGRP)) >> 3);
	s_mode[3] = '0' + ((umode & (S_IROTH | S_IWOTH | S_IXOTH)) >> 0);
	s_mode[4] = 0;

	gtk_label_set (GTK_LABEL (gpp->mode_label), s_mode);
}

static GtkWidget *
perm_check_new (char *text, int state, GpropPerm *gpp)
{
	GtkWidget *check;

	if (text)
		check = gtk_check_button_new_with_label (text);
	else
		check = gtk_check_button_new ();

	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (check), FALSE);
	gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (check), state ? TRUE : FALSE);

	gtk_signal_connect (GTK_OBJECT (check), "toggled",
			    (GtkSignalFunc) perm_set_mode_label,
			    gpp);

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
	r = perm_check_new (NULL, umode & rmask, gpp);			\
	w = perm_check_new (NULL, umode & wmask, gpp);			\
	x = perm_check_new (NULL, umode & xmask, gpp);			\
									\
	ATTACH (table, label_new (name, 0.0, 0.5), 0, 1, y, y + 1);	\
	ATTACH (table, r, 1, 2, y, y + 1);				\
	ATTACH (table, w, 2, 3, y, y + 1);				\
	ATTACH (table, x, 3, 4, y, y + 1);				\
} while (0);
	
static GtkWidget *
perm_mode_new (GpropPerm *gpp, umode_t umode)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *table;

	frame = gtk_frame_new ("File mode (permissions)");

	vbox = gtk_vbox_new (FALSE, 4);
	gtk_container_border_width (GTK_CONTAINER (vbox), 6);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_widget_show (vbox);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	gtk_box_pack_start (GTK_BOX (hbox), label_new ("Current mode: ", 0.0, 0.5), FALSE, FALSE, 0);

	gpp->mode_label = label_new ("0000", 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), gpp->mode_label, FALSE, FALSE, 0);

	table = gtk_table_new (4, 5, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (table), 4);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
	gtk_widget_show (table);

	/* Headings */

	ATTACH (table, label_new ("Read", 0.0, 0.5),     1, 2, 0, 1);
	ATTACH (table, label_new ("Write", 0.0, 0.5),    2, 3, 0, 1);
	ATTACH (table, label_new ("Exec", 0.0, 0.5),     3, 4, 0, 1);
	ATTACH (table, label_new ("Special", 0.0, 0.5),  4, 5, 0, 1);

	/* Permissions */

	PERMSET ("User",  gpp->rusr, gpp->wusr, gpp->xusr, S_IRUSR, S_IWUSR, S_IXUSR, 1);
	PERMSET ("Group", gpp->rgrp, gpp->wgrp, gpp->xgrp, S_IRGRP, S_IWGRP, S_IXGRP, 2);
	PERMSET ("Other", gpp->roth, gpp->woth, gpp->xoth, S_IROTH, S_IWOTH, S_IXOTH, 3);

	/* Special */

	gpp->suid = perm_check_new ("Set UID", umode & S_ISUID, gpp);
	gpp->sgid = perm_check_new ("Set GID", umode & S_ISGID, gpp);
	gpp->svtx = perm_check_new ("Sticky",  umode & S_ISVTX, gpp);
	
	ATTACH (table, gpp->suid, 4, 5, 1, 2);
	ATTACH (table, gpp->sgid, 4, 5, 2, 3);
	ATTACH (table, gpp->svtx, 4, 5, 3, 4);

	perm_set_mode_label (NULL, gpp);
	
	return frame;
}

#undef ATTACH
#undef PERMSET

static GtkWidget *
perm_owner_new (char *owner)
{
	GtkWidget *gentry;
	GtkWidget *entry;

	/* FIXME: this should be a nice pull-down list of user names, as in achown.c */

	gentry = gnome_entry_new ("gprop_perm_owner");
	entry = gnome_entry_gtk_entry (GNOME_ENTRY (gentry));
	gtk_entry_set_text (GTK_ENTRY (entry), owner);

	return gentry;
}

static GtkWidget *
perm_group_new (char *group)
{
	GtkWidget *gentry;
	GtkWidget *entry;

	/* FIXME: this should be a nice pull-down list of group names, as in achown.c */

	gentry = gnome_entry_new ("gprop_perm_group");
	entry = gnome_entry_gtk_entry (GNOME_ENTRY (gentry));
	gtk_entry_set_text (GTK_ENTRY (entry), group);

	return gentry;
}

static GtkWidget *
perm_ownership_new (GpropPerm *gpp, char *owner, char *group)
{
	GtkWidget *frame;
	GtkWidget *table;

	frame = gtk_frame_new ("File ownership");

	table = gtk_table_new (2, 2, FALSE);
	gtk_container_border_width (GTK_CONTAINER (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);
	gtk_table_set_row_spacings (GTK_TABLE (table), 4);
	gtk_container_add (GTK_CONTAINER (frame), table);
	gtk_widget_show (table);

	/* Owner */

	gtk_table_attach (GTK_TABLE (table), label_new ("Owner", 0.0, 0.5),
			  0, 1, 0, 1,
			  GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK,
			  0, 0);

	gpp->owner = perm_owner_new (owner);
	gtk_table_attach (GTK_TABLE (table), gpp->owner,
			  1, 2, 0, 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_widget_show (gpp->owner);

	/* Group */

	gtk_table_attach (GTK_TABLE (table), label_new ("Group", 0.0, 0.5),
			  0, 1, 1, 2,
			  GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK,
			  0, 0);

	gpp->group = perm_group_new (group);
	gtk_table_attach (GTK_TABLE (table), gpp->group,
			  1, 2, 1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_widget_show (gpp->group);

	return frame;
}

GpropPerm *
gprop_perm_new (umode_t umode, char *owner, char *group)
{
	GpropPerm *gpp;
	GtkWidget *w;

	g_return_val_if_fail (owner != NULL, NULL);
	g_return_val_if_fail (group != NULL, NULL);

	gpp = g_new (GpropPerm, 1);

	gpp->top = gtk_vbox_new (FALSE, 6);

	w = perm_mode_new (gpp, umode);
	gtk_box_pack_start (GTK_BOX (gpp->top), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	w = perm_ownership_new (gpp, owner, group);
	gtk_box_pack_start (GTK_BOX (gpp->top), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	return gpp;
}

void
gprop_perm_get_data (GpropPerm *gpp, umode_t *umode, char **owner, char **group)
{
	g_return_if_fail (gpp != NULL);

	if (umode)
		*umode = perm_get_umode (gpp);

	if (owner)
		*owner = g_strdup (gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (gpp->owner)))));

	if (group)
		*group = g_strdup (gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (gpp->group)))));
}

/***** Directory *****/

GpropDir *
gprop_dir_new (char *icon_filename)
{
	GpropDir *gpd;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *entry;

	gpd = g_new (GpropDir, 1);

	gpd->top = gtk_vbox_new (FALSE, 6);

	frame = gtk_frame_new ("Directory icon");
	gtk_box_pack_start (GTK_BOX (gpd->top), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_border_width (GTK_CONTAINER (vbox), 6);
	gtk_container_add(GTK_CONTAINER (frame), vbox);
	gtk_widget_show (vbox);

	gpd->icon_filename = gnome_file_entry_new ("gprop_dir_icon_filename", "Select directory icon");
	entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (gpd->icon_filename));
	gtk_entry_set_text (GTK_ENTRY (entry), icon_filename ? icon_filename : "");
	gtk_box_pack_start (GTK_BOX (vbox), gpd->icon_filename, FALSE, FALSE, 0);
	gtk_widget_show (gpd->icon_filename);

	return gpd;
}

void
gprop_dir_get_data (GpropDir *gpd, char **icon_filename)
{
	GtkWidget *entry;

	g_return_if_fail (gpd != NULL);

	entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (gpd->icon_filename));

	if (icon_filename)
		*icon_filename = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
}

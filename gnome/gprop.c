/* Properties dialog for the Gnome edition of the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 *
 * Known bug: the problem with the multiple entries in the Properties
 * is a problem with the hisotry code.
 */

#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
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

static void
free_stuff (GtkWidget *widget, gpointer data)
{
	if (data)
		g_free (data);
}

/***** Filename *****/

GpropFilename *
gprop_filename_new (char *complete_filename, char *filename)
{
	GpropFilename *gp;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	char *s;

	g_return_val_if_fail (complete_filename != NULL, NULL);
	g_return_val_if_fail (filename != NULL, NULL);

	gp = g_new (GpropFilename, 1);

	gp->top = gtk_vbox_new (FALSE, 6);
	gtk_signal_connect (GTK_OBJECT (gp->top), "destroy",
			    (GtkSignalFunc) free_stuff,
			    gp);

	frame = gtk_frame_new (_("Filename"));
	gtk_box_pack_start (GTK_BOX (gp->top), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_widget_show (vbox);

	s = g_copy_strings (_("Full name: "), complete_filename, NULL);
	gtk_box_pack_start (GTK_BOX (vbox), label_new (s, 0.0, 0.5), FALSE, FALSE, 0);
	g_free (s);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	gtk_box_pack_start (GTK_BOX (hbox), label_new (_("Filename"), 0.0, 0.5), FALSE, FALSE, 0);

	gp->filename = gnome_entry_new (NULL);
	gtk_entry_set_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (gp->filename))), filename);
	gtk_box_pack_start (GTK_BOX (hbox), gp->filename, TRUE, TRUE, 0);
	gtk_widget_show (gp->filename);

	return gp;
}

void
gprop_filename_get_data (GpropFilename *gp, char **filename)
{
	GtkWidget *entry;

	g_return_if_fail (gp != NULL);

	if (filename) {
		entry = gnome_entry_gtk_entry (GNOME_ENTRY (gp->filename));
		*filename = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
	}
}

/* Executable */
GpropExec *
gprop_exec_new (GnomeDesktopEntry *dentry)
{
	GpropExec *ge;
	GtkWidget *frame, *table;
	char *s;
	
	ge = g_new (GpropExec, 1);
	ge->top = gtk_vbox_new (FALSE, 6);

	frame = gtk_frame_new (_("Command"));
	gtk_box_pack_start (GTK_BOX (ge->top), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	
	table = gtk_table_new (0, 0, 0);
	gtk_container_set_border_width (GTK_CONTAINER (table), 6);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);
	gtk_container_add (GTK_CONTAINER (frame), table);

	gtk_table_attach (GTK_TABLE (table), label_new (_("Command:"), 0.0, 0.5),
			  0, 1, 0, 1,
			  GTK_FILL, GTK_FILL, 0, 0);
	
	ge->entry = gnome_entry_new (NULL);
	s = gnome_config_assemble_vector (dentry->exec_length, (const char * const *) dentry->exec);
	gtk_entry_set_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (ge->entry))), s);
	g_free (s);
	gtk_table_attach (GTK_TABLE (table), ge->entry,
			  1, 2, 0, 1, 0, 0, 0, 0);
	ge->check = gtk_check_button_new_with_label (_("Use terminal"));
	GTK_TOGGLE_BUTTON (ge->check)->active = dentry->terminal ? 1 : 0;
	gtk_table_attach (GTK_TABLE (table), ge->check,
			  1, 2, 1, 2, 0, 0, 0, 0);
	gtk_widget_show_all (ge->top);
	return ge;
}

void
gprop_exec_get_data (GpropExec *ge, GnomeDesktopEntry *dentry)
{
	GtkEntry *entry;

	entry = GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (ge->entry)));
	g_strfreev (dentry->exec);
	gnome_config_make_vector (gtk_entry_get_text (entry),
				  &dentry->exec_length, &dentry->exec);
	dentry->terminal = GTK_TOGGLE_BUTTON (ge->check)->active;
}
	      
/***** permissions *****/

static umode_t
perm_get_umode (GpropPerm *gp)
{
	umode_t umode;

#define SETBIT(widget, flag) umode |= GTK_TOGGLE_BUTTON (widget)->active ? flag : 0

	umode = 0;

	SETBIT (gp->suid, S_ISUID);
	SETBIT (gp->sgid, S_ISGID);
	SETBIT (gp->svtx, S_ISVTX);
	
	SETBIT (gp->rusr, S_IRUSR);
	SETBIT (gp->wusr, S_IWUSR);
	SETBIT (gp->xusr, S_IXUSR);
	
	SETBIT (gp->rgrp, S_IRGRP);
	SETBIT (gp->wgrp, S_IWGRP);
	SETBIT (gp->xgrp, S_IXGRP);
	
	SETBIT (gp->roth, S_IROTH);
	SETBIT (gp->woth, S_IWOTH);
	SETBIT (gp->xoth, S_IXOTH);

	return umode;
	
#undef SETBIT
}

static void
perm_set_mode_label (GtkWidget *widget, gpointer data)
{
	umode_t umode;
	GpropPerm *gp;
	char s_mode[5];
	
	gp = data;

	umode = perm_get_umode (gp);

	s_mode[0] = '0' + ((umode & (S_ISUID | S_ISGID | S_ISVTX)) >> 9);
	s_mode[1] = '0' + ((umode & (S_IRUSR | S_IWUSR | S_IXUSR)) >> 6);
	s_mode[2] = '0' + ((umode & (S_IRGRP | S_IWGRP | S_IXGRP)) >> 3);
	s_mode[3] = '0' + ((umode & (S_IROTH | S_IWOTH | S_IXOTH)) >> 0);
	s_mode[4] = 0;

	gtk_label_set_text (GTK_LABEL (gp->mode_label), s_mode);
}

static GtkWidget *
perm_check_new (char *text, int state, GpropPerm *gp)
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
			    gp);

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
	r = perm_check_new (NULL, umode & rmask, gp);			\
	w = perm_check_new (NULL, umode & wmask, gp);			\
	x = perm_check_new (NULL, umode & xmask, gp);			\
									\
	ATTACH (table, label_new (name, 0.0, 0.5), 0, 1, y, y + 1);	\
	ATTACH (table, r, 1, 2, y, y + 1);				\
	ATTACH (table, w, 2, 3, y, y + 1);				\
	ATTACH (table, x, 3, 4, y, y + 1);				\
} while (0);
	
static GtkWidget *
perm_mode_new (GpropPerm *gp, umode_t umode)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *table;

	frame = gtk_frame_new (_("File mode (permissions)"));

	vbox = gtk_vbox_new (FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_widget_show (vbox);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	gtk_box_pack_start (GTK_BOX (hbox), label_new (_("Current mode: "), 0.0, 0.5), FALSE, FALSE, 0);

	gp->mode_label = label_new ("0000", 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), gp->mode_label, FALSE, FALSE, 0);

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

	PERMSET (_("User"),  gp->rusr, gp->wusr, gp->xusr, S_IRUSR, S_IWUSR, S_IXUSR, 1);
	PERMSET (_("Group"), gp->rgrp, gp->wgrp, gp->xgrp, S_IRGRP, S_IWGRP, S_IXGRP, 2);
	PERMSET (_("Other"), gp->roth, gp->woth, gp->xoth, S_IROTH, S_IWOTH, S_IXOTH, 3);

	/* Special */

	gp->suid = perm_check_new (_("Set UID"), umode & S_ISUID, gp);
	gp->sgid = perm_check_new (_("Set GID"), umode & S_ISGID, gp);
	gp->svtx = perm_check_new (_("Sticky"),  umode & S_ISVTX, gp);
	
	ATTACH (table, gp->suid, 4, 5, 1, 2);
	ATTACH (table, gp->sgid, 4, 5, 2, 3);
	ATTACH (table, gp->svtx, 4, 5, 3, 4);

	perm_set_mode_label (NULL, gp);
	
	return frame;
}

#undef ATTACH
#undef PERMSET

static GtkWidget *
perm_owner_new (char *owner)
{
	GtkWidget *gentry;
	GtkWidget *entry;
	GtkWidget *list;
	struct passwd *passwd;
	int i, sel;

	gentry = gnome_entry_new (NULL);

	list = GTK_COMBO (gentry)->list;

	/* We can't use 0 as the intial element because the gnome entry may already
	 * have loaded some history from a file.
	 */

	i = g_list_length (GTK_LIST (list)->children);
	sel = i;

	gnome_entry_append_history (GNOME_ENTRY (gentry), FALSE, _("<Unknown>"));

	for (setpwent (); (passwd = getpwent ()) != NULL; i++) {
		gnome_entry_append_history (GNOME_ENTRY (gentry), FALSE, passwd->pw_name);
		if (strcmp (passwd->pw_name, owner) == 0)
			sel = i;
	}
	endpwent ();
	gtk_list_select_item (GTK_LIST (list), sel);

	entry = gnome_entry_gtk_entry (GNOME_ENTRY (gentry));
	gtk_entry_set_text (GTK_ENTRY (entry), owner);

	return gentry;
}

static GtkWidget *
perm_group_new (char *group)
{
	GtkWidget *gentry;
	GtkWidget *entry;
	GtkWidget *list;
	struct group *grp;
	int i, sel;

	gentry = gnome_entry_new ("gprop_perm_group");
	gnome_entry_append_history (GNOME_ENTRY (gentry), FALSE, _("<Unknown>"));

	list = GTK_COMBO (gentry)->list;

	/* We can't use 0 as the intial element because the gnome entry may already
	 * have loaded some history from a file.
	 */

	i = g_list_length (GTK_LIST (list)->children);
	sel = i;

	for (setgrent (); (grp = getgrent ()) != NULL; i++) {
		gnome_entry_append_history (GNOME_ENTRY (gentry), FALSE, grp->gr_name);
		if (strcmp (grp->gr_name, group) == 0)
			sel = i;
	}
	endgrent ();
	
	gtk_list_select_item (GTK_LIST (list), sel);

	entry = gnome_entry_gtk_entry (GNOME_ENTRY (gentry));
	gtk_entry_set_text (GTK_ENTRY (entry), group);

	return gentry;
}

static GtkWidget *
perm_ownership_new (GpropPerm *gp, char *owner, char *group)
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

	gp->owner = perm_owner_new (owner);
	gtk_table_attach (GTK_TABLE (table), gp->owner,
			  1, 2, 0, 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_widget_show (gp->owner);

	/* Group */

	gtk_table_attach (GTK_TABLE (table), label_new (_("Group"), 0.0, 0.5),
			  0, 1, 1, 2,
			  GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK,
			  0, 0);

	gp->group = perm_group_new (group);
	gtk_table_attach (GTK_TABLE (table), gp->group,
			  1, 2, 1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_widget_show (gp->group);

	return frame;
}

GpropPerm *
gprop_perm_new (umode_t umode, char *owner, char *group)
{
	GpropPerm *gp;
	GtkWidget *w;

	g_return_val_if_fail (owner != NULL, NULL);
	g_return_val_if_fail (group != NULL, NULL);

	gp = g_new (GpropPerm, 1);

	gp->top = gtk_vbox_new (FALSE, 6);
	gtk_signal_connect (GTK_OBJECT (gp->top), "destroy",
			    (GtkSignalFunc) free_stuff,
			    gp);

	w = perm_mode_new (gp, umode);
	gtk_box_pack_start (GTK_BOX (gp->top), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	w = perm_ownership_new (gp, owner, group);
	gtk_box_pack_start (GTK_BOX (gp->top), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	return gp;
}

void
gprop_perm_get_data (GpropPerm *gp, umode_t *umode, char **owner, char **group)
{
	g_return_if_fail (gp != NULL);

	if (umode)
		*umode = perm_get_umode (gp);

	if (owner)
		*owner = g_strdup (gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (gp->owner)))));

	if (group)
		*group = g_strdup (gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (gp->group)))));
}

/***** General *****/

static void
change_icon (GtkEntry *entry, GpropGeneral *gp)
{
	char *filename;

	filename = gtk_entry_get_text (entry);

	if (g_file_exists (filename) && gp->icon_pixmap){
		gnome_pixmap_load_file (GNOME_PIXMAP (gp->icon_pixmap), gtk_entry_get_text (entry));
	}
}

GpropGeneral *
gprop_general_new (char *title, char *icon_filename)
{
	GpropGeneral *gp;
	GtkWidget *frame;
	GtkWidget *table;
	GtkWidget *entry;

	g_return_val_if_fail (title != NULL, NULL);
	g_return_val_if_fail (icon_filename != NULL, NULL);

	gp = g_new (GpropGeneral, 1);

	gp->top = gtk_vbox_new (FALSE, 6);
	gtk_signal_connect (GTK_OBJECT (gp->top), "destroy",
			    (GtkSignalFunc) free_stuff,
			    gp);

	frame = gtk_frame_new (_("General"));
	gtk_box_pack_start (GTK_BOX (gp->top), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);

	table = gtk_table_new (3, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 6);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 4);
	gtk_container_add (GTK_CONTAINER (frame), table);
	gtk_widget_show (table);

	gtk_table_attach (GTK_TABLE (table), label_new (_("Title"), 0.0, 0.5),
			  0, 1, 0, 1,
			  GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);

	gp->title = gnome_entry_new ("gprop_general_title");
	entry = gnome_entry_gtk_entry (GNOME_ENTRY (gp->title));
	gtk_entry_set_text (GTK_ENTRY (entry), title);
	gtk_table_attach (GTK_TABLE (table), gp->title,
			  1, 2, 0, 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_widget_show (gp->title);

	gtk_table_attach (GTK_TABLE (table), label_new (_("Icon"), 0.0, 0.5),
			  0, 1, 1, 2,
			  GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);

	gp->icon_pixmap = gnome_pixmap_new_from_file (icon_filename);
	gtk_table_attach (GTK_TABLE (table), gp->icon_pixmap,
			  1, 2, 2, 3,
			  GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_widget_show (gp->icon_pixmap);

	gp->icon_filename = gnome_file_entry_new ("gprop_general_icon_filename", _("Select icon"));
	entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (gp->icon_filename));
	gtk_signal_connect (GTK_OBJECT (entry), "changed",
			    (GtkSignalFunc) change_icon,
			    gp);
	gtk_entry_set_text (GTK_ENTRY (entry), icon_filename);
	gtk_table_attach (GTK_TABLE (table), gp->icon_filename,
			  1, 2, 1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_widget_show (gp->icon_filename);

	return gp;
}

void
gprop_general_get_data (GpropGeneral *gp, char **title, char **icon_filename)
{
	GtkWidget *entry;

	g_return_if_fail (gp != NULL);

	if (title) {
		entry = gnome_entry_gtk_entry (GNOME_ENTRY (gp->title));
		*title = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
	}

	if (icon_filename) {
		entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (gp->icon_filename));
		*icon_filename = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
	}
}

/* GNU Midnight Commander -- GNOME edition
 *
 * Properties dialog for files and desktop icons.
 *
 * Copyright (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 */

#include <config.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include "x.h"
#include "global.h"
#include "gprop.h"
#include "dialog.h"
#include "file.h"
#include "fileopctx.h"
#include "../vfs/vfs.h"
#include "gdesktop.h"
#include "gdesktop-icon.h"
#include "gpageprop.h"
#include "gicon.h"

static int prop_dialog_result;

static GtkWidget *
c_spacing (GtkWidget *widget)
{
	gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
	return widget;
}

static void
properties_button_click (GtkWidget *widget, gpointer value)
{
	prop_dialog_result = (int) value;
	gtk_main_quit ();
}

static void
kill_toplevel ()
{
	gtk_main_quit ();
}

int
item_properties (GtkWidget *parent, char *fname, DesktopIconInfo *dii)
{
	GtkWidget     *parent_window;
	GdkCursor     *cursor;
	GtkWidget     *notebook, *ok, *cancel;
	GtkWidget     *vbox;
	GpropFilename *name;
	GpropPerm     *perm;
	GpropGeneral  *gene;
	GtkDialog     *toplevel;

	umode_t        new_mode;
	char          *new_group;
	char          *new_owner;
	char          *new_title;
	char          *new_name;
	char          *base;
	char          *icon_filename;

	struct stat s;
	int retval = 0;

	/* Set a clock cursor while we create stuff and read users/groups */

	parent_window = gtk_widget_get_toplevel (parent);
	cursor = gdk_cursor_new (GDK_WATCH);
	gdk_window_set_cursor (parent_window->window, cursor);
	gdk_cursor_destroy (cursor);

	toplevel = GTK_DIALOG (gtk_dialog_new ());
	notebook = gtk_notebook_new ();
	gtk_box_pack_start_defaults (GTK_BOX (toplevel->vbox), notebook);
	gtk_window_set_title (GTK_WINDOW (toplevel), fname);

	/* Create the property widgets */
	mc_stat (fname, &s);

	vbox = gtk_vbox_new (FALSE, 6);

	name = gprop_filename_new (fname, base);
	gtk_box_pack_start (GTK_BOX (vbox), name->top, FALSE, FALSE, 0);
	if (dii) {
		GdkImlibImage *icon;
		file_entry *fe;
		char *dirname;
		char *p;
		const char *ifile;

		p = strrchr (fname, PATH_SEP);
		g_assert (p != NULL);
		dirname = g_strndup (fname, p - fname);

		fe = file_entry_from_file (fname);
		icon = gicon_get_icon_for_file (dirname, fe, FALSE);
		g_free (dirname);
		file_entry_free (fe);
		ifile = gicon_get_filename_for_icon (icon);
		if (ifile == NULL)
			icon_filename = g_strdup (ICONDIR "i-regular.png");
		else
			icon_filename = g_strdup (ifile);
		
		gene = gprop_general_new (fname, icon_filename);
					 
		gtk_box_pack_start (GTK_BOX (vbox), gene->top, FALSE, FALSE, 0);
	}

	perm = gprop_perm_new (s.st_mode, get_owner (s.st_uid), get_group (s.st_gid));

	/* Pack them into nice notebook */
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), c_spacing (vbox), gtk_label_new ("General"));
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), c_spacing (perm->top), gtk_label_new ("Permissions"));

	/* Ok, Cancel */
	ok = gnome_stock_button (GNOME_STOCK_BUTTON_OK);
	GTK_WIDGET_SET_FLAGS (ok, GTK_CAN_DEFAULT);

	cancel = gnome_stock_button (GNOME_STOCK_BUTTON_CANCEL);
	GTK_WIDGET_SET_FLAGS (cancel, GTK_CAN_DEFAULT);

	gtk_box_pack_start (GTK_BOX (toplevel->action_area), ok, 0, 0, 0);
	gtk_box_pack_start (GTK_BOX (toplevel->action_area), cancel, 0, 0, 0);

	gtk_signal_connect (GTK_OBJECT (ok), "clicked", GTK_SIGNAL_FUNC (properties_button_click), (gpointer) 0);
	gtk_signal_connect (GTK_OBJECT (cancel), "clicked", GTK_SIGNAL_FUNC (properties_button_click), (gpointer) 1);
	gtk_signal_connect (GTK_OBJECT (toplevel), "delete_event", GTK_SIGNAL_FUNC (kill_toplevel), toplevel);

	/* Start the dialog box */
	prop_dialog_result = -1;

	gtk_widget_grab_default (ok);
	gtk_widget_show_all (GTK_WIDGET (toplevel));

	/* Restore the arrow cursor and run the dialog */

	cursor = gdk_cursor_new (GDK_TOP_LEFT_ARROW);
	gdk_window_set_cursor (parent_window->window, cursor);
	gdk_cursor_destroy (cursor);

	gtk_grab_add (GTK_WIDGET (toplevel));
	gtk_main ();
	gtk_grab_remove (GTK_WIDGET (toplevel));

	if (prop_dialog_result != 0) {
		gtk_widget_destroy (GTK_WIDGET (toplevel));
		g_free (icon_filename);
		return 0; /* nothing changed */
	}

	/* Check and change permissions */

	gprop_perm_get_data (perm, &new_mode, &new_owner, &new_group);

	if (new_mode != s.st_mode) {
		mc_chmod (fname, new_mode);
		retval |= GPROP_MODE;
	}

	if ((strcmp (new_owner, get_owner (s.st_uid)) != 0) ||
	    (strcmp (new_group, get_group (s.st_gid)) != 0)) {
		struct passwd *p;
		struct group  *g;
		uid_t uid;
		gid_t gid;

		/* Get uid */
		p = getpwnam (new_owner);
		if (!p) {
			uid = atoi (new_owner);
			if (uid == 0) {
				int v;
				v = query_dialog ("Warning",
						  "You entered an invalid user name, should I use `root'?",
						  0, 2, "Yes", "No");
				if (v != 0)
					goto ciao;
			}
		} else
			uid = p->pw_uid;

		/* get gid */
		g = getgrnam (new_group);
		if (!g) {
			gid = atoi (new_group);
			if (gid == 0) {
				message (1, "Error", "You entered an invalid group name");
				goto ciao2;
			}
		} else
			gid = g->gr_gid;

		mc_chown (fname, uid, gid);
		retval |= GPROP_UID | GPROP_GID;

		ciao2:
		endgrent ();
		ciao:
		endpwent ();
	}

	/* Check and change filename */

	gprop_filename_get_data (name, &new_name);

	if (strchr (new_name, '/'))
	  message (1, "Error", "The new name includes the `/' character");
	else if (strcmp (new_name, base) != 0) {
		char  *base = x_basename (fname);
		char   save = *base;
		char  *full_target;
		long   count = 0;
		double bytes = 0;
		FileOpContext *ctx;

		*base = 0;
		full_target = concat_dir_and_file (fname, new_name);
		*base = save;

		ctx = file_op_context_new ();
		file_op_context_create_ui (ctx, OP_MOVE, FALSE);
		move_file_file (ctx, fname, full_target, &count, &bytes);
		file_op_context_destroy (ctx);
			
		if (dii) {
			 g_free (dii->filename);
			dii->filename = full_target;
		} else
			 g_free (full_target);
			
		retval |= GPROP_FILENAME;
	}

	/* Check and change title and icon -- change is handled by caller */

	if (dii) {
		char *new_icon;

		gprop_general_get_data (gene, &new_title, &new_icon);

		if (strcmp (new_icon, icon_filename) != 0)
			gnome_metadata_set (fname, "icon-filename", strlen (new_icon)+1, new_icon);

		desktop_icon_set_text(DESKTOP_ICON(dii->dicon), new_title);
		g_free(new_title);

		retval |= GPROP_TITLE;
	}

	gtk_widget_destroy (GTK_WIDGET (toplevel));
	g_free (icon_filename);
	return retval;
}

#include <config.h>
#include "x.h"
#include <stdio.h>
#include <sys/stat.h>
#include "dir.h"
#include "panel.h"
#include "gscreen.h"
#include "main.h"
#include "gmain.h"
#include "cmd.h"
#include "boxes.h"
#include "profile.h"
#include "setup.h"
#include "panelize.h"
#include "dialog.h"
#include "layout.h"
#include "../vfs/vfs.h"
#include "gprefs.h"

/* Orphan confirmation options */
/* Auto save setup */
/* use internal edit */ /* maybe have a mime-type capplet for these */
/* use internal view */
/* complete: show all */
/* Animation??? */
/* advanced chown */
/* cd follows links */
/* safe delete */
static GtkWidget *
file_display_pane (WPanel *panel)
{
	/* This should configure the file options. */
	/* Show Backup files */
	/* show Hidden files */
	/* mix files and directories */
	/* shell patterns */
	GtkWidget *vbox;
	
	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	return vbox;
}
static GtkWidget *
confirmation_pane (WPanel *panel)
{
	/* Confirm... */
	/*  delete file*/
	/*  overwrite */
	/*  execute */
	/* Verbose Operations */
	return gtk_frame_new (NULL);
}
static GtkWidget *
custom_view_pane (WPanel *panel)
{ 
	return gtk_frame_new (NULL);
}
static GtkWidget *
vfs_pane (WPanel *panel)
{
	/* VFS timeout */
	/* Anon ftp password */
	/* Always use ftp proxy: */
	return gtk_frame_new (NULL);
}
static GtkWidget *
caching_and_optimizations_pane (WPanel *panel)
{
	/* Fast dir reload */
	/* Compute totals */
	/* ftpfs directory cache timeout */
	return gtk_frame_new (NULL);
}
void
gnome_configure_box (GtkWidget *widget, WPanel *panel)
{
	GtkWidget *prefs_dlg;

	prefs_dlg = gnome_property_box_new ();
	/* do we want a generic page? */
	gnome_property_box_append_page (GNOME_PROPERTY_BOX (prefs_dlg),
					file_display_pane (panel),
					gtk_label_new (_("File Display")));
	gnome_property_box_append_page (GNOME_PROPERTY_BOX (prefs_dlg),
					confirmation_pane (panel),
					gtk_label_new (_("Confirmation")));
	gnome_property_box_append_page (GNOME_PROPERTY_BOX (prefs_dlg),
					custom_view_pane (panel),
					gtk_label_new (_("Custom View")));
	gnome_property_box_append_page (GNOME_PROPERTY_BOX (prefs_dlg),
					vfs_pane (panel),
					gtk_label_new (_("Caching and Optimizations")));
	gnome_property_box_append_page (GNOME_PROPERTY_BOX (prefs_dlg),
					caching_and_optimizations_pane (panel),
					gtk_label_new (_("VFS")));
	gtk_widget_show_all (GNOME_PROPERTY_BOX (prefs_dlg)->notebook);
	gnome_dialog_run_and_close (GNOME_DIALOG (prefs_dlg));
}


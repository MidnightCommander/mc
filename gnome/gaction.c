#include <config.h>
#include <string.h>
#include "x.h"
#include "global.h"
#include "dir.h"
#include "command.h"
#include "panel.h"
#define WANT_WIDGETS		/* bleah */
#include "main.h"
#include "color.h"
#include "mouse.h"
#include "layout.h"		/* get_panel_widget */
#include "ext.h"		/* regex_command */
#include "cmd.h"		/* copy_cmd, ren_cmd, delete_cmd, ... */
#include "gscreen.h"
#include "gmain.h"
#include "dir.h"
#include "dialog.h"
#include "gcmd.h"
#include "../vfs/vfs.h"
#include "gnome-open-dialog.h"

static void
gmc_execute (const char *fname, const char *buf, int needs_terminal)
{
	exec_extension (fname, buf, NULL, NULL, 0, needs_terminal);
}

int
gmc_open_filename (char *fname, GList *args)
{
	const char *mime_type;
	const char *cmd;
	char *buf;
	int size;
	int needs_terminal = 0;

	mime_type = gnome_mime_type_or_default (fname, NULL);

	/*
	 * We accept needs_terminal as -1, which means our caller
	 * did not want to do the work
	 */
	if (gnome_metadata_get (fname, "flags", &size, &buf) == 0){
		needs_terminal = (strstr (buf, "needsterminal") != 0);
		g_free (buf);
	} else if (mime_type){
		const char *flags;
		
		flags = gnome_mime_type_or_default (mime_type, "flags");
		needs_terminal = (strstr (flags, "needsterminal") != 0);
	}
	
	if (gnome_metadata_get (fname, "fm-open", &size, &buf) == 0){
		gmc_execute (fname, buf, needs_terminal);
		g_free (buf);
		return 1;
	}

	if (gnome_metadata_get (fname, "open", &size, &buf) == 0){
		gmc_execute (fname, buf, needs_terminal);
		g_free (buf);
		return 1;
	}

	if (!mime_type)
		return 0;
	
		
	cmd = gnome_mime_get_value (mime_type, "fm-open");
	
	if (cmd){
		gmc_execute (fname, cmd, needs_terminal);
		return 1;
	}
	
	cmd = gnome_mime_get_value (mime_type, "open");
	if (cmd){
		gmc_execute (fname, cmd, needs_terminal);
		return 1;
	}

	if (strcmp (mime_type, "application/x-gnome-app-info")  == 0){
		GnomeDesktopEntry *entry;

		entry = gnome_desktop_entry_load (fname);
		if (entry){
			gnome_desktop_entry_launch (entry);
			gnome_desktop_entry_free (entry);
		}
	}

	return 0;
}

int
gmc_edit (char *fname)
{
	const char *mime_type;
	const char *cmd;
	char *buf;
	int size;
	char *editor, *type;
	int on_terminal;

	if (gnome_metadata_get (fname, "edit", &size, &buf) == 0){
		gmc_execute (fname, buf, 0);
		g_free (buf);
		return 1;
	}

	mime_type = gnome_mime_type_or_default (fname, NULL);
	if (mime_type){
		cmd = gnome_mime_get_value (mime_type, "edit");
	
		if (cmd){
			gmc_execute (fname, cmd, 0);
			return 1;
		}
	}

	gnome_config_push_prefix( "/editor/Editor/");
	type = gnome_config_get_string ("EDITOR_TYPE=executable");
	
	if (strcmp (type, "mc-internal") == 0){
		g_free (type);
		do_edit (fname);
		return 1;
	}
	g_free (type);

	editor = gnome_config_get_string ("EDITOR=emacs");
	on_terminal = gnome_config_get_bool ("NEEDS_TERM=false");

	if (on_terminal){
		char *quoted = name_quote (fname, 0);
		char *editor_cmd = g_strconcat (editor, " ", quoted, NULL);
		
		gnome_open_terminal_with_cmd (editor_cmd);
		g_free (quoted);
		g_free (editor_cmd);
	} else {
		char *cmd = g_strconcat (editor, " %s", NULL);
		
		gmc_execute (fname, cmd, 0);
		g_free (cmd);
	}

	g_free (editor);
	return 0;
}

int
gmc_open (file_entry *fe)
{
	return gmc_open_filename (fe->fname, NULL);
}

int
gmc_open_with (gchar *filename)
{
	GtkWidget *dialog;
	gchar *command = NULL;
	gchar *real_command;
	WPanel *panel = cpanel;

	dialog = gnome_open_dialog_new (filename);
	if (!is_a_desktop_panel (panel))
		gnome_dialog_set_parent (GNOME_DIALOG (dialog), GTK_WINDOW (panel->xwindow));

	switch (gnome_dialog_run (GNOME_DIALOG (dialog))) {
	case 0:
	        command = gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (
			GNOME_FILE_ENTRY (GNOME_OPEN_DIALOG (dialog)->entry)->gentry))));
		/* Assume COMMAND %f */
		if (command) {
			/* FIXME: these need the needs_terminal argument to be set correctly */
			if (strchr (command, '%'))
				exec_extension (filename, command, NULL, NULL, 0, FALSE);
			else {
				/* This prolly isn't perfect, but it will do. */
				real_command = g_strconcat (command, " %f", NULL);
				exec_extension (filename, real_command, NULL, NULL, 0, FALSE);
				g_free (real_command);
			}
		}
		if (command) {
			gtk_widget_destroy (dialog);
			return 1;
		}
		gtk_widget_destroy (dialog);
		return 0;
	case 1:
		gtk_widget_destroy (dialog);
	default:
		return 0;
	}
}
static void
gmc_run_view (const char *filename, const char *buf)
{
	exec_extension (filename, buf, NULL, NULL, 0, 0);
}
static gchar *
gmc_view_command (gchar *filename)
{
	const char *mime_type, *cmd;
	char *buf;
	int size;

	if (gnome_metadata_get (filename, "fm-view", &size, &buf) == 0)
		return buf;

	if (gnome_metadata_get (filename, "view", &size, &buf) == 0)
		return buf;

	mime_type = gnome_mime_type_or_default (filename, NULL);
	if (!mime_type)
		return NULL;
	
		
	cmd = gnome_mime_get_value (mime_type, "fm-view");
	
	if (cmd)
		return g_strdup (cmd);
	
	cmd = gnome_mime_get_value (mime_type, "view");
	if (cmd){
		return g_strdup (cmd);
	}
	return NULL;
}
	
int
gmc_view (char *filename, int start_line)
{
	char *cmd;

	cmd = gmc_view_command (filename);
	if (cmd) {
		gmc_run_view (filename, cmd);
		g_free (cmd);
		return 1;
	} else
		view (NULL, filename, 0, 0);
	return 0;
}

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
#include "view.h"
#include "gcmd.h"
#include "../vfs/vfs.h"
#include "gnome-open-dialog.h"

static void
gmc_unable_to_execute_dlg (gchar *fname, const gchar *command, gchar *action, const gchar *mime)
{
	GtkWidget *msg_dialog = NULL;
	GtkWidget *hack_widget;
	gchar *msg;
	gchar *fix = NULL;
	if (!strcmp (action, "x-gnome-app-info")) {
		msg = g_strdup_printf (
			_("Unable to execute\n\"%s\".\n\n"
			"Please check it to see if it points to a valid command."),
			fname);
		
	} else {
		if (mime)
			fix = g_strdup_printf (
				_("\".\n\nTo fix this, bring up the mime-properties editor "
				"in the GNOME Control Center, and edit the default %s"
				"-action for \"%s\"."),
				action, mime);
		else 
			fix = g_strdup_printf (
				_("\".\n\nTo fix this error, bring up this file's properties "
				"and change the default %s-action."),
				action);

		msg = g_strdup_printf (
			_("Unable to %s\n\"%s\"\nwith the command:\n\"%s\"%s"),
			action, fname, command, fix);
			
		g_free (fix);
	}
	msg_dialog = gnome_message_box_new (msg,
					    GNOME_MESSAGE_BOX_ERROR,
					    GNOME_STOCK_BUTTON_OK,
					    NULL);
	/* Kids, don't try this at home */
	/* this is pretty evil... <-:  */
	hack_widget = GNOME_DIALOG (msg_dialog)->vbox;
	hack_widget = ((GtkBoxChild *) GTK_BOX (hack_widget)->children->data)->widget;
	hack_widget = ((GtkBoxChild *) GTK_BOX (hack_widget)->children->next->data)->widget;
	gtk_label_set_line_wrap (GTK_LABEL (hack_widget), TRUE);

	gnome_dialog_run_and_close (GNOME_DIALOG (msg_dialog));
	g_free (msg);
}

static void
gmc_execute (const char *fname, const char *buf, int needs_terminal)
{
	exec_extension (fname, buf, NULL, NULL, 0, needs_terminal);
}

static gboolean
gmc_check_exec_string (const char *buf)
{
	gint argc;
	gint retval, i;
	gchar *prog;
	gchar **argv;
	
	gnome_config_make_vector (buf, &argc, &argv);
	if (argc < 1)
		return FALSE;

	prog = gnome_is_program_in_path (argv[0]);
	if (prog)
		retval = TRUE;
	else
		retval = FALSE;
	g_free (prog);
	/* check to see if it's a GMC directive. */
	if ((strlen (argv[0]) > 1) && (argv[0][0] == '%') && (argv[0][1] != '%'))
		retval = TRUE;
	for (i = 0; i < argc; i++)
		g_free (argv[i]);
	g_free (argv);
	return retval;
}
int
gmc_open_filename (char *fname, GList *args)
{
	const char *mime_type;
	const char *cmd;
	char *buf = NULL;
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
	
	if (gnome_metadata_get (fname, "fm-open", &size, &buf) != 0) {
		gnome_metadata_get (fname, "open", &size, &buf);
	}
	if (buf) {
		if (gmc_check_exec_string (buf))
			gmc_execute (fname, buf, needs_terminal);
		else
			gmc_unable_to_execute_dlg (fname, buf, _("open"), NULL);
		g_free (buf);
		return 1;
	}
	if (!mime_type)
		return 0;
	
	cmd = gnome_mime_get_value (mime_type, "fm-open");
	if (cmd == NULL) {
		cmd = gnome_mime_get_value (mime_type, "open");
	}
	
	if (cmd){
		if (gmc_check_exec_string (cmd))
			gmc_execute (fname, cmd, needs_terminal);
		else 
			gmc_unable_to_execute_dlg (fname, cmd, _("open"), mime_type);
		return 1;
	}

	if (strcmp (mime_type, "application/x-gnome-app-info")  == 0){
		GnomeDesktopEntry *entry;

		entry = gnome_desktop_entry_load (fname);
		if (entry){
			gnome_desktop_entry_launch (entry);
			gnome_desktop_entry_free (entry);
			return 1;
		} else {
			gmc_unable_to_execute_dlg (fname, NULL, "x-gnome-app-info", NULL);
			return 1;
		}
	}
	/* We just struck out.  Prompt the user. */
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
		if (gmc_check_exec_string (buf))
			gmc_execute (fname, buf, 0);
		else
			gmc_unable_to_execute_dlg (fname, buf, _("edit"), NULL);
		g_free (buf);
		return 1;
	}

	mime_type = gnome_mime_type_or_default (fname, NULL);
	if (mime_type){
		cmd = gnome_mime_get_value (mime_type, "edit");
	
		if (cmd){
			if (gmc_check_exec_string (cmd))
				gmc_execute (fname, cmd, 0);
			else
				gmc_unable_to_execute_dlg (fname, cmd, _("edit"), mime_type);
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
	gint retval;
	gchar *fullname;
	fullname = g_concat_dir_and_file (cpanel->cwd, fe->fname);
	retval = gmc_open_filename (fullname, NULL);
	g_free (fullname);
	return retval;
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
	gchar *cmd;
	const gchar *mime_type;
	mime_type = gnome_mime_type_or_default (filename, NULL);
	cmd = gmc_view_command (filename);
	if (cmd) {
		if (gmc_check_exec_string (cmd))
			gmc_run_view (filename, cmd);
		else
			gmc_unable_to_execute_dlg (filename, cmd, _("view"), mime_type);
		g_free (cmd);
		return 1;
	} else
		view (NULL, filename, 0, 0);
	return 0;
}

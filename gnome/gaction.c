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
#include "dir.h"
#include "dialog.h"
#include "../vfs/vfs.h"

static void
gmc_execute (const char *fname, const char *buf)
{
	exec_extension (fname, buf, NULL, NULL, 0);
}

int
gmc_open_filename (char *fname, GList *args)
{
	const char *mime_type;
	const char *cmd;
	char *buf;
	int size;

	if (gnome_metadata_get (fname, "fm-open", &size, &buf) == 0){
		gmc_execute (fname, buf);
		g_free (buf);
		return 1;
	}

	if (gnome_metadata_get (fname, "open", &size, &buf) == 0){
		gmc_execute (fname, buf);
		g_free (buf);
		return 1;
	}

	mime_type = gnome_mime_type_or_default (fname, NULL);
	if (!mime_type)
		return 0;
	
		
	cmd = gnome_mime_get_value (mime_type, "fm-open");
	
	if (cmd){
		gmc_execute (fname, cmd);
		return 1;
	}
	
	cmd = gnome_mime_get_value (mime_type, "open");
	if (cmd){
		gmc_execute (fname, cmd);
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
gmc_edit_filename (char *fname)
{
	const char *mime_type;
	const char *cmd;
	char *buf;
	int size;
	char *editor, *type;
	int on_terminal;

	if (gnome_metadata_get (fname, "edit", &size, &buf) == 0){
		gmc_execute (fname, buf);
		g_free (buf);
		return 1;
	}

	mime_type = gnome_mime_type_or_default (fname, NULL);
	if (mime_type){
		cmd = gnome_mime_get_value (mime_type, "edit");
	
		if (cmd){
			gmc_execute (fname, cmd);
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
		
		gmc_execute (fname, cmd);
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

static void
gmc_run_view (const char *filename, const char *buf)
{
	exec_extension (filename, buf, NULL, NULL, 0);
}

int
gmc_view (char *filename, int start_line)
{
	const char *mime_type, *cmd;
	char *buf;
	int size;

	if (gnome_metadata_get (filename, "fm-view", &size, &buf) == 0){
		gmc_run_view (filename, buf);
		g_free (buf);
		return 1;
	}

	if (gnome_metadata_get (filename, "view", &size, &buf) == 0){
		gmc_run_view (filename, buf);
		g_free (buf);
		return 1;
	}

	mime_type = gnome_mime_type_or_default (filename, NULL);
	if (!mime_type)
		return 0;
	
		
	cmd = gnome_mime_get_value (mime_type, "fm-view");
	
	if (cmd){
		gmc_run_view (filename, cmd);
		return 1;
	}
	
	cmd = gnome_mime_get_value (mime_type, "view");
	if (cmd){
		gmc_run_view (filename, cmd);
		return 1;
	}
	return 0;
}

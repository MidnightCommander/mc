#include <config.h>
#include <string.h>
#include <stdlib.h>		/* atoi */
#include "fs.h"
#include "mad.h"
#include "x.h"
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
gmc_execute (file_entry *fe, char *buf)
{
	exec_extension (fe->fname, buf, NULL, NULL, 0);
}

int
gmc_open (file_entry *fe)
{
	char *mime_type, *cmd;
	char *buf;
	int size;

	if (gnome_metadata_get (fe->fname, "fm-open", &size, &buf) == 0){
		gmc_execute (fe, buf);
		free (buf);
		return 1;
	}

	if (gnome_metadata_get (fe->fname, "open", &size, &buf) == 0){
		gmc_execute (fe, buf);
		free (buf);
		return 1;
	}

	mime_type = gnome_mime_type_or_default (fe->fname, NULL);
	if (!mime_type)
		return 0;
	
		
	cmd = gnome_mime_get_value (mime_type, "fm-open");
	
	if (cmd){
		gmc_execute (fe, cmd);
		return 1;
	}
	
	cmd = gnome_mime_get_value (mime_type, "open");
	if (cmd){
		gmc_execute (fe, cmd);
		return 1;
	}
	return 0;
}

static void
gmc_run_view (char *filename, char *buf)
{
	exec_extension (filename, buf, NULL, NULL, 0);
}

void
gmc_view (char *filename, int start_line)
{
	char *mime_type, *cmd;
	char *buf;
	int size;

	if (gnome_metadata_get (filename, "fm-view", &size, &buf) == 0){
		gmc_run_view (filename, buf);
		free (buf);
		return 1;
	}

	if (gnome_metadata_get (filename, "view", &size, &buf) == 0){
		gmc_run_view (filename, buf);
		free (buf);
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

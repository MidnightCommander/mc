/* Printing support for the Midnight Commander
 *
 * Copyright (C) 1998-1999 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#include <config.h>
#include "main.h"
#include "util.h"
#include <libgnome/libgnome.h>
#include "gdesktop.h"
#include "gprint.h"

void
gprint_setup_devices (void)
{
	static char *gprint_path;
	char *desktop_quoted;
	char *command;

	if (!gprint_path)
		gprint_path = gnome_is_program_in_path ("g-print");

	if (!gprint_path)
		return;

	desktop_quoted = name_quote (desktop_directory, 0);
	command = g_strconcat (gprint_path, " --desktop-dir ", desktop_quoted, NULL);
	g_free (desktop_quoted);

	my_system (EXECUTE_WAIT | EXECUTE_AS_SHELL, shell, command);
	g_free (command);
}

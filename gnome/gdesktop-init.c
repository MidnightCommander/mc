/* 
 *
 * Copyright (C) 1998-1999 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#include <config.h>
#include <unistd.h>
#include "main.h"
#include "util.h"
#include <libgnome/libgnome.h>
#include "gdesktop.h"
#include "gdesktop-init.h"
#include "gprint.h"
#include "gmount.h"

static void
desktop_load_init_from (const char *file)
{
	void *iterator_handle;
	char *config_path = g_strconcat ("=", file, "=", NULL);
	
	iterator_handle = gnome_config_init_iterator_sections (config_path);

	do {
		char *key;
		char *file_and_section;
		char *title, *type;

		/* Get next section name */
		iterator_handle = gnome_config_iterator_next (
			iterator_handle, &key, NULL);

		/* Now access the values in the section */
		file_and_section = g_strconcat (config_path, "/", key, "/", NULL);
		gnome_config_push_prefix (file_and_section);
		title = gnome_config_get_translated_string ("title=None");
		type = gnome_config_get_string ("type=url");

		/*
		 * handle the different link types
		 */
		if (strcmp (type, "url") == 0){
			int  used;
			char *icon = NULL, *url;
			
			url = gnome_config_get_string ("url");
			icon = gnome_config_get_string_with_default ("icon=", &used);
			if (!icon)
				icon = g_concat_dir_and_file (ICONDIR, "gnome-http-url.png");
			
			if (url && *url){
				char *filename = g_concat_dir_and_file (desktop_directory, key);
													
				desktop_create_url (filename, title, url, icon);

				g_free (filename);
			}

			if (url)
				g_free (url);
			g_free (icon);
		}
		g_free (title);
		g_free (file_and_section);
		gnome_config_pop_prefix ();
	} while (iterator_handle);

	g_free (config_path);
}

static void
desktop_init_at (const char *dir)
{
	DIR *d;
	struct dirent *dent;
	const int links_extlen = sizeof (".links")-1;
	struct stat s;
	
	d = opendir (dir);
	if (!d)
		return;

	while ((dent = readdir (d)) != NULL){
		int len = strlen (dent->d_name);
		char *fname;

		fname = g_concat_dir_and_file (dir, dent->d_name);

		/*
		 * If it is an executable
		 */
		if (access (fname, X_OK) == 0){
			char *desktop_quoted;
			char *command;
			
			desktop_quoted = name_quote (desktop_directory, 0);
			command = g_strconcat (fname, " --desktop-dir", desktop_quoted, NULL);
			g_free (desktop_quoted);

			my_system (EXECUTE_WAIT | EXECUTE_AS_SHELL, shell, command);
			g_free (command);
			g_free (fname);
			continue;
		}

		if (len < links_extlen){
			g_free (fname);
			continue;
		}
		
		if (strcmp (dent->d_name + len - links_extlen, ".links")){
			g_free (fname);
			continue;
		}
		
		desktop_load_init_from (fname);
	}
}

void
gdesktop_init (void)
{
	char *dir;

	desktop_init_at (DESKTOP_INIT_DIR);
	
	dir = gnome_util_home_file ("desktop-init");
	desktop_init_at (dir);
	g_free (dir);

	gmount_setup_devices ();
	gprint_setup_devices ();
}


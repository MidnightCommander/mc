/* Customizable desktop links for the Midnight Commander
 *
 * Copyright (C) 1998-1999 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#include <config.h>
#include <unistd.h>
#include "x.h"
#include "main.h"
#include "util.h"
#include "gdesktop.h"
#include "gdesktop-init.h"
#include "gprint.h"
#include "gmount.h"
#include "../vfs/vfs.h"

static void
desktop_load_init_from (const char *file)
{
	char *key;
	char *file_and_section;
	char *title, *type;

	void *iterator_handle;
	char *config_path = g_strconcat ("=", file, "=", NULL);

	iterator_handle = gnome_config_init_iterator_sections (config_path);

	iterator_handle = gnome_config_iterator_next (
		iterator_handle, &key, NULL);

	do {
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
			char *icon, *url;
			char *icon2 = NULL;

			url = gnome_config_get_string ("url");
			icon = gnome_config_get_string_with_default ("icon=", &used);
			if (icon){
				icon2 = gnome_pixmap_file (icon);
				g_free (icon);
			}
			if (!icon2)
				icon2 = g_concat_dir_and_file (ICONDIR, "gnome-http-url.png");
			if (url && *url){
				char *filename = g_concat_dir_and_file (desktop_directory, key);

				desktop_create_url (filename, title, url, icon2, FALSE);
				g_free (filename);
			}

			if (url)
				g_free (url);
			g_free (icon2);
		}
		g_free (title);
		g_free (file_and_section);
		gnome_config_pop_prefix ();
		
		/* Get next section name */
		iterator_handle = gnome_config_iterator_next (
			iterator_handle, &key, NULL);

	} while (iterator_handle);

	g_free (config_path);
}

static void
desktop_init_at (const char *dir)
{
	DIR *d;
	struct dirent *dent;
	struct stat s;
	const int links_extlen = sizeof (".links")-1;

	d = opendir (dir);
	if (!d)
		return;

	while ((dent = readdir (d)) != NULL){
		int len = strlen (dent->d_name);
		char *fname;

		fname = g_concat_dir_and_file (dir, dent->d_name);
		if (stat (fname, &s) < 0) {
			g_free (fname);
			continue;
		}
		if (S_ISDIR (s.st_mode)) {
			g_free (fname);
			continue;
		}
		if (is_exe (s.st_mode)) {
			/* let's try running it */
			char *desktop_quoted;
			char *command;

			desktop_quoted = name_quote (desktop_directory, 0);
			command = g_strconcat (fname, " --desktop-dir=", desktop_quoted, NULL);
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
		g_free (fname);
	}
	closedir (d);
}

void
gdesktop_links_init (void)
{
	char *link_name;
	char *icon;
	char *dir;

	/* Create the link to the user's home directory so that he will have an icon */
	link_name = g_concat_dir_and_file (desktop_directory, _("Home directory"));
	mc_symlink (gnome_user_home_dir, link_name);
	g_free (link_name);

	/* Create the link to the user's trash directory */
	link_name = g_concat_dir_and_file (desktop_directory, "Trash.gmc");
	icon = gnome_pixmap_file ("mc/gnome-trashcan.png");
	mc_mkdir (link_name, S_IRUSR | S_IWUSR | S_IXUSR );
	if (icon){
		gnome_metadata_set (link_name, "icon-filename", strlen (icon) + 1, icon);
		g_free (icon);
	}
	gnome_metadata_set (link_name, "icon-caption", strlen (_("Trash")) + 1, _("Trash"));
	g_free (link_name);

	/* Create custom links */

	desktop_init_at (DESKTOP_INIT_DIR);

	dir = gnome_libdir_file ("desktop-links");
	if (dir) {
		desktop_init_at (dir);
		g_free (dir);
	}
}

/* Session management for the Midnight Commander
 *
 * Copyright (C) 1999 the Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#include <config.h>
#include "main.h"
#include "../vfs/vfs.h"
#include "gcmd.h"
#include "gmain.h"
#include "gscreen.h"
#include "gsession.h"
#include <gnome.h>


/* Whether we are session managed or not */
int session_have_sm = FALSE;

/* The master SM client */
static GnomeClient *master_client;

/* Structure to hold information for a panel to be created */
typedef struct {
	char *cwd;
	int column_width[GMC_COLUMNS];
	char *user_format;
	int list_type;
} PanelInfo;


/* Saves the SM info for a panel */
static void
save_panel_info (WPanel *panel)
{
 	char section[50];
	char key[50];
	char *path;
	int i;

	g_snprintf (section, sizeof (section), "panel %d", panel->id);

	path = g_strconcat (section, "/cwd", NULL);
	gnome_config_set_string (path, panel->cwd);
	g_free (path);

	path = g_strconcat (section, "/user_format", NULL);
	gnome_config_set_string (path, panel->user_format);
	g_free (path);

	for (i = 0; i < GMC_COLUMNS; i++) {
		g_snprintf (key, sizeof (key), "/column_width_%i", i);
		path = g_strconcat (section, key, NULL);
		gnome_config_set_int (path, panel->column_width[i]);
		g_free (path);
	}

	path = g_strconcat (section, "/list_type", NULL);
	gnome_config_set_int (path, panel->list_type);
	g_free (path);
}

/* Loads a panel from the information in the specified gnome-config file/section */
static PanelInfo *
load_panel_info (char *file, char *section)
{
	PanelInfo *pi;
	char *prefix;
	char *cwd, *user_format;
	char key[50];
	int i;

	pi = NULL;

	prefix = g_strconcat (file, section, "/", NULL);
	printf ("Prefix is \"%s\"\n", prefix);
	gnome_config_push_prefix (prefix);

	cwd = gnome_config_get_string ("cwd");
	if (!cwd) {
		g_warning ("Could not read panel data for \"%s\"", prefix);
		gnome_config_pop_prefix ();
		g_free (prefix);
		return NULL;
	}

	pi = g_new (PanelInfo, 1);
	pi->cwd = cwd;

	user_format = gnome_config_get_string ("user_format");
	if (!user_format)
		user_format = g_strdup (DEFAULT_USER_FORMAT);

	pi->user_format = user_format;

	for (i = 0; i < GMC_COLUMNS; i++) {
		g_snprintf (key, sizeof (key), "column_width_%i=0", i);
		pi->column_width[i] = gnome_config_get_int_with_default (key, NULL);
	}

	pi->list_type = gnome_config_get_int_with_default ("list_type=0", NULL);

	gnome_config_pop_prefix ();
	g_free (prefix);

	return pi;
}

/* Frees a panel info structure */
static void
free_panel_info (PanelInfo *pi)
{
	g_free (pi->cwd);
	g_free (pi->user_format);
	g_free (pi);
}

static void
create_panel_from_info (PanelInfo *pi)
{
	WPanel *panel;

	panel = new_panel_at (pi->cwd);

	g_free (panel->user_format);
	panel->user_format = g_strdup (pi->user_format);
	memcpy (panel->column_width, pi->column_width, sizeof (pi->column_width));
	panel->list_type = pi->list_type;
}

static void
load_session_info (char *filename)
{
	void *iterator;
	char *key, *value;
	PanelInfo *pi;

	iterator = gnome_config_init_iterator_sections (filename);

	while ((iterator = gnome_config_iterator_next (iterator, &key, &value)) != NULL)
		if (key && strncmp (key, "panel ", 6) == 0) {
			pi = load_panel_info (filename, key);
			g_free (key);
			if (!pi)
				continue;

			create_panel_from_info (pi);
			free_panel_info (pi);
		}
}

static void
create_default_panel (const char *startup_dir)
{
	char buf[MC_MAXPATHLEN];
	const char *dir = buf;

	if (startup_dir == NULL)
		mc_get_current_wd (buf, MC_MAXPATHLEN);
	else
		dir = startup_dir;

	new_panel_with_geometry_at (dir, cmdline_geometry);
}

/* Callback from the master client to save the session */
static gint
session_save (GnomeClient *client, gint phase, GnomeSaveStyle save_style, gint shutdown,
	      GnomeInteractStyle interact_style, gint fast, gpointer data)
{
	char *prefix;
	GList *l;
	PanelContainer *pc;
	gchar *discard_args[] = { "rm", NULL };

	prefix = gnome_client_get_config_prefix (client);
	gnome_config_push_prefix (prefix);

	/* Save the panels */

	for (l = containers; l; l = l->next) {
		pc = l->data;
		if (!is_a_desktop_panel (pc->panel))
			save_panel_info (pc->panel);
	}

	gnome_config_pop_prefix ();
	gnome_config_sync ();

	discard_args[1] = gnome_config_get_real_path (prefix);
	gnome_client_set_discard_command (client, 2, discard_args);
	g_free (discard_args[1]);

	return TRUE;
}

/* Callback from the master client to terminate the session */
static void
session_die (GnomeClient *client, gpointer data)
{
	gmc_do_quit ();
}

/**
 * session_init:
 * @void:
 *
 * Initializes session management.  Contacts the master client and
 * connects the appropriate signals.
 **/
void
session_init ()
{
	master_client = gnome_master_client ();

	session_have_sm = (gnome_client_get_flags (master_client) & GNOME_CLIENT_IS_CONNECTED) != 0;

	gtk_signal_connect (GTK_OBJECT (master_client), "save_yourself",
			    (GtkSignalFunc) session_save,
			    NULL);
	gtk_signal_connect (GTK_OBJECT (master_client), "die",
			    (GtkSignalFunc) session_die,
			    NULL);

	session_set_restart (TRUE);
}

/**
 * session_load:
 * @void:
 *
 * Loads the saved session.
 **/
void
session_load (void)
{
	char *filename;

	if (gnome_client_get_flags (master_client) & GNOME_CLIENT_RESTORED) {
		filename = gnome_client_get_config_prefix (master_client);
		load_session_info (filename);
	} else if (!nowindows)
		create_default_panel (this_dir);
}

/**
 * session_set_restart:
 * @restart: TRUE if it should restart immediately, FALSE if it should restart
 * never.
 *
 * Sets the restart style of the session-managed client.
 **/
void
session_set_restart (int restart)
{
	gnome_client_set_restart_style (master_client,
					(restart
					 ? GNOME_RESTART_IMMEDIATELY
					 : GNOME_RESTART_NEVER));
}

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
} PanelInfo;


/* Saves the SM info for a panel */
static void
save_panel_info (WPanel *panel)
{
	char section[50];
	char *key;

	sprintf (section, "panel %d", panel->id);

	key = g_strconcat (section, "/cwd", NULL);
	gnome_config_set_string (key, panel->cwd);
	g_free (key);

	/* FIXME: save information about list column sizes, etc. */
}

/* Loads a panel from the information in the specified gnome-config file/section */
static PanelInfo *
load_panel_info (char *file, char *section)
{
	PanelInfo *pi;
	char *prefix;
	char *cwd;

	pi = NULL;

	prefix = g_strconcat (file, section, "/", NULL);
	printf ("Prefix is \"%s\"\n", prefix);
	gnome_config_push_prefix (prefix);

	cwd = gnome_config_get_string ("cwd");
	if (cwd) {
		pi = g_new (PanelInfo, 1);
		pi->cwd = cwd;
	} else
		g_warning ("Could not read panel data for \"%s\"", prefix);

	gnome_config_pop_prefix ();
	g_free (prefix);

	return pi;
}

/* Frees a panel info structure */
static void
free_panel_info (PanelInfo *pi)
{
	g_free (pi->cwd);
	g_free (pi);
}

/* Idle handler to create the list of panels loaded from the session info file */
static gint
idle_create_panels (gpointer data)
{
	GSList *panels, *p;
	PanelInfo *pi;

	panels = data;

	for (p = panels; p; p = p->next) {
		pi = p->data;
		new_panel_at (pi->cwd);
		free_panel_info (pi);
	}

	g_slist_free (panels);
	return FALSE;
}

/* Loads session information from the specified gnome-config file */
static void
load_session_info (char *filename)
{
	void *iterator;
	char *key, *value;
	GSList *panels;
	PanelInfo *pi;

	/* Slurp the list of panels to create */

	iterator = gnome_config_init_iterator_sections (filename);
	panels = NULL;

	while ((iterator = gnome_config_iterator_next (iterator, &key, &value)) != NULL)
		if (key && strncmp (key, "panel ", 6) == 0) {
			pi = load_panel_info (filename, key);
			panels = g_slist_prepend (panels, pi);
			g_free (key);
		}

	/* Create the panels in the idle loop to keep run_dlg() happy.  See the
	 * comment in gmain.c:non_corba_create_panels().
	 */

	gtk_idle_add_priority (GTK_PRIORITY_DEFAULT + 1, idle_create_panels, panels);
}

/* Idle handler to create the default panel */
static gint
idle_create_default_panel (gpointer data)
{
	char *cwd;

	cwd = data;
	new_panel_with_geometry_at (cwd, cmdline_geometry);
	g_free (cwd);

	return FALSE;
}

/* Queues the creation of the default panel */
static void
create_default_panel (void)
{
	char buf[MC_MAXPATHLEN];

	mc_get_current_wd (buf, MC_MAXPATHLEN);

	gtk_idle_add_priority (GTK_PRIORITY_DEFAULT + 1, idle_create_default_panel, g_strdup (buf));
}

/* Callback from the master client to save the session */
static gint
session_save (GnomeClient *client, gint phase, GnomeSaveStyle save_style, gint shutdown,
	      GnomeInteractStyle interact_style, gint fast, gpointer data)
{
	char *prefix;
	GList *l;
	PanelContainer *pc;

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
		create_default_panel ();
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

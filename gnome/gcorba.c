/* CORBA support for the Midnight Commander
 *
 * Copyright (C) 1999 The Free Sofware Foundation
 *
 * Authors: Miguel de Icaza <miguel@nuclecu.unam.mx>
 *          Federico Mena <federico@nuclecu.unam.mx>
 *          Elliot Lee <sopwith@cuc.edu>
 */

#include <config.h>
#include "panel.h"
#include "../vfs/vfs.h"
#include <libgnorba/gnorba.h>
#include "FileManager.h"
#include "gcmd.h"
#include "gcorba.h"
#include "gdesktop.h"
#include "global.h"
#include "gmain.h"
#include "gscreen.h"
#include "main.h"


/* The ORB for the whole program */
CORBA_ORB orb = CORBA_OBJECT_NIL;

/* The POA */
PortableServer_POA poa = CORBA_OBJECT_NIL;



/* Desktop servant */
typedef struct {
	POA_GNOME_FileManager_Desktop servant;
} DesktopServant;

static PortableServer_ServantBase__epv desktop_base_epv;
static POA_GNOME_FileManager_Desktop__epv desktop_epv;
static POA_GNOME_FileManager_Desktop__vepv desktop_vepv;

/* Window servant */
typedef struct {
	POA_GNOME_FileManager_Window servant;

	WPanel *panel;
} WindowServant;

static PortableServer_ServantBase__epv window_base_epv;
static POA_GNOME_FileManager_Window__epv window_epv;
static POA_GNOME_FileManager_Window__vepv window_vepv;

/* WindowFactory servant */

typedef struct {
	POA_GNOME_FileManager_WindowFactory servant;
} WindowFactoryServant;

static PortableServer_ServantBase__epv window_factory_base_epv;
static POA_GNOME_GenericFactory__epv window_factory_generic_factory_epv;
static POA_GNOME_FileManager_WindowFactory__epv window_factory_epv;
static POA_GNOME_FileManager_WindowFactory__vepv window_factory_vepv;



/* References to the window factory and desktop server objects */

static CORBA_Object window_factory_server = CORBA_OBJECT_NIL;
static CORBA_Object desktop_server = CORBA_OBJECT_NIL;



/* Desktop implementation */

/* Desktop::rescan method */
static void
Desktop_rescan (PortableServer_Servant servant, CORBA_Environment *ev)
{
	desktop_reload_icons (FALSE, 0, 0);
}

/* Desktop::rescan_devices method */
static void
Desktop_rescan_devices (PortableServer_Servant servant, CORBA_Environment *ev)
{
	desktop_rescan_devices ();
}

/* Fills the vepv structure for the desktop object */
static void
Desktop_class_init (void)
{
	static int inited = FALSE;

	if (inited)
		return;

	inited = TRUE;

	desktop_epv.rescan = Desktop_rescan;
	desktop_epv.rescan_devices = Desktop_rescan_devices;

	desktop_vepv._base_epv = &desktop_base_epv;
	desktop_vepv.GNOME_FileManager_Desktop_epv = &desktop_epv;
}

/* Creates a reference for the desktop object */
static GNOME_FileManager_Desktop
Desktop_create (PortableServer_POA poa, CORBA_Environment *ev)
{
	DesktopServant *ds;
	PortableServer_ObjectId *objid;

	Desktop_class_init ();

	ds = g_new0 (DesktopServant, 1);
	ds->servant.vepv = &desktop_vepv;

	POA_GNOME_FileManager_Desktop__init ((PortableServer_Servant) ds, ev);
	objid = PortableServer_POA_activate_object (poa, ds, ev);
	CORBA_free (objid);

	return PortableServer_POA_servant_to_reference (poa, ds, ev);
}



/* Window implementation */

/* Window::close method */
static void
Window_close (PortableServer_Servant servant, CORBA_Environment *ev)
{
	WindowServant *ws;

	ws = (WindowServant *) servant;
	gnome_close_panel (GTK_WIDGET (ws->panel->xwindow), ws->panel);
}

/* Destroys the servant for an image window */
static void
window_destroy (WindowServant *ws, CORBA_Environment *ev)
{
	PortableServer_ObjectId *objid;

	objid = PortableServer_POA_servant_to_id (poa, ws, ev);
	PortableServer_POA_deactivate_object (poa, objid, ev);
	CORBA_free (objid);

	POA_GNOME_FileManager_Window__fini (ws, ev);
	g_free (ws);
}

/* Fills the vepv structure for the window object */
static void
Window_class_init (void)
{
	static int inited = FALSE;

	if (inited)
		return;

	inited = TRUE;

	window_epv.close = Window_close;

	window_vepv._base_epv = &window_base_epv;
	window_vepv.GNOME_FileManager_Window_epv = &window_epv;
}



/* WindowFactory implementation */

/* WindowFactory::the_desktop attribute getter */
static GNOME_FileManager_Desktop
WindowFactory_get_the_desktop (PortableServer_Servant servant,
			       CORBA_Environment *ev)
{
	g_assert (desktop_server != CORBA_OBJECT_NIL);

	return CORBA_Object_duplicate (desktop_server, ev);
}

/* Called when a panel created through CORBA is destroyed */
static void
panel_destroyed (GtkObject *object, gpointer data)
{
	WindowServant *ws;
	CORBA_Environment ev;

	ws = data;

	CORBA_exception_init (&ev);
	window_destroy (ws, &ev);
	CORBA_exception_free (&ev);
}

/* Returns a servant for a panel, creating one if necessary */
static WindowServant *
window_servant_from_panel (WPanel *panel, CORBA_Environment *ev)
{
	WindowServant *ws;
	PortableServer_ObjectId *objid;

	if (panel->servant)
		return panel->servant;

	Window_class_init ();

	ws = g_new0 (WindowServant, 1);
	ws->servant.vepv = &window_vepv;

	POA_GNOME_FileManager_Window__init ((PortableServer_Servant) ws, ev);
	objid = PortableServer_POA_activate_object (poa, ws, ev);
	CORBA_free (objid);

	ws->panel = panel;
	panel->servant = ws;

	gtk_signal_connect (GTK_OBJECT (panel->xwindow), "destroy",
			    (GtkSignalFunc) panel_destroyed,
			    ws);

	return ws;
}

/* WindowFactory::create_window method */
static GNOME_FileManager_Window
WindowFactory_create_window (PortableServer_Servant servant,
			     CORBA_char *dir,
			     CORBA_Environment *ev)
{
	WPanel *panel;
	WindowServant *ws;

	panel = new_panel_at (dir);
	ws = window_servant_from_panel (panel, ev);

	return PortableServer_POA_servant_to_reference (poa, ws, ev);
}

/* WindowFactory::rescan_directory method */
static void
WindowFactory_rescan_directory (PortableServer_Servant servant,
				CORBA_char *dir,
				CORBA_Environment *ev)
{
	PanelContainer *pc;
	GList *l;
	int len;

	/* We do a blind compare against the panel's cwd */

	len = strlen (dir);
	if (dir[len - 1] == PATH_SEP)
		len--;

	for (l = containers; l; l = l->next) {
		pc = l->data;

		if (strncmp (dir, pc->panel->cwd, len) == 0
		    && (pc->panel->cwd[len] == 0 || pc->panel->cwd[len] == PATH_SEP))
			update_one_panel_widget (pc->panel, UP_RELOAD, UP_KEEPSEL);
	}
}

/* WindowFactory::close_invalid_windows method */
static void
WindowFactory_close_invalid_windows (PortableServer_Servant servant,
				     CORBA_Environment *ev)
{
	PanelContainer *pc;
	GList *l;

	/* To see if a panel is valid or not, we try to cd to its cwd.  If this
	 * fails, then we destroy the panel's window.
	 */

	l = containers;
	while (l) {
		pc = l->data;
		l = l->next;

		if (mc_chdir (pc->panel->cwd) != 0)
			gnome_close_panel (GTK_WIDGET (pc->panel->xwindow), pc->panel);
	}
}

/* Creates an object reference for a panel */
static GNOME_FileManager_Window
window_reference_from_panel (WPanel *panel, CORBA_Environment *ev)
{
	WindowServant *ws;

	ws = window_servant_from_panel (panel, ev);
	return PortableServer_POA_servant_to_reference (poa, ws, ev);
}

/* WindowFactory::get_window_by_directory method */
static GNOME_FileManager_WindowFactory_WindowSeq *
WindowFactory_get_windows_by_directory (PortableServer_Servant servant,
					CORBA_char *dir,
					CORBA_Environment *ev)
{
	GNOME_FileManager_WindowFactory_WindowSeq *seq;
	PanelContainer *pc;
	GList *l;
	int n, i;

	/* We return a sequence of the windows that match the specified
         * directory.
	 */

	/* Count 'em */
	n = 0;
	for (l = containers; l; l = l->next) {
		pc = l->data;

		if (strcmp (pc->panel->cwd, dir) == 0)
			n++;
	}

	seq = GNOME_FileManager_WindowFactory_WindowSeq__alloc ();
	seq->_length = n;
	seq->_buffer = CORBA_sequence_GNOME_FileManager_Window_allocbuf (n);

	i = 0;

	for (l = containers; l; l = l->next) {
		pc = l->data;

		if (strcmp (pc->panel->cwd, dir) == 0)
			seq->_buffer[i++] = window_reference_from_panel (pc->panel, ev);
	}

	return seq;
}

/* WindowFactory GenericFactory::supports method */
static CORBA_boolean
WindowFactory_supports (PortableServer_Servant servant,
			CORBA_char *obj_goad_id,
			CORBA_Environment *ev)
{
	if (strcmp (obj_goad_id, "IDL:GNOME:FileManager:Window:1.0") == 0
	    || strcmp (obj_goad_id, "IDL:GNOME:FileManager:Desktop:1.0") == 0)
		return CORBA_TRUE;
	else
		return CORBA_FALSE;
}

/* WindowFactory GenericFactory::create_object method */
static CORBA_Object
WindowFactory_create_object (PortableServer_Servant servant,
			     CORBA_char *goad_id,
			     GNOME_stringlist *params,
			     CORBA_Environment *ev)
{
	if (strcmp (goad_id, "IDL:GNOME:FileManager:Window:1.0") != 0)
		return WindowFactory_create_window (
			servant,
			params->_length != 0 ? params->_buffer[0] : home_dir,
			ev);
	else if (strcmp (goad_id, "IDL:GNOME:FileManager:Desktop:1.0") == 0)
		return WindowFactory_get_the_desktop (servant, ev);
	else {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_GenericFactory_CannotActivate,
				     NULL);
		return CORBA_OBJECT_NIL;
	}
}

/* Fills the vepv structure for the window factory object */
static void
WindowFactory_class_init (void)
{
	static int inited = FALSE;

	if (inited)
		return;

	inited = TRUE;

	window_factory_generic_factory_epv.supports = WindowFactory_supports;
	window_factory_generic_factory_epv.create_object = WindowFactory_create_object;

	window_factory_epv._get_the_desktop = WindowFactory_get_the_desktop;
	window_factory_epv.create_window = WindowFactory_create_window;
	window_factory_epv.rescan_directory = WindowFactory_rescan_directory;
	window_factory_epv.close_invalid_windows = WindowFactory_close_invalid_windows;
	window_factory_epv.get_windows_by_directory = WindowFactory_get_windows_by_directory;

	window_factory_vepv._base_epv = &window_factory_base_epv;
	window_factory_vepv.GNOME_GenericFactory_epv = &window_factory_generic_factory_epv;
	window_factory_vepv.GNOME_FileManager_WindowFactory_epv = &window_factory_epv;
}

/* Creates a reference for the window factory object */
static GNOME_FileManager_WindowFactory
WindowFactory_create (PortableServer_POA poa, CORBA_Environment *ev)
{
	WindowFactoryServant *wfs;
	PortableServer_ObjectId *objid;

	WindowFactory_class_init ();

	wfs = g_new0 (WindowFactoryServant, 1);
	wfs->servant.vepv = &window_factory_vepv;

	POA_GNOME_FileManager_WindowFactory__init ((PortableServer_Servant) wfs, ev);
	objid = PortableServer_POA_activate_object (poa, wfs, ev);
	CORBA_free (objid);

	return PortableServer_POA_servant_to_reference (poa, wfs, ev);
}



/* Creates and registers the CORBA servers.  Returns TRUE on success, FALSE
 * otherwise.
 */
static int
register_servers (void)
{
	CORBA_Environment ev;
	int retval;
	int v;

	retval = FALSE;
	CORBA_exception_init (&ev);

	/* Register the window factory and see if it was already there */

	window_factory_server = WindowFactory_create (poa, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		goto out;

	v = goad_server_register (CORBA_OBJECT_NIL, window_factory_server,
				  "IDL:GNOME:FileManager:WindowFactory:1.0", "object", &ev);
	switch (v) {
	case 0:
		corba_have_server = FALSE;
		break;

	case -2:
		corba_have_server = FALSE;
		break;

	default:
		goto out;
	}

	/* Register the desktop server */

	desktop_server = Desktop_create (poa, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		goto out;

	goad_server_register (CORBA_OBJECT_NIL, desktop_server,
			      "IDL:GNOME:FileManager:Desktop:1.0", "object", &ev);

	retval = TRUE;

	/* Done */
 out:
	CORBA_exception_free (&ev);

	return retval;
}

/**
 * corba_init_server:
 * @void:
 *
 * Initializes the CORBA server for GMC.  Returns whether initialization was
 * successful or not, and sets the global corba_have_server variable.
 *
 * Return value: TRUE if successful, FALSE otherwise.
 **/
int
corba_init_server (void)
{
	int retval;
	CORBA_Environment ev;

	retval = FALSE;
	CORBA_exception_init (&ev);

	/* Get the POA and create the server */

	poa = (PortableServer_POA) CORBA_ORB_resolve_initial_references (orb, "RootPOA", &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		goto out;

	CORBA_exception_free (&ev);

	/* See if the servers are there */

	window_factory_server = goad_server_activate_with_id (
		NULL,
		"IDL:GNOME:FileManager:WindowFactory:1.0",
		GOAD_ACTIVATE_EXISTING_ONLY,
		NULL);

	desktop_server = goad_server_activate_with_id (
		NULL,
		"IDL:GNOME:FileManager:Desktop:1.0",
		GOAD_ACTIVATE_EXISTING_ONLY,
		NULL);

	if (window_factory_server != CORBA_OBJECT_NIL) {
		corba_have_server = TRUE;
		retval = TRUE;
	} else
		retval = register_servers ();

 out:
	return retval;
}

/**
 * corba_activate_server:
 * @void:
 *
 * Activates the POA manager and thus makes the services available to the
 * outside world.
 **/
void
corba_activate_server (void)
{
	CORBA_Environment ev;
	PortableServer_POAManager poa_manager;

	/* Do nothing if the server is already running */
	if (corba_have_server)
		return;

	CORBA_exception_init (&ev);

	poa_manager = PortableServer_POA__get_the_POAManager (poa, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		goto out;

	PortableServer_POAManager_activate (poa_manager, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		goto out;

 out:

	CORBA_exception_free (&ev);
}

/**
 * corba_create_window:
 * @dir: The directory in which to create the window, or NULL for the cwd.
 *
 * Creates a GMC window using a CORBA call to the server.
 **/
void
corba_create_window (char *dir)
{
	CORBA_Environment ev;
	char cwd[MC_MAXPATHLEN];

	if (dir == NULL) {
		mc_get_current_wd (cwd, MC_MAXPATHLEN);
		dir = cwd;
	}

	CORBA_exception_init (&ev);
	GNOME_FileManager_WindowFactory_create_window (window_factory_server, dir, &ev);
	CORBA_exception_free (&ev);
}

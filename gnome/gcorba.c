/* CORBA support for the Midhignt Commander
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
#include "global.h"
#include "gmain.h"
#include "gscreen.h"
#include "main.h"


/* The ORB for the whole program */
CORBA_ORB orb = CORBA_OBJECT_NIL;

/* The POA */
PortableServer_POA poa;



/* Desktop servant */
typedef struct {
	POA_GNOME_FileManager_Desktop servant;
} DesktopServant;

static POA_GNOME_FileManager_Desktop__epv desktop_epv;
static POA_GNOME_FileManager_Desktop__vepv desktop_vepv;

/* Window servant */
typedef struct {
	POA_GNOME_FileManager_Window servant;

	WPanel *panel;
} WindowServant;

static POA_GNOME_FileManager_Window__epv window_epv;
static POA_GNOME_FileManager_Window__vepv window_vepv;

/* WindowFactory servant */

typedef struct {
	POA_GNOME_FileManager_WindowFactory servant;
} WindowFactoryServant;

static POA_GNOME_FileManager_WindowFactory__epv window_factory_epv;
static POA_GNOME_FileManager_WindowFactory__vepv window_factory_vepv;



/* References to the window factory and desktop server objects */

static CORBA_Object window_factory_server = CORBA_OBJECT_NIL;
static CORBA_Object desktop_server = CORBA_OBJECT_NIL;



/* Window implementation */





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

	for (l = containers; l; l = l->next) {
		pc = l->data;

		if (mc_chdir (pc->panel->cwd) != 0)
			gnome_close_panel (pc->panel->xwindow, pc->panel);
	}
}

/* Creates an object reference for a panel */
static GNOME_FileManagerWindow
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
	seq->_buffer = CORBA_sequence_GNOME_FileManager_Window_allocbuf (n);

	for (l = containers, i = 0; l; l = l->next, i++) {
		pc = l->data;

		if (strcmp (pc->panel->cwd, dir) == 0)
			seq->_buffer[i] = window_reference_from_panel (pc->panel, ev);
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
	else if (strcmp (obj_goad_id, "IDL:GNOME:FileManager:Desktop:1.0") == 0) {
		/* FIXME */
	} else {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_GenericFactory_CannotActivate,
				     NULL);
		return CORBA_OBJECT_NIL;
	}
}
























/*** Implementation stub prototypes ***/

static void impl_GNOME_FileManagerWindow__destroy (impl_POA_GNOME_FileManagerWindow *servant,
						   CORBA_Environment *ev);
static void impl_GNOME_FileManagerWindow_close (impl_POA_GNOME_FileManagerWindow *servant,
						CORBA_Environment *ev);

static void impl_GNOME_FileManagerFactory__destroy (impl_POA_GNOME_FileManagerFactory *servant,
						    CORBA_Environment *ev);

static CORBA_boolean impl_GNOME_FileManagerFactory_supports (
	impl_POA_GNOME_FileManagerFactory *servant,
	const CORBA_char *obj_goad_id,
	CORBA_Environment *ev);

static CORBA_Object impl_GNOME_FileManagerFactory_create_object (
	impl_POA_GNOME_FileManagerFactory *servant,
	const CORBA_char *goad_id,
	const GNOME_stringlist *params,
	CORBA_Environment *ev);

static GNOME_FileManagerWindow impl_GNOME_FileManagerFactory_create_window (
	impl_POA_GNOME_FileManagerFactory * servant,
	const CORBA_char * dir,
	CORBA_Environment * ev);

/*** epv structures ***/

static PortableServer_ServantBase__epv impl_GNOME_FileManagerWindow_base_epv =
{
	NULL,							/* _private data */
	NULL,							/* finalize routine */
	NULL,							/* default_POA routine */
};

static POA_GNOME_FileManagerWindow__epv impl_GNOME_FileManagerWindow_epv =
{
	NULL,							/* _private */
	(gpointer) &impl_GNOME_FileManagerWindow_close,

};

static PortableServer_ServantBase__epv impl_GNOME_FileManagerFactory_base_epv =
{
	NULL,							/* _private data */
	NULL,							/* finalize routine */
	NULL,							/* default_POA routine */
};

static POA_GNOME_FileManagerFactory__epv impl_GNOME_FileManagerFactory_epv =
{
	NULL,							/* _private */
	(gpointer) &impl_GNOME_FileManagerFactory_create_window,
};

static POA_GNOME_GenericFactory__epv impl_GNOME_FileManagerFactory_GNOME_GenericFactory_epv =
{
	NULL,							/* _private */
	(gpointer) &impl_GNOME_FileManagerFactory_supports,
	(gpointer) &impl_GNOME_FileManagerFactory_create_object
};

/*** vepv structures ***/

static POA_GNOME_FileManagerWindow__vepv impl_GNOME_FileManagerWindow_vepv =
{
	&impl_GNOME_FileManagerWindow_base_epv,
	&impl_GNOME_FileManagerWindow_epv
};

static POA_GNOME_FileManagerFactory__vepv impl_GNOME_FileManagerFactory_vepv =
{
	&impl_GNOME_FileManagerFactory_base_epv,
	&impl_GNOME_FileManagerFactory_GNOME_GenericFactory_epv,
	&impl_GNOME_FileManagerFactory_epv
};

/*** Stub implementations ***/

static GNOME_FileManagerWindow
impl_GNOME_FileManagerWindow__create(PortableServer_POA poa,
				     impl_POA_GNOME_FileManagerWindow **servant,
				     CORBA_Environment *ev)
{
	GNOME_FileManagerWindow retval;
	impl_POA_GNOME_FileManagerWindow *newservant;
	PortableServer_ObjectId *objid;

	newservant = g_new0 (impl_POA_GNOME_FileManagerWindow, 1);
	newservant->servant.vepv = &impl_GNOME_FileManagerWindow_vepv;
	newservant->poa = poa;
	POA_GNOME_FileManagerWindow__init ((PortableServer_Servant) newservant, ev);
	objid = PortableServer_POA_activate_object (poa, newservant, ev);
	CORBA_free (objid);
	retval = PortableServer_POA_servant_to_reference (poa, newservant, ev);

	if (servant)
		*servant = newservant;

	return retval;
}

/* You shouldn't call this routine directly without first deactivating the servant... */
static void
impl_GNOME_FileManagerWindow__destroy(impl_POA_GNOME_FileManagerWindow *servant,
				      CORBA_Environment *ev)
{
	PortableServer_ObjectId *objid;

	objid = PortableServer_POA_servant_to_id (servant->poa, servant, ev);
	PortableServer_POA_deactivate_object (servant->poa, objid, ev);
	CORBA_free (objid);

	POA_GNOME_FileManagerWindow__fini ((PortableServer_Servant) servant, ev);
	g_free(servant);
}

static void
impl_GNOME_FileManagerWindow_close(impl_POA_GNOME_FileManagerWindow *servant,
				   CORBA_Environment *ev)
{
	gnome_close_panel (GTK_WIDGET (servant->panel->xwindow), servant->panel);
}

static GNOME_FileManagerFactory
impl_GNOME_FileManagerFactory__create(PortableServer_POA poa,
				      impl_POA_GNOME_FileManagerFactory **servant,
				      CORBA_Environment *ev)
{
	GNOME_FileManagerFactory retval;
	impl_POA_GNOME_FileManagerFactory *newservant;
	PortableServer_ObjectId *objid;

	newservant = g_new0 (impl_POA_GNOME_FileManagerFactory, 1);
	newservant->servant.vepv = &impl_GNOME_FileManagerFactory_vepv;
	newservant->poa = poa;
	POA_GNOME_FileManagerFactory__init ((PortableServer_Servant) newservant, ev);
	objid = PortableServer_POA_activate_object (poa, newservant, ev);
	CORBA_free (objid);
	retval = PortableServer_POA_servant_to_reference (poa, newservant, ev);

	if (servant)
		*servant = newservant;

	return retval;
}

/* You shouldn't call this routine directly without first deactivating the servant... */
static void
impl_GNOME_FileManagerFactory__destroy(impl_POA_GNOME_FileManagerFactory *servant,
				       CORBA_Environment *ev)
{
	PortableServer_ObjectId *objid;

	objid = PortableServer_POA_servant_to_id (servant->poa, servant, ev);
	PortableServer_POA_deactivate_object (servant->poa, objid, ev);
	CORBA_free (objid);

	POA_GNOME_FileManagerFactory__fini ((PortableServer_Servant) servant, ev);
	g_free (servant);
}

static CORBA_boolean
impl_GNOME_FileManagerFactory_supports (impl_POA_GNOME_FileManagerFactory *servant,
					const CORBA_char *obj_goad_id,
					CORBA_Environment *ev)
{
	if (strcmp (obj_goad_id, "IDL:GNOME:FileManagerWindow:1.0") == 0)
		return CORBA_TRUE;
	else
		return CORBA_FALSE;
}

static CORBA_Object
impl_GNOME_FileManagerFactory_create_object (impl_POA_GNOME_FileManagerFactory *servant,
					     const CORBA_char *goad_id,
					     const GNOME_stringlist *params,
					     CORBA_Environment *ev)
{
	if (strcmp (goad_id, "IDL:GNOME:FileManagerWindow:1.0") != 0) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_GenericFactory_CannotActivate,
				     NULL);
		return CORBA_OBJECT_NIL;
	}

	return impl_GNOME_FileManagerFactory_create_window (
		servant,
		params->_length ? params->_buffer[0] : home_dir,
		ev);
}

/* Callback used when a panel is destroyed */
static void
panel_destroyed (GtkObject *object, gpointer data)
{
	impl_POA_GNOME_FileManagerWindow *servant;
	CORBA_Environment ev;

	servant = data;

	CORBA_exception_init (&ev);
	impl_GNOME_FileManagerWindow__destroy (servant, &ev);
	CORBA_exception_free (&ev);
}

static GNOME_FileManagerWindow
impl_GNOME_FileManagerFactory_create_window(impl_POA_GNOME_FileManagerFactory *servant,
					    const CORBA_char *dir,
					    CORBA_Environment *ev)
{
	GNOME_FileManagerWindow retval;
	impl_POA_GNOME_FileManagerWindow *newservant;
	WPanel *panel;

	retval = impl_GNOME_FileManagerWindow__create (servant->poa,
						       &newservant,
						       ev);

	panel = new_panel_at ((dir && *dir) ? dir : home_dir);
	newservant->panel = panel;

	gtk_signal_connect (GTK_OBJECT (panel->xwindow), "destroy",
			    (GtkSignalFunc) panel_destroyed,
			    newservant);

	return retval;
}

/*** Public non-implementation functions ***/

/* Creates the CORBA server */
static int
create_server (void)
{
	CORBA_Environment ev;
	PortableServer_POA poa;
	PortableServer_POAManager poa_manager;
	int retval;
	int v;

	retval = FALSE;
	CORBA_exception_init (&ev);

	/* Get the POA and create the server */

	poa = (PortableServer_POA) CORBA_ORB_resolve_initial_references (orb, "RootPOA", &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		goto out;

	poa_manager = PortableServer_POA__get_the_POAManager (poa, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		goto out;

	PortableServer_POAManager_activate (poa_manager, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		goto out;

	window_factory_server = impl_GNOME_FileManagerFactory__create (poa, NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		goto out;

	CORBA_Object_release ((CORBA_Object) poa, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		goto out;

	/* Register the server and see if it was already there */

	v = goad_server_register (CORBA_OBJECT_NIL, window_factory_server,
				  "IDL:GNOME:FileManager:WindowFactory:1.0", "server", &ev);
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

	retval = TRUE;

	/* Done */

 out:
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
	window_factory_server = goad_server_activate_with_id (
		NULL,
		"IDL:GNOME:FileManager:WindowFactory:1.0",
		GOAD_ACTIVATE_EXISTING_ONLY,
		NULL);

	if (window_factory_server != CORBA_OBJECT_NIL) {
		corba_have_server = TRUE;
		return TRUE;
	} else
		return create_server ();
}

/**
 * corba_create_window:
 * @dir: The directory in which to create the window, or NULL for the cwd.
 *
 * Creates a GMC window using a CORBA call to the server.
 **/
void
corba_create_window (const char *dir)
{
	CORBA_Environment ev;
	char cwd[MC_MAXPATHLEN];

	if (dir == NULL) {
		mc_get_current_wd (cwd, MC_MAXPATHLEN);
		dir = cwd;
	}

	CORBA_exception_init (&ev);
	GNOME_FileManagerFactory_create_window (window_factory_server, dir, &ev);
	CORBA_exception_free (&ev);
}

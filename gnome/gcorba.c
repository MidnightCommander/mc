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
#include <libgnorba/gnorba.h>
#include "FileManager.h"
#include "gcmd.h"
#include "gcorba.h"
#include "global.h"
#include "gmain.h"

/*** Global variables ***/

CORBA_ORB orb = CORBA_OBJECT_NIL;

/*** Local variables ***/

/* Reference to the server object */
static CORBA_Object gmc_server;

/*** App-specific servant structures ***/

typedef struct {
	POA_GNOME_FileManagerWindow servant;
	PortableServer_POA poa;

	WPanel *panel;
} impl_POA_GNOME_FileManagerWindow;

typedef struct {
	POA_GNOME_FileManagerFactory servant;
	PortableServer_POA poa;
} impl_POA_GNOME_FileManagerFactory;

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
	if (strcmp (obj_goad_id, "gmc_filemanager_window") == 0)
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
	if (strcmp (goad_id, "gmc_filemanager_window") != 0) {
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

	gmc_server = impl_GNOME_FileManagerFactory__create (poa, NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		goto out;

	CORBA_Object_release ((CORBA_Object) poa, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		goto out;

	/* Register the server and see if it was already there */

	v = goad_server_register (CORBA_OBJECT_NIL, gmc_server,
				  "gmc_filemanager_factory", "server", &ev);
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
 * Initializes the CORBA factory object for gmc.  Returns whether initialization
 * was successful or not, and sets the global corba_have_server variable.
 * 
 * Return value: TRUE if successful, FALSE otherwise.
 **/
int
corba_init_server (void)
{
	gmc_server = goad_server_activate_with_id (NULL,
						   "gmc_filemanager_factory",
						   GOAD_ACTIVATE_EXISTING_ONLY,
						   NULL);

	if (gmc_server != CORBA_OBJECT_NIL) {
		corba_have_server = TRUE;
		return TRUE;
	} else
		return create_server ();
}

/**
 * corba_create_window:
 * @void: 
 * 
 * Creates a GMC window using a CORBA call to the server.
 **/
void
corba_create_window (void)
{
	CORBA_Environment ev;
	char cwd[MC_MAXPATHLEN];

	mc_get_current_wd (cwd, MC_MAXPATHLEN);

	CORBA_exception_init (&ev);
	GNOME_FileManagerFactory_create_window (gmc_server, cwd, &ev);
	CORBA_exception_free (&ev);
}

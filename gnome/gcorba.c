/*
 * gcorba.c:  CORBA support code for the GNOME Midnight Commander
 *
 * Author:
 *    Miguel de Icaza (miguel@gnu.org)
 *    Elliot Lee (sopwith@cuc.edu)
 */
#include <config.h>
#include <libgnorba/gnorba.h>
#include <libgnorba/gnome-factory.h>
#include "FileManager.h"
#include <config.h>
#include "x.h"
#include <stdio.h>
#include <sys/stat.h>
#include "dir.h"
#include "global.h"
#include "panel.h"
#include "gscreen.h"
#include "main.h"
#include "gmain.h"
#include "gcorba.h"

PortableServer_POA poa;
PortableServer_POAManager poa_manager;
CORBA_ORB orb;

typedef struct {
	POA_GNOME_FileManagerWindow servant;
	PortableServer_POA poa;
	
	WPanel *mywin;
} impl_POA_GNOME_FileManagerWindow;

typedef struct {
	POA_GNOME_FileManagerFactory servant;
	PortableServer_POA poa;
} impl_POA_GNOME_FileManagerFactory;

/*** Implementation stub prototypes ***/

static void
impl_GNOME_FileManagerWindow_close(impl_POA_GNOME_FileManagerWindow * servant,
				   CORBA_Environment * ev);

GNOME_FileManagerWindow
impl_GNOME_FileManagerFactory_create_window(impl_POA_GNOME_FileManagerFactory * servant,
					    CORBA_char * dir,
					    CORBA_Environment * ev);

CORBA_boolean
impl_GNOME_FileManagerFactory_supports(impl_POA_GNOME_FileManagerFactory * servant,
				       CORBA_char * obj_goad_id,
				       CORBA_Environment * ev);
CORBA_Object
impl_GNOME_FileManagerFactory_create_object(impl_POA_GNOME_FileManagerFactory * servant,
					    CORBA_char * goad_id,
					    GNOME_stringlist * params,
					    CORBA_Environment * ev);

/*** epv structures ***/

static PortableServer_ServantBase__epv impl_GNOME_FileManagerWindow_base_epv =
{
   NULL, NULL, NULL
};
static POA_GNOME_FileManagerWindow__epv impl_GNOME_FileManagerWindow_epv =
{
   NULL,			/* _private */
   (gpointer) & impl_GNOME_FileManagerWindow_close,
};
static PortableServer_ServantBase__epv impl_GNOME_FileManagerFactory_base_epv =
{
   NULL,			/* _private data */
   NULL,
   NULL			/* default_POA routine */
};
static POA_GNOME_FileManagerFactory__epv impl_GNOME_FileManagerFactory_epv =
{
   NULL,			/* _private */
   (gpointer) & impl_GNOME_FileManagerFactory_create_window,
};

static POA_GNOME_GenericFactory__epv impl_GNOME_FileManagerFactory_GNOME_GenericFactory_epv =
{
   NULL,			/* _private */
   (gpointer) & impl_GNOME_FileManagerFactory_supports,
   (gpointer) & impl_GNOME_FileManagerFactory_create_object,
};

/*** vepv structures ***/

static POA_GNOME_FileManagerWindow__vepv impl_GNOME_FileManagerWindow_vepv =
{
   &impl_GNOME_FileManagerWindow_base_epv,
   &impl_GNOME_FileManagerWindow_epv,
};
static POA_GNOME_FileManagerFactory__vepv impl_GNOME_FileManagerFactory_vepv =
{
   &impl_GNOME_FileManagerFactory_base_epv,
   &impl_GNOME_FileManagerFactory_GNOME_GenericFactory_epv,
   &impl_GNOME_FileManagerFactory_epv,
};

impl_POA_GNOME_FileManagerFactory poa_filemanagerfactory_servant = {
	{ NULL, &impl_GNOME_FileManagerWindow_vepv}, NULL
};

GNOME_FileManagerFactory filemanagerfactory_server;

/*** Stub implementations ***/

static void
do_window_close(GtkWidget *widget, gpointer _servant)
{
	CORBA_Environment ev;
	PortableServer_ObjectId *objid;
	impl_POA_GNOME_FileManagerWindow *servant = _servant;
	
	CORBA_exception_init(&ev);
	objid = PortableServer_POA_servant_to_id (servant->poa, _servant, &ev);
	PortableServer_POA_deactivate_object (servant->poa, objid, &ev);
	CORBA_free(objid);
	POA_GNOME_FileManagerWindow__fini(_servant, &ev);
	
	g_free(servant);
	CORBA_exception_free(&ev);
}

static void
impl_GNOME_FileManagerWindow_close(impl_POA_GNOME_FileManagerWindow * servant, CORBA_Environment * ev)
{
	gtk_widget_destroy(servant->mywin->xwindow);
}

GNOME_FileManagerWindow
impl_GNOME_FileManagerFactory_create_window(impl_POA_GNOME_FileManagerFactory * servant,
					    CORBA_char * dir,
					    CORBA_Environment * ev)
{
	GNOME_FileManagerWindow retval;
	impl_POA_GNOME_FileManagerWindow *newservant;
	PortableServer_ObjectId *objid;
	
	newservant = g_new0(impl_POA_GNOME_FileManagerWindow, 1);
	newservant->servant.vepv = &impl_GNOME_FileManagerWindow_vepv;
	newservant->poa = poa;
	newservant->mywin = new_panel_at ((dir && *dir) ? dir : home_dir);
	
	gtk_signal_connect(GTK_OBJECT(newservant->mywin->xwindow),
			   "destroy", impl_GNOME_FileManagerWindow_close,
			   newservant);
	
	POA_GNOME_FileManagerWindow__init((PortableServer_Servant) newservant, ev);
	objid = PortableServer_POA_activate_object(poa, newservant, ev);
	CORBA_free(objid);
	retval = PortableServer_POA_servant_to_reference(poa, newservant, ev);
	
	return retval;
}

CORBA_boolean
impl_GNOME_FileManagerFactory_supports(impl_POA_GNOME_FileManagerFactory * servant,
				       CORBA_char * obj_goad_id,
				       CORBA_Environment * ev)
{
	return !strcmp(obj_goad_id, "gmc_filemanager_window");
}

CORBA_Object
impl_GNOME_FileManagerFactory_create_object(impl_POA_GNOME_FileManagerFactory * servant,
					    CORBA_char * goad_id,
					    GNOME_stringlist * params,
					    CORBA_Environment * ev)
{
	if(!strcmp(goad_id, "gmc_filemanager_window"))
		return impl_GNOME_FileManagerFactory_create_window(
			servant,
			params->_length ? params->_buffer [0] : home_dir, ev);
	
	CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
			     ex_GNOME_GenericFactory_CannotActivate,
			     NULL);
	
	return CORBA_OBJECT_NIL;
}

void
corba_register_server (void)
{
	int v;
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);

	orb = gnome_CORBA_ORB();

	poa = CORBA_ORB_resolve_initial_references (orb, "RootPOA", &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		printf ("Can not resolve initial reference to RootPOA");
		return;
	}

	poa_manager = PortableServer_POA__get_the_POAManager (poa, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		printf ("Can not get the POAmanager");
		return;
	}
	
	PortableServer_POAManager_activate (poa_manager, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		printf ("Can not get the POAmanager");
		return;
	}

	/*
	 * Initialize the Factory Object
	 */
	POA_GNOME_FileManagerFactory__init (
		(POA_GNOME_FileManagerFactory*)&poa_filemanagerfactory_servant,
		&ev);
	
	if (ev._major != CORBA_NO_EXCEPTION){
		printf ("Can not initialize FileManagerFactory object\n");
		return;
	}

	/* Activate the object */
	CORBA_free (PortableServer_POA_activate_object
		    (poa, &poa_filemanagerfactory_servant, &ev));

	/* Get a refeerence to te object */
	filemanagerfactory_server = PortableServer_POA_servant_to_reference (
		poa, &poa_filemanagerfactory_servant, &ev);
	
	v = goad_server_register (
		NULL, filemanagerfactory_server,
		"gmc_FileManagerFactory", "server", &ev);

	if (v != 0)
		return;
	
}

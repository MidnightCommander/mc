/*
 * gcorba.c:  CORBA support code for the GNOME Midnight Commander
 *
 * Author:
 *    Miguel de Icaza (miguel@gnu.org)
 */
#include <config.h>
#include <libgnorba/gnorba.h>
#include "gcorba.h"

CORBA_ORB               orb;
PortaPortableServer_POA poa;
CORBA_Object            *name_server;
CORBA_Environment       ev;

/* The object we define: */
GNOME_FileManagerFactory filemanagerfactory_server;

PortableServer_ServantBase__epv base_epv = {
	NULL,
	NULL,
	NULL
};

/* GNOME::Factory implementation */
static CORBA_Object
FileManager_Factory_new (PortableServer_Servant servant, CORBA_Environment *ev)
{
}

static CORBA_Object
FileManager_Factory_new_args (PortableServer_Servant servant,
			      CORBA_char *argument,
			      CORBA_Environment *ev)
{
	return FileManager_Factory_new (servant, ev);
}

FileManager_Factory_new_args
POA_GNOME_Factory__epv poa_gnome_factory__epv = {
	FileManager_Factory_new,
	FileManager_Factory_new_args
	
};

POA_FileManager_Factory__vepv poa_filemanager_factory_vepv = {
	&base_epv,
	&poa_gnome_factory_epv,
	NULL			/* POA_FileManager_Factory_epv */
};

POA_FileManager_Factory poa_filemanagerfactory_servant = {
	NULL,
	&poa_filemanager_factory_vepv,
};

#define FACTORY_ID "IDL:GNOME/FileManagerFactory:1.0"
PortableServer_ObjectId factory_id = { 0, sizeof (FACTORY_ID), FACTORY_ID };

void
corba_register_server (void)
{
	PortableServer_POAManager poa_manager;
	int v;
	
	CORBA_exception_init (&ev);

	poa = CORBA_ORB_resolve_initial_references (orb, "RootPOA", &ev);
	if (ev.major != CORBA_NO_EXCEPTION){
		printf ("Can not resolve initial reference to RootPOA");
		return;
	}

	poa_manager = PortableServer_POA__get_the_POAManager (poa, &ev);
	if (ev.major != CORBA_NO_EXCEPTION){
		printf ("Can not get the POAmanager");
		return;
	}
	
	PortableServer_POAManager_activate (poa_manager, &ev);
	if (ev.major != CORBA_NO_EXCEPTION){
		printf ("Can not get the POAmanager");
		return;
	}

	/*
	 * Initialize the Factory Object
	 */
	POA_GNOME_FileManagerFactory__init (&poa_filemanagerfactory_servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		printf ("Can not initialize FileManagerFactory object\n");
		return;
	}

	/* Activate the object */
	PortableServer_POA_activate_object_with_id (
		poa, &factory_id, &poa_filemanagerfactory_servant, &ev);

	/* Get a refeerence to te object */
	filemanagerfactory_server = PortableServer_POA_servant_to_reference (
		poa, &poa_filemanagerfactory_servant, &ev);
	
	/*
	 * Register the FileManagerFactory with GOAD.
	 */
	name_server = gnome_name_service_get ();

	
	if (!name_server){
		printf ("Can not talk to the name server\n");
		return;
	}

	v = goad_server_register (
		name_server, file_manager_factory,
		"FileManagerFactory", "FileManager:1.0", &ev);

	if (v != 0)
		return;
	
}


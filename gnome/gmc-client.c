/* Simple command-line client for the GNOME edition of the Midnight Commander
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include <libgnorba/gnorba.h>
#include "FileManager.h"



/* Tries to contact the window factory */
static CORBA_Object
get_window_factory (void)
{
	CORBA_Object obj;

	obj = goad_server_activate_with_id (
		NULL,
		"IDL:GNOME:FileManager:WindowFactory:1.0",
		GOAD_ACTIVATE_EXISTING_ONLY,
		NULL);
	if (obj == CORBA_OBJECT_NIL)
		fprintf (stderr, _("Could not contact the file manager\n"));

	return obj;
}

static CORBA_Object
get_desktop (void)
{
	CORBA_Environment ev;
	CORBA_Object wf;
	CORBA_Object obj;

	wf = get_window_factory ();
	if (wf == CORBA_OBJECT_NIL)
		return CORBA_OBJECT_NIL;

	CORBA_exception_init (&ev);
	obj = GNOME_FileManager_WindowFactory__get_the_desktop (wf, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		fprintf (stderr, _("Could not get the desktop\n"));
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return obj;
}

/* Creates a new window in the specified directory */
static int
create_window (char *dir, CORBA_Environment *ev)
{
	CORBA_Object obj;

	obj = get_window_factory ();
	if (obj == CORBA_OBJECT_NIL)
		return FALSE;

	g_assert (dir != NULL);

	/* FIXME: need to canonicalize non-absolute pathnames */

	GNOME_FileManager_WindowFactory_create_window (obj, dir, ev);
	CORBA_Object_release (obj, ev);
	return TRUE;
}

/* Rescan the specified directory */
static int
rescan_directory (char *dir, CORBA_Environment *ev)
{
	CORBA_Object obj;

	obj = get_window_factory ();
	if (obj == CORBA_OBJECT_NIL)
		return FALSE;

	g_assert (dir != NULL);

	/* FIXME: need to canonicalize non-absolute pathnames */

	GNOME_FileManager_WindowFactory_rescan_directory (obj, dir, ev);
	CORBA_Object_release (obj, ev);
	return TRUE;
}

/* Rescan the desktop icons */
static int
rescan_desktop (CORBA_Environment *ev)
{
	CORBA_Object obj;

	obj = get_desktop ();
	if (obj == CORBA_OBJECT_NIL)
		return FALSE;

	GNOME_FileManager_Desktop_rescan (obj, ev);
	CORBA_Object_release (obj, ev);
	return TRUE;
}

/* Rescan the desktop device icons */
static int
rescan_desktop_devices (CORBA_Environment *ev)
{
	CORBA_Object obj;

	obj = get_desktop ();
	if (obj == CORBA_OBJECT_NIL)
		return FALSE;

	GNOME_FileManager_Desktop_rescan_devices (obj, ev);
	CORBA_Object_release (obj, ev);
	return TRUE;
}

/* Close invalid windows */
static int
close_invalid_windows (CORBA_Environment *ev)
{
	CORBA_Object obj;

	obj = get_window_factory ();
	if (obj == CORBA_OBJECT_NIL)
		return FALSE;

	GNOME_FileManager_WindowFactory_close_invalid_windows (obj, ev);
	CORBA_Object_release (obj, ev);
	return TRUE;
}



/* Option arguments */

enum {
	ARG_CREATE_WINDOW,
	ARG_RESCAN_DIRECTORY,
	ARG_RESCAN_DESKTOP,
	ARG_RESCAN_DESKTOP_DEVICES,
	ARG_CLOSE_INVALID_WINDOWS
};

static int selected_option = -1;
static char *directory;

/* Parse an argument */
static void
parse_an_arg (poptContext ctx,
	      enum poptCallbackReason reason,
	      const struct poptOption *opt,
	      char *arg,
	      void *data)
{
	if (reason == POPT_CALLBACK_REASON_OPTION)
		selected_option = opt->val;
	else if (reason == POPT_CALLBACK_REASON_POST && selected_option == -1) {
		poptPrintUsage (ctx, stderr, 0);
		exit (1);
	}
}

static const struct poptOption options[] = {
	{ NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_POST, parse_an_arg, 0, NULL, NULL },
	{ "create-window", '\0', POPT_ARG_STRING, &directory, ARG_CREATE_WINDOW,
	  N_("Create window showing the specified directory"), N_("DIRECTORY") },
	{ "rescan-directory", '\0', POPT_ARG_STRING, &directory, ARG_RESCAN_DIRECTORY,
	  N_("Rescan the specified directory"), N_("DIRECTORY") },
	{ "rescan-desktop", '\0', POPT_ARG_NONE, NULL, ARG_RESCAN_DESKTOP,
	  N_("Rescan the desktop icons"), NULL },
	{ "rescan-desktop-devices", '\0', POPT_ARG_NONE, NULL, ARG_RESCAN_DESKTOP_DEVICES,
	  N_("Rescan the desktop device icons"), NULL },
	{ "close-invalid-windows", '\0', POPT_ARG_NONE, NULL, ARG_CLOSE_INVALID_WINDOWS,
	  N_("Close windows whose directories cannot be reached"), NULL },
	{ NULL, '\0', 0, NULL, 0, NULL, NULL }
};



int
main (int argc, char **argv)
{
	poptContext ctx;
	CORBA_Environment ev;
	CORBA_ORB orb;
	int result;

	bindtextdomain ("gmc-client", LOCALEDIR);
	textdomain ("gmc-client");

	CORBA_exception_init (&ev);
	orb = gnome_CORBA_init_with_popt_table ("gmc-client", VERSION,
						&argc, argv, options, 0, &ctx,
						0, &ev);

	switch (selected_option) {
	case ARG_CREATE_WINDOW:
		result = create_window (directory, &ev);
		break;

	case ARG_RESCAN_DIRECTORY:
		result = rescan_directory (directory, &ev);
		break;

	case ARG_RESCAN_DESKTOP:
		result = rescan_desktop (&ev);
		break;

	case ARG_RESCAN_DESKTOP_DEVICES:
		result = rescan_desktop_devices (&ev);
		break;

	case ARG_CLOSE_INVALID_WINDOWS:
		result = close_invalid_windows (&ev);
		break;

	default:
		g_assert_not_reached ();
	}

	if (!result || ev._major != CORBA_NO_EXCEPTION) {
		poptFreeContext (ctx);
		exit (1);
	}

	CORBA_exception_free (&ev);
	poptFreeContext (ctx);
	return 0;
}

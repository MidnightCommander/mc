/* Toplevel file window for the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef GMC_WINDOW_H
#define GMC_WINDOW_H

#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-app.h>

BEGIN_GNOME_DECLS


typedef struct _GmcWindow Gmcwindow;
typedef struct _GmcWindowClass GmcWindowClass;

struct _GmcWindow {
	GnomeApp app;

	GtkWidget *paned;	/* Paned container to split into tree/list views */
	GtkWidget *tree;	/* Tree view */
	GtkWidget *notebook;	/* Notebook to switch between list and icon views */
	GtkWidget *clist;	/* List view (column list) */
	GtkWidget *ilist;	/* Icon view (icon list) */
};

struct _GmcWindowClass {
	GnomeAppClass parent_class;
};


END_GNOME_DECLS

#endif

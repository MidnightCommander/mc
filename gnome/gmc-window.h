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


/* File listing modes */
typedef enum {
	FILE_LIST_LIST,
	FILE_LIST_ICONS
} FileListType;


typedef struct _GmcWindow GmcWindow;
typedef struct _GmcWindowClass GmcWindowClass;

struct _GmcWindow {
	GnomeApp app;

	GtkWidget *paned;	/* Paned container to split into tree/list views */
	GtkWidget *tree;	/* Tree view */
	GtkWidget *notebook;	/* Notebook to switch between list and icon views */
	GtkWidget *clist_sw;	/* Scrolled window for the clist */
	GtkWidget *clist;	/* List view (column list) */
	GtkWidget *ilist_sw;	/* Scrolled window for the icon list */
	GtkWidget *ilist;	/* Icon view (icon list) */

	FileListType list_type;	/* Current file listing type */
};

struct _GmcWindowClass {
	GnomeAppClass parent_class;
};


/* Standard Gtk function */
GtkType gmc_window_get_type (void);

/* Creates a new GMC window */
GtkWidget *gmc_window_new (void);


END_GNOME_DECLS

#endif

/* Drag and Drop functionality for the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#ifndef GDND_H
#define GDND_H

#include <gtk/gtk.h>


/* Standard DnD types */
typedef enum {
	TARGET_MC_DESKTOP_ICON,
	TARGET_URI_LIST,
	TARGET_TEXT_PLAIN,
	TARGET_URL,
	TARGET_NTARGETS
} TargetType;

/* DnD target names */
#define TARGET_MC_DESKTOP_ICON_TYPE	"application/x-mc-desktop-icon"
#define TARGET_URI_LIST_TYPE 		"text/uri-list"
#define TARGET_TEXT_PLAIN_TYPE 		"text/plain"
#define TARGET_URL_TYPE			"_NETSCAPE_URL"

/* Atoms for the DnD types, indexed per the enum above */
extern GdkAtom dnd_target_atoms[];


/* Initializes drag and drop by interning the target convenience atoms */
void gdnd_init (void);

/* Drop the list of URIs in the selection data to the specified directory */
int gdnd_drop_on_directory (GdkDragContext *context, GtkSelectionData *selection_data, char *dirname);

/* Test whether the specified context has a certain target type */
int gdnd_drag_context_has_target (GdkDragContext *context, TargetType type);

#endif

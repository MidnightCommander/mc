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
enum {
	TARGET_MC_DESKTOP_ICON,
	TARGET_URI_LIST,
	TARGET_TEXT_PLAIN,
	TARGET_URL
};


/* Drop the list of URIs in the selection data to the specified directory */
int gdnd_drop_on_directory (GdkDragContext *context, GtkSelectionData *selection_data, char *dirname);


#endif

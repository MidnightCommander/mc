/* GNU Midnight Commander -- GNOME edition
 *
 * Properties dialog for files and desktop icons.
 *
 * Copyright (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 */

#ifndef _GPAGEPROP_H
#define _GPAGEPROP_H


#include <gnome.h>
#include "gdesktop.h"


typedef enum {
	GPROP_FILENAME = 1 << 0,
	GPROP_MODE     = 1 << 1,
	GPROP_UID      = 1 << 2,
	GPROP_GID      = 1 << 3,
	GPROP_ICON     = 1 << 4,
	GPROP_TITLE    = 1 << 5
} GpropChanged;

/* Returns a mask of the above specifying what changed.
 */

int item_properties (GtkWidget *parent, char *fname, desktop_icon_info *di);


#endif

/* Icon loading support for the Midnight Commander
 *
 * Copyright (C) 1998-1999 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@nuclecu.unam.mx>
 *          Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef GNOME_GICON_H
#define GNOME_GICON_H

#include <glib.h>
#include <gdk_imlib.h>
#include "dir.h"

/* Standard icon sizes */
#define ICON_IMAGE_WIDTH 48
#define ICON_IMAGE_HEIGHT 48

void gicon_init (void);

GdkImlibImage *gicon_get_icon_for_file (char *directory, file_entry *fe, gboolean do_quick);
const char *gicon_get_filename_for_icon (GdkImlibImage *image);


#endif

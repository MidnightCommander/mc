/* Desktop preferences box for the Midnight Commander
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef GDESKTOP_PREFS_H
#define GDESKTOP_PREFS_H

#include <libgnomeui/gnome-propertybox.h>

typedef struct _GDesktopPrefs GDesktopPrefs;

GDesktopPrefs *desktop_prefs_new (GnomePropertyBox *pbox);
void desktop_prefs_apply (GDesktopPrefs *dp);

#endif

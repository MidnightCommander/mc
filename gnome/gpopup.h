/* Popup menus for the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#ifndef GPOPUP_H
#define GPOPUP_H


#include <gdk/gdktypes.h>
#include "panel.h"
#include "gdesktop.h"

int gpopup_do_popup2 (GdkEventButton *event, WPanel *panel, DesktopIconInfo *dii);

#if 0
int gpopup_do_popup (GdkEventButton *event, WPanel *from_panel,
		     DesktopIconInfo *dii,
		     int panel_row, char *filename);
#endif


#endif

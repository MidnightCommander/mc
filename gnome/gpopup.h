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

int gpopup_do_popup (GdkEventButton *event, WPanel *from_panel,
		     desktop_icon_info *dii,
		     int panel_row, char *filename);


#endif

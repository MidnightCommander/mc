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


void file_popup (GdkEventButton *event, void *WPanel_pointer, void *desktop_icon_t_pointer, int row, char *filename);

int gpopup_do_popup (char *filename, WPanel *from_panel, GdkEventButton *event);


#endif

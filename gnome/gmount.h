/* Mount/umount support for the Midnight Commander
 *
 * Copyright (C) 1998-1999 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@nuclecu.unam.mx>
 *          Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef GMOUNT_H
#define GMOUNT_H

#include <glib.h>

void gmount_setup_devices (int cleanup);

#if 0
gboolean  is_block_device_mountable (char *devname);
gboolean  is_block_device_mounted   (char *devname);
#endif

#endif

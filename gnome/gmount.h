/* Mount/umount support for the Midnight Commander
 *
 * Copyright (C) 1998-1999 The Free Software Foundation
 *
 * Author: Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#ifndef GMOUNT_H
#define GMOUNT_H

#include <glib.h>

gboolean  is_block_device_mountable (char *devname);
gboolean  is_block_device_mounted   (char *devname);
GList    *get_list_of_mountable_devices (void);

#endif

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

void gmount_setup_devices (void);
void desktop_cleanup_devices (void);

char     *is_block_device_mountable (char *mount_point);
gboolean  is_block_device_mounted   (char *mount_point);
char     *mount_point_to_device     (char *mount_point);

#endif

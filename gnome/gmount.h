#ifndef GMOUNT_H
#define GMOUNT_H

gboolean  is_block_device_mountable (char *devname);
gboolean  is_block_device_mounted   (char *devname);
GList    *get_list_of_mountable_devices (void);

#endif

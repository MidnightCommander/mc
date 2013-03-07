/**
 * \file
 * \brief Header: SFTP FS
 */

#ifndef MC__VFS_SMBFS_INTERNAL_H
#define MC__VFS_SMBFS_INTERNAL_H

#include <libsmbclient.h>

#include "lib/vfs/vfs.h"
#include "lib/vfs/xdirentry.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

extern struct vfs_class smbfs_class;
extern struct vfs_s_subclass smbfs_subclass;

/*** declarations of public functions ************************************************************/

void smbfs_init_class (void);
void smbfs_init_subclass (void);
void smbfs_init_class_callbacks (void);
void smbfs_init_subclass_callbacks (void);

void
smbfs_cb_authdata_provider (const char *server, const char *share,
                            char *workgroup, int wgmaxlen, char *username, int unmaxlen,
                            char *password, int pwmaxlen);

/*** inline functions ****************************************************************************/

#endif /* MC__VFS_SMBFS_INTERNAL_H */

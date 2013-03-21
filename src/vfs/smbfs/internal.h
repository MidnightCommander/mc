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

typedef struct
{
    int socket_handle;
} smbfs_super_data_t;

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
const char *smbfs_strerror (int err_no);

char *smbfs_make_url (const vfs_path_element_t * element, gboolean with_path);

void *smbfs_opendir (const vfs_path_t * vpath, GError ** error);
void *smbfs_readdir (void *data, GError ** error);
int smbfs_closedir (void *data, GError ** error);
int smbfs_mkdir (const vfs_path_t * vpath, mode_t mode, GError ** error);
int smbfs_rmdir (const vfs_path_t * vpath, GError ** error);

int smbfs_lstat (const vfs_path_t * vpath, struct stat *buf, GError ** error);
int smbfs_stat (const vfs_path_t * vpath, struct stat *buf, GError ** error);


/*** inline functions ****************************************************************************/

#endif /* MC__VFS_SMBFS_INTERNAL_H */

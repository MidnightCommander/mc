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
    char *username;
    char *password;
    char *workgroup;
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
void
smbfs_auth_dialog (const char *server, const char *share,
                   char **workgroup, char **username, char **password);

const char *smbfs_strerror (int err_no);

char *smbfs_make_url (const vfs_path_element_t * element, gboolean with_path);

void *smbfs_dir_open (const vfs_path_t * vpath, GError ** error);
void *smbfs_dir_read (void *data, GError ** error);
int smbfs_dir_close (void *data, GError ** error);
int smbfs_dir_make (const vfs_path_t * vpath, mode_t mode, GError ** error);
int smbfs_dir_remove (const vfs_path_t * vpath, GError ** error);

int smbfs_lstat (const vfs_path_t * vpath, struct stat *buf, GError ** error);
int smbfs_stat (const vfs_path_t * vpath, struct stat *buf, GError ** error);

void smbfs_assign_value_if_not_null (char *value, char **assignee);

gboolean smbfs_file_open (vfs_file_handler_t * file_handler, const vfs_path_t * vpath, int flags,
                          mode_t mode, GError ** error);
int smbfs_file_stat (vfs_file_handler_t * data, struct stat *buf, GError ** error);
off_t smbfs_file_lseek (vfs_file_handler_t * file_handler, off_t offset, int whence,
                        GError ** error);
ssize_t smbfs_file_read (vfs_file_handler_t * file_handler, char *buffer, size_t count,
                         GError ** error);
ssize_t smbfs_file_write (vfs_file_handler_t * file_handler, const char *buffer, size_t count,
                          GError ** error);
int smbfs_file_close (vfs_file_handler_t * file_handler, GError ** error);

int smbfs_attr_chmod (const vfs_path_t * vpath, mode_t mode, GError ** error);

int smbfs_unlink (const vfs_path_t * vpath, GError ** error);
int smbfs_rename (const vfs_path_t * vpath1, const vfs_path_t * vpath2, GError ** error);

int smbfs_file_change_modification_time (const vfs_path_t * vpath, struct utimbuf *tbuf,
                                         GError ** error);

/*** inline functions ****************************************************************************/

#endif /* MC__VFS_SMBFS_INTERNAL_H */

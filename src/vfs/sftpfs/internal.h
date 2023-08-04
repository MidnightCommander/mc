/**
 * \file
 * \brief Header: SFTP FS
 */

#ifndef MC__VFS_SFTPFS_INTERNAL_H
#define MC__VFS_SFTPFS_INTERNAL_H

#include <libssh2.h>
#include <libssh2_sftp.h>

#include "lib/vfs/vfs.h"
#include "lib/vfs/xdirentry.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define SFTP_DEFAULT_PORT 22

/* LIBSSH2_INVALID_SOCKET is defined in libssh2 >= 1.4.1 */
#ifndef LIBSSH2_INVALID_SOCKET
#define LIBSSH2_INVALID_SOCKET -1
#endif

#define SFTP_SUPER(super) ((sftpfs_super_t *) (super))

/*** enums ***************************************************************************************/

typedef enum
{
    NONE = 0,
    PUBKEY = (1 << 0),
    PASSWORD = (1 << 1),
    AGENT = (1 << 2)
} sftpfs_auth_type_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    struct vfs_s_super base;

    sftpfs_auth_type_t auth_type;
    sftpfs_auth_type_t config_auth_type;

    LIBSSH2_KNOWNHOSTS *known_hosts;
    char *known_hosts_file;

    LIBSSH2_SESSION *session;
    LIBSSH2_SFTP *sftp_session;

    LIBSSH2_AGENT *agent;

    char *pubkey;
    char *privkey;

    int socket_handle;
    const char *ip_address;
    vfs_path_element_t *original_connection_info;
} sftpfs_super_t;

/*** global variables defined in .c file *********************************************************/

extern GString *sftpfs_filename_buffer;
extern struct vfs_s_subclass sftpfs_subclass;
extern struct vfs_class *vfs_sftpfs_ops;

/*** declarations of public functions ************************************************************/

void sftpfs_init_config_variables_patterns (void);
void sftpfs_deinit_config_variables_patterns (void);

gboolean sftpfs_is_sftp_error (LIBSSH2_SFTP * sftp_session, int sftp_res, int sftp_error);
void sftpfs_ssherror_to_gliberror (sftpfs_super_t * super, int libssh_errno, GError ** mcerror);
gboolean sftpfs_waitsocket (sftpfs_super_t * super, int sftp_res, GError ** mcerror);

const GString *sftpfs_fix_filename (const char *file_name);

gboolean sftpfs_op_init (sftpfs_super_t ** super, const vfs_path_element_t ** path_element,
                         const vfs_path_t * vpath, GError ** mcerror);

void sftpfs_attr_to_stat (const LIBSSH2_SFTP_ATTRIBUTES * attrs, struct stat *s);
int sftpfs_lstat (const vfs_path_t * vpath, struct stat *buf, GError ** mcerror);
int sftpfs_stat (const vfs_path_t * vpath, struct stat *buf, GError ** mcerror);
int sftpfs_readlink (const vfs_path_t * vpath, char *buf, size_t size, GError ** mcerror);
int sftpfs_symlink (const vfs_path_t * vpath1, const vfs_path_t * vpath2, GError ** mcerror);
int sftpfs_utime (const vfs_path_t * vpath, time_t atime, time_t mtime, GError ** mcerror);
int sftpfs_chmod (const vfs_path_t * vpath, mode_t mode, GError ** mcerror);
int sftpfs_unlink (const vfs_path_t * vpath, GError ** mcerror);
int sftpfs_rename (const vfs_path_t * vpath1, const vfs_path_t * vpath2, GError ** mcerror);

void sftpfs_fill_connection_data_from_config (struct vfs_s_super *super, GError ** mcerror);
int sftpfs_open_connection (struct vfs_s_super *super, GError ** mcerror);
void sftpfs_close_connection (struct vfs_s_super *super, const char *shutdown_message,
                              GError ** mcerror);

vfs_file_handler_t *sftpfs_fh_new (struct vfs_s_inode *ino, gboolean changed);

void *sftpfs_opendir (const vfs_path_t * vpath, GError ** mcerror);
struct vfs_dirent *sftpfs_readdir (void *data, GError ** mcerror);
int sftpfs_closedir (void *data, GError ** mcerror);
int sftpfs_mkdir (const vfs_path_t * vpath, mode_t mode, GError ** mcerror);
int sftpfs_rmdir (const vfs_path_t * vpath, GError ** mcerror);

gboolean sftpfs_open_file (vfs_file_handler_t * fh, int flags, mode_t mode, GError ** mcerror);
ssize_t sftpfs_read_file (vfs_file_handler_t * fh, char *buffer, size_t count, GError ** mcerror);
ssize_t sftpfs_write_file (vfs_file_handler_t * fh, const char *buffer, size_t count,
                           GError ** mcerror);
int sftpfs_close_file (vfs_file_handler_t * fh, GError ** mcerror);
int sftpfs_fstat (void *data, struct stat *buf, GError ** mcerror);
off_t sftpfs_lseek (vfs_file_handler_t * fh, off_t offset, int whence, GError ** mcerror);

/*** inline functions ****************************************************************************/

#endif /* MC__VFS_SFTPFS_INTERNAL_H */

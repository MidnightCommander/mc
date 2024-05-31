/* Virtual File System: SFTP file system.
   The interface function

   Copyright (C) 2011-2024
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2011
   Slava Zanko <slavazanko@gmail.com>, 2011-2013
   Andrew Borodin <aborodin@vmail.ru>, 2021-2022

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>             /* memset() */

#include "lib/global.h"
#include "lib/vfs/netutil.h"
#include "lib/vfs/utilvfs.h"
#include "lib/vfs/gc.h"
#include "lib/widget.h"
#include "lib/tty/tty.h"        /* tty_enable_interrupt_key () */

#include "internal.h"

#include "sftpfs.h"

/*** global variables ****************************************************************************/

struct vfs_s_subclass sftpfs_subclass;
struct vfs_class *vfs_sftpfs_ops = VFS_CLASS (&sftpfs_subclass);        /* used in file.c */

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for VFS-class init action.
 *
 * @param me structure of VFS class
 */

static int
sftpfs_cb_init (struct vfs_class *me)
{
    (void) me;

    if (libssh2_init (0) != 0)
        return 0;

    sftpfs_filename_buffer = g_string_new ("");
    sftpfs_init_config_variables_patterns ();
    return 1;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for VFS-class deinit action.
 *
 * @param me structure of VFS class
 */

static void
sftpfs_cb_done (struct vfs_class *me)
{
    (void) me;

    sftpfs_deinit_config_variables_patterns ();
    g_string_free (sftpfs_filename_buffer, TRUE);
    libssh2_exit ();
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for opening file.
 *
 * @param vpath path to file
 * @param flags flags (see man 2 open)
 * @param mode  mode (see man 2 open)
 * @return file data handler if success, NULL otherwise
 */

static void *
sftpfs_cb_open (const vfs_path_t *vpath, int flags, mode_t mode)
{
    vfs_file_handler_t *fh;
    struct vfs_class *me;
    struct vfs_s_super *super;
    const char *path_super;
    struct vfs_s_inode *path_inode;
    GError *mcerror = NULL;
    gboolean is_changed = FALSE;

    path_super = vfs_s_get_path (vpath, &super, 0);
    if (path_super == NULL)
        return NULL;

    me = VFS_CLASS (vfs_path_get_last_path_vfs (vpath));

    path_inode = vfs_s_find_inode (me, super, path_super, LINK_FOLLOW, FL_NONE);
    if (path_inode != NULL && ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)))
    {
        me->verrno = EEXIST;
        return NULL;
    }

    if (path_inode == NULL)
    {
        char *name;
        struct vfs_s_entry *ent;
        struct vfs_s_inode *dir;

        name = g_path_get_dirname (path_super);
        dir = vfs_s_find_inode (me, super, name, LINK_FOLLOW, FL_DIR);
        g_free (name);
        if (dir == NULL)
            return NULL;

        name = g_path_get_basename (path_super);
        ent = vfs_s_generate_entry (me, name, dir, 0755);
        g_free (name);
        path_inode = ent->ino;
        vfs_s_insert_entry (me, dir, ent);
        is_changed = TRUE;
    }

    if (S_ISDIR (path_inode->st.st_mode))
    {
        me->verrno = EISDIR;
        return NULL;
    }

    fh = sftpfs_fh_new (path_inode, is_changed);

    if (!sftpfs_open_file (fh, flags, mode, &mcerror))
    {
        mc_error_message (&mcerror, NULL);
        g_free (fh);
        return NULL;
    }

    vfs_rmstamp (me, (vfsid) super);
    super->fd_usage++;
    fh->ino->st.st_nlink++;
    return fh;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for opening directory.
 *
 * @param vpath path to directory
 * @return directory data handler if success, NULL otherwise
 */

static void *
sftpfs_cb_opendir (const vfs_path_t *vpath)
{
    GError *mcerror = NULL;
    void *ret_value;

    /* reset interrupt flag */
    tty_got_interrupt ();

    ret_value = sftpfs_opendir (vpath, &mcerror);
    mc_error_message (&mcerror, NULL);
    return ret_value;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for reading directory entry.
 *
 * @param data directory data handler
 * @return information about direntry if success, NULL otherwise
 */

static struct vfs_dirent *
sftpfs_cb_readdir (void *data)
{
    GError *mcerror = NULL;
    struct vfs_dirent *sftpfs_dirent;

    if (tty_got_interrupt ())
    {
        tty_disable_interrupt_key ();
        return NULL;
    }

    sftpfs_dirent = sftpfs_readdir (data, &mcerror);
    if (!mc_error_message (&mcerror, NULL))
    {
        if (sftpfs_dirent != NULL)
            vfs_print_message (_("sftp: (Ctrl-G break) Listing... %s"), sftpfs_dirent->d_name);
        else
            vfs_print_message ("%s", _("sftp: Listing done."));
    }

    return sftpfs_dirent;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for closing directory.
 *
 * @param data directory data handler
 * @return 0 if success, negative value otherwise
 */

static int
sftpfs_cb_closedir (void *data)
{
    int rc;
    GError *mcerror = NULL;

    rc = sftpfs_closedir (data, &mcerror);
    mc_error_message (&mcerror, NULL);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for lstat VFS-function.
 *
 * @param vpath path to file or directory
 * @param buf   buffer for store stat-info
 * @return 0 if success, negative value otherwise
 */

static int
sftpfs_cb_lstat (const vfs_path_t *vpath, struct stat *buf)
{
    int rc;
    GError *mcerror = NULL;

    rc = sftpfs_lstat (vpath, buf, &mcerror);
    mc_error_message (&mcerror, NULL);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for stat VFS-function.
 *
 * @param vpath path to file or directory
 * @param buf   buffer for store stat-info
 * @return 0 if success, negative value otherwise
 */

static int
sftpfs_cb_stat (const vfs_path_t *vpath, struct stat *buf)
{
    int rc;
    GError *mcerror = NULL;

    rc = sftpfs_stat (vpath, buf, &mcerror);
    mc_error_message (&mcerror, NULL);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for fstat VFS-function.
 *
 * @param data file data handler
 * @param buf  buffer for store stat-info
 * @return 0 if success, negative value otherwise
 */

static int
sftpfs_cb_fstat (void *data, struct stat *buf)
{
    int rc;
    GError *mcerror = NULL;

    rc = sftpfs_fstat (data, buf, &mcerror);
    mc_error_message (&mcerror, NULL);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for readlink VFS-function.
 *
 * @param vpath path to file or directory
 * @param buf   buffer for store stat-info
 * @param size  buffer size
 * @return 0 if success, negative value otherwise
 */

static int
sftpfs_cb_readlink (const vfs_path_t *vpath, char *buf, size_t size)
{
    int rc;
    GError *mcerror = NULL;

    rc = sftpfs_readlink (vpath, buf, size, &mcerror);
    mc_error_message (&mcerror, NULL);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for utime VFS-function.
 *
 * @param vpath path to file or directory
 * @param times access and modification time to set
 * @return 0 if success, negative value otherwise
 */

static int
sftpfs_cb_utime (const vfs_path_t *vpath, mc_timesbuf_t *times)
{
    int rc;
    GError *mcerror = NULL;
    mc_timespec_t atime, mtime;

    vfs_get_timespecs_from_timesbuf (times, &atime, &mtime);
    rc = sftpfs_utime (vpath, atime.tv_sec, mtime.tv_sec, &mcerror);

    mc_error_message (&mcerror, NULL);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for symlink VFS-function.
 *
 * @param vpath1 path to file or directory
 * @param vpath2 path to symlink
 * @return 0 if success, negative value otherwise
 */

static int
sftpfs_cb_symlink (const vfs_path_t *vpath1, const vfs_path_t *vpath2)
{
    int rc;
    GError *mcerror = NULL;

    rc = sftpfs_symlink (vpath1, vpath2, &mcerror);
    mc_error_message (&mcerror, NULL);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for symlink VFS-function.
 *
 * @param vpath unused
 * @param mode  unused
 * @param dev   unused
 * @return always 0
 */

static int
sftpfs_cb_mknod (const vfs_path_t *vpath, mode_t mode, dev_t dev)
{
    (void) vpath;
    (void) mode;
    (void) dev;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for link VFS-function.
 *
 * @param vpath1 unused
 * @param vpath2 unused
 * @return always 0
 */

static int
sftpfs_cb_link (const vfs_path_t *vpath1, const vfs_path_t *vpath2)
{
    (void) vpath1;
    (void) vpath2;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for chown VFS-function.
 *
 * @param vpath unused
 * @param owner unused
 * @param group unused
 * @return always 0
 */

static int
sftpfs_cb_chown (const vfs_path_t *vpath, uid_t owner, gid_t group)
{
    (void) vpath;
    (void) owner;
    (void) group;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for reading file content.
 *
 * @param data   file data handler
 * @param buffer buffer for data
 * @param count  data size
 * @return 0 if success, negative value otherwise
 */

static ssize_t
sftpfs_cb_read (void *data, char *buffer, size_t count)
{
    int rc;
    GError *mcerror = NULL;
    vfs_file_handler_t *fh = VFS_FILE_HANDLER (data);

    if (tty_got_interrupt ())
    {
        tty_disable_interrupt_key ();
        return 0;
    }

    rc = sftpfs_read_file (fh, buffer, count, &mcerror);
    mc_error_message (&mcerror, NULL);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for writing file content.
 *
 * @param data  file data handler
 * @param buf   buffer for data
 * @param count data size
 * @return 0 if success, negative value otherwise
 */

static ssize_t
sftpfs_cb_write (void *data, const char *buf, size_t nbyte)
{
    int rc;
    GError *mcerror = NULL;
    vfs_file_handler_t *fh = VFS_FILE_HANDLER (data);

    rc = sftpfs_write_file (fh, buf, nbyte, &mcerror);
    mc_error_message (&mcerror, NULL);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for close file.
 *
 * @param data file data handler
 * @return 0 if success, negative value otherwise
 */

static int
sftpfs_cb_close (void *data)
{
    int rc;
    GError *mcerror = NULL;
    struct vfs_s_super *super = VFS_FILE_HANDLER_SUPER (data);
    vfs_file_handler_t *fh = VFS_FILE_HANDLER (data);

    super->fd_usage--;
    if (super->fd_usage == 0)
        vfs_stamp_create (vfs_sftpfs_ops, super);

    rc = sftpfs_close_file (fh, &mcerror);
    mc_error_message (&mcerror, NULL);

    if (fh->handle != -1)
        close (fh->handle);

    vfs_s_free_inode (vfs_sftpfs_ops, fh->ino);

    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for chmod VFS-function.
 *
 * @param vpath path to file or directory
 * @param mode  mode (see man 2 open)
 * @return 0 if success, negative value otherwise
 */

static int
sftpfs_cb_chmod (const vfs_path_t *vpath, mode_t mode)
{
    int rc;
    GError *mcerror = NULL;

    rc = sftpfs_chmod (vpath, mode, &mcerror);
    mc_error_message (&mcerror, NULL);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for mkdir VFS-function.
 *
 * @param vpath path directory
 * @param mode  mode (see man 2 open)
 * @return 0 if success, negative value otherwise
 */

static int
sftpfs_cb_mkdir (const vfs_path_t *vpath, mode_t mode)
{
    int rc;
    GError *mcerror = NULL;

    rc = sftpfs_mkdir (vpath, mode, &mcerror);
    mc_error_message (&mcerror, NULL);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for rmdir VFS-function.
 *
 * @param vpath path directory
 * @return 0 if success, negative value otherwise
 */

static int
sftpfs_cb_rmdir (const vfs_path_t *vpath)
{
    int rc;
    GError *mcerror = NULL;

    rc = sftpfs_rmdir (vpath, &mcerror);
    mc_error_message (&mcerror, NULL);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for lseek VFS-function.
 *
 * @param data   file data handler
 * @param offset file offset
 * @param whence method of seek (at begin, at current, at end)
 * @return 0 if success, negative value otherwise
 */

static off_t
sftpfs_cb_lseek (void *data, off_t offset, int whence)
{
    off_t ret_offset;
    vfs_file_handler_t *fh = VFS_FILE_HANDLER (data);
    GError *mcerror = NULL;

    ret_offset = sftpfs_lseek (fh, offset, whence, &mcerror);
    mc_error_message (&mcerror, NULL);
    return ret_offset;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for unlink VFS-function.
 *
 * @param vpath path to file or directory
 * @return 0 if success, negative value otherwise
 */

static int
sftpfs_cb_unlink (const vfs_path_t *vpath)
{
    int rc;
    GError *mcerror = NULL;

    rc = sftpfs_unlink (vpath, &mcerror);
    mc_error_message (&mcerror, NULL);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for rename VFS-function.
 *
 * @param vpath1 path to source file or directory
 * @param vpath2 path to destination file or directory
 * @return 0 if success, negative value otherwise
 */

static int
sftpfs_cb_rename (const vfs_path_t *vpath1, const vfs_path_t *vpath2)
{
    int rc;
    GError *mcerror = NULL;

    rc = sftpfs_rename (vpath1, vpath2, &mcerror);
    mc_error_message (&mcerror, NULL);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for errno VFS-function.
 *
 * @param me unused
 * @return value of errno global variable
 */

static int
sftpfs_cb_errno (struct vfs_class *me)
{
    (void) me;

    return errno;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for fill_names VFS function.
 * Add SFTP connections to the 'Active VFS connections'  list
 *
 * @param me   unused
 * @param func callback function for adding SFTP-connection to list of active connections
 */

static void
sftpfs_cb_fill_names (struct vfs_class *me, fill_names_f func)
{
    GList *iter;

    (void) me;

    for (iter = sftpfs_subclass.supers; iter != NULL; iter = g_list_next (iter))
    {
        const struct vfs_s_super *super = (const struct vfs_s_super *) iter->data;
        GString *name;

        name = vfs_path_element_build_pretty_path_str (super->path_element);

        func (name->str);
        g_string_free (name, TRUE);
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for checking if connection is equal to existing connection.
 *
 * @param vpath_element path element with connection data
 * @param super         data with exists connection
 * @param vpath         unused
 * @param cookie        unused
 * @return TRUE if connections is equal, FALSE otherwise
 */

static gboolean
sftpfs_archive_same (const vfs_path_element_t *vpath_element, struct vfs_s_super *super,
                     const vfs_path_t *vpath, void *cookie)
{
    int result;
    vfs_path_element_t *orig_connect_info;

    (void) vpath;
    (void) cookie;

    orig_connect_info = SFTP_SUPER (super)->original_connection_info;

    result = ((g_strcmp0 (vpath_element->host, orig_connect_info->host) == 0)
              && (g_strcmp0 (vpath_element->user, orig_connect_info->user) == 0)
              && (vpath_element->port == orig_connect_info->port));

    return result;
}

/* --------------------------------------------------------------------------------------------- */

static struct vfs_s_super *
sftpfs_new_archive (struct vfs_class *me)
{
    sftpfs_super_t *arch;

    arch = g_new0 (sftpfs_super_t, 1);
    arch->base.me = me;
    arch->base.name = g_strdup (PATH_SEP_STR);
    arch->auth_type = NONE;
    arch->config_auth_type = NONE;
    arch->socket_handle = LIBSSH2_INVALID_SOCKET;

    return VFS_SUPER (arch);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for opening new connection.
 *
 * @param super         connection data
 * @param vpath         unused
 * @param vpath_element path element with connection data
 * @return 0 if success, -1 otherwise
 */

static int
sftpfs_open_archive (struct vfs_s_super *super, const vfs_path_t *vpath,
                     const vfs_path_element_t *vpath_element)
{
    GError *mcerror = NULL;
    sftpfs_super_t *sftpfs_super = SFTP_SUPER (super);
    int ret_value;

    (void) vpath;

    if (vpath_element->host == NULL || *vpath_element->host == '\0')
    {
        vfs_print_message ("%s", _("sftp: Invalid host name."));
        vpath_element->class->verrno = EPERM;
        return -1;
    }

    sftpfs_super->original_connection_info = vfs_path_element_clone (vpath_element);
    super->path_element = vfs_path_element_clone (vpath_element);

    sftpfs_fill_connection_data_from_config (super, &mcerror);
    if (mc_error_message (&mcerror, &ret_value))
    {
        vpath_element->class->verrno = ret_value;
        return -1;
    }

    super->root =
        vfs_s_new_inode (vpath_element->class, super,
                         vfs_s_default_stat (vpath_element->class, S_IFDIR | 0755));

    ret_value = sftpfs_open_connection (super, &mcerror);
    mc_error_message (&mcerror, NULL);
    return ret_value;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for closing connection.
 *
 * @param me    unused
 * @param super connection data
 */

static void
sftpfs_free_archive (struct vfs_class *me, struct vfs_s_super *super)
{
    GError *mcerror = NULL;

    (void) me;

    sftpfs_close_connection (super, "Normal Shutdown", &mcerror);

    vfs_path_element_free (SFTP_SUPER (super)->original_connection_info);

    mc_error_message (&mcerror, NULL);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for getting directory content.
 *
 * @param me          unused
 * @param dir         unused
 * @param remote_path unused
 * @return always 0
 */

static int
sftpfs_cb_dir_load (struct vfs_class *me, struct vfs_s_inode *dir, const char *remote_path)
{
    (void) me;
    (void) dir;
    (void) remote_path;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Initialization of SFTP Virtual File Sysytem.
 */

void
vfs_init_sftpfs (void)
{
    tcp_init ();

    vfs_init_subclass (&sftpfs_subclass, "sftpfs", VFSF_NOLINKS | VFSF_REMOTE, "sftp");

    vfs_sftpfs_ops->init = sftpfs_cb_init;
    vfs_sftpfs_ops->done = sftpfs_cb_done;

    vfs_sftpfs_ops->fill_names = sftpfs_cb_fill_names;

    vfs_sftpfs_ops->opendir = sftpfs_cb_opendir;
    vfs_sftpfs_ops->readdir = sftpfs_cb_readdir;
    vfs_sftpfs_ops->closedir = sftpfs_cb_closedir;
    vfs_sftpfs_ops->mkdir = sftpfs_cb_mkdir;
    vfs_sftpfs_ops->rmdir = sftpfs_cb_rmdir;

    vfs_sftpfs_ops->stat = sftpfs_cb_stat;
    vfs_sftpfs_ops->lstat = sftpfs_cb_lstat;
    vfs_sftpfs_ops->fstat = sftpfs_cb_fstat;
    vfs_sftpfs_ops->readlink = sftpfs_cb_readlink;
    vfs_sftpfs_ops->symlink = sftpfs_cb_symlink;
    vfs_sftpfs_ops->link = sftpfs_cb_link;
    vfs_sftpfs_ops->utime = sftpfs_cb_utime;
    vfs_sftpfs_ops->mknod = sftpfs_cb_mknod;
    vfs_sftpfs_ops->chown = sftpfs_cb_chown;
    vfs_sftpfs_ops->chmod = sftpfs_cb_chmod;

    vfs_sftpfs_ops->open = sftpfs_cb_open;
    vfs_sftpfs_ops->read = sftpfs_cb_read;
    vfs_sftpfs_ops->write = sftpfs_cb_write;
    vfs_sftpfs_ops->close = sftpfs_cb_close;
    vfs_sftpfs_ops->lseek = sftpfs_cb_lseek;
    vfs_sftpfs_ops->unlink = sftpfs_cb_unlink;
    vfs_sftpfs_ops->rename = sftpfs_cb_rename;
    vfs_sftpfs_ops->ferrno = sftpfs_cb_errno;

    sftpfs_subclass.archive_same = sftpfs_archive_same;
    sftpfs_subclass.new_archive = sftpfs_new_archive;
    sftpfs_subclass.open_archive = sftpfs_open_archive;
    sftpfs_subclass.free_archive = sftpfs_free_archive;
    sftpfs_subclass.fh_new = sftpfs_fh_new;
    sftpfs_subclass.dir_load = sftpfs_cb_dir_load;

    vfs_register_class (vfs_sftpfs_ops);
}

/* --------------------------------------------------------------------------------------------- */

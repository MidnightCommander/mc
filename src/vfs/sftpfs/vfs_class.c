/* Virtual File System: SFTP file system.
   The VFS class functions

   Copyright (C) 2011-2017
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2011
   Slava Zanko <slavazanko@gmail.com>, 2011, 2012

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

#include "lib/global.h"
#include "lib/widget.h"
#include "lib/vfs/gc.h"
#include "lib/tty/tty.h"        /* tty_enable_interrupt_key () */

#include "internal.h"

/*** global variables ****************************************************************************/

struct vfs_class sftpfs_class;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

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
sftpfs_cb_open (const vfs_path_t * vpath, int flags, mode_t mode)
{
    vfs_file_handler_t *file_handler;
    const vfs_path_element_t *path_element;
    struct vfs_s_super *super;
    const char *path_super;
    struct vfs_s_inode *path_inode;
    GError *mcerror = NULL;
    gboolean is_changed = FALSE;

    path_element = vfs_path_get_by_index (vpath, -1);

    path_super = vfs_s_get_path (vpath, &super, 0);
    if (path_super == NULL)
        return NULL;

    path_inode = vfs_s_find_inode (path_element->class, super, path_super, LINK_FOLLOW, FL_NONE);
    if (path_inode != NULL && ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)))
    {
        path_element->class->verrno = EEXIST;
        return NULL;
    }

    if (path_inode == NULL)
    {
        char *dirname, *name;
        struct vfs_s_entry *ent;
        struct vfs_s_inode *dir;

        dirname = g_path_get_dirname (path_super);
        name = g_path_get_basename (path_super);
        dir = vfs_s_find_inode (path_element->class, super, dirname, LINK_FOLLOW, FL_DIR);
        if (dir == NULL)
        {
            g_free (dirname);
            g_free (name);
            return NULL;
        }
        ent = vfs_s_generate_entry (path_element->class, name, dir, 0755);
        path_inode = ent->ino;
        vfs_s_insert_entry (path_element->class, dir, ent);
        g_free (dirname);
        g_free (name);
        is_changed = TRUE;
    }

    if (S_ISDIR (path_inode->st.st_mode))
    {
        path_element->class->verrno = EISDIR;
        return NULL;
    }

    file_handler = g_new0 (vfs_file_handler_t, 1);
    file_handler->pos = 0;
    file_handler->ino = path_inode;
    file_handler->handle = -1;
    file_handler->changed = is_changed;
    file_handler->linear = 0;
    file_handler->data = NULL;

    if (!sftpfs_open_file (file_handler, flags, mode, &mcerror))
    {
        mc_error_message (&mcerror, NULL);
        g_free (file_handler);
        return NULL;
    }

    vfs_rmstamp (path_element->class, (vfsid) super);
    super->fd_usage++;
    file_handler->ino->st.st_nlink++;
    return file_handler;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for opening directory.
 *
 * @param vpath path to directory
 * @return directory data handler if success, NULL otherwise
 */

static void *
sftpfs_cb_opendir (const vfs_path_t * vpath)
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

static void *
sftpfs_cb_readdir (void *data)
{
    GError *mcerror = NULL;
    union vfs_dirent *sftpfs_dirent;

    if (tty_got_interrupt ())
    {
        tty_disable_interrupt_key ();
        return NULL;
    }

    sftpfs_dirent = sftpfs_readdir (data, &mcerror);
    if (!mc_error_message (&mcerror, NULL))
    {
        if (sftpfs_dirent != NULL)
            vfs_print_message (_("sftp: (Ctrl-G break) Listing... %s"), sftpfs_dirent->dent.d_name);
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
sftpfs_cb_lstat (const vfs_path_t * vpath, struct stat *buf)
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
sftpfs_cb_stat (const vfs_path_t * vpath, struct stat *buf)
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
sftpfs_cb_readlink (const vfs_path_t * vpath, char *buf, size_t size)
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
 * @param vpath unused
 * @param times unused
 * @return always 0
 */

static int
sftpfs_cb_utime (const vfs_path_t * vpath, mc_timesbuf_t * times)
{
    (void) vpath;
    (void) times;

    return 0;
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
sftpfs_cb_symlink (const vfs_path_t * vpath1, const vfs_path_t * vpath2)
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
sftpfs_cb_mknod (const vfs_path_t * vpath, mode_t mode, dev_t dev)
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
sftpfs_cb_link (const vfs_path_t * vpath1, const vfs_path_t * vpath2)
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
sftpfs_cb_chown (const vfs_path_t * vpath, uid_t owner, gid_t group)
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
    vfs_file_handler_t *fh = (vfs_file_handler_t *) data;

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
    vfs_file_handler_t *fh = (vfs_file_handler_t *) data;

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
    struct vfs_s_super *super;
    vfs_file_handler_t *file_handler = (vfs_file_handler_t *) data;

    super = file_handler->ino->super;

    super->fd_usage--;
    if (super->fd_usage == 0)
        vfs_stamp_create (&sftpfs_class, super);

    rc = sftpfs_close_file (file_handler, &mcerror);
    mc_error_message (&mcerror, NULL);

    if (file_handler->handle != -1)
        close (file_handler->handle);

    vfs_s_free_inode (&sftpfs_class, file_handler->ino);
    g_free (file_handler);

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
sftpfs_cb_chmod (const vfs_path_t * vpath, mode_t mode)
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
sftpfs_cb_mkdir (const vfs_path_t * vpath, mode_t mode)
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
sftpfs_cb_rmdir (const vfs_path_t * vpath)
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
    vfs_file_handler_t *file_handler = (vfs_file_handler_t *) data;
    GError *mcerror = NULL;

    ret_offset = sftpfs_lseek (file_handler, offset, whence, &mcerror);
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
sftpfs_cb_unlink (const vfs_path_t * vpath)
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
sftpfs_cb_rename (const vfs_path_t * vpath1, const vfs_path_t * vpath2)
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
        char *name;

        name = vfs_path_element_build_pretty_path_str (super->path_element);

        func (name);
        g_free (name);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Initialization of VFS class structure.
 *
 * @return the VFS class structure.
 */

void
sftpfs_init_class (void)
{
    memset (&sftpfs_class, 0, sizeof (sftpfs_class));
    sftpfs_class.name = "sftpfs";
    sftpfs_class.prefix = "sftp";
    sftpfs_class.flags = VFSF_NOLINKS;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Initialization of VFS class callbacks.
 */

void
sftpfs_init_class_callbacks (void)
{
    sftpfs_class.init = sftpfs_cb_init;
    sftpfs_class.done = sftpfs_cb_done;

    sftpfs_class.fill_names = sftpfs_cb_fill_names;

    sftpfs_class.opendir = sftpfs_cb_opendir;
    sftpfs_class.readdir = sftpfs_cb_readdir;
    sftpfs_class.closedir = sftpfs_cb_closedir;
    sftpfs_class.mkdir = sftpfs_cb_mkdir;
    sftpfs_class.rmdir = sftpfs_cb_rmdir;

    sftpfs_class.stat = sftpfs_cb_stat;
    sftpfs_class.lstat = sftpfs_cb_lstat;
    sftpfs_class.fstat = sftpfs_cb_fstat;
    sftpfs_class.readlink = sftpfs_cb_readlink;
    sftpfs_class.symlink = sftpfs_cb_symlink;
    sftpfs_class.link = sftpfs_cb_link;
    sftpfs_class.utime = sftpfs_cb_utime;
    sftpfs_class.mknod = sftpfs_cb_mknod;
    sftpfs_class.chown = sftpfs_cb_chown;
    sftpfs_class.chmod = sftpfs_cb_chmod;

    sftpfs_class.open = sftpfs_cb_open;
    sftpfs_class.read = sftpfs_cb_read;
    sftpfs_class.write = sftpfs_cb_write;
    sftpfs_class.close = sftpfs_cb_close;
    sftpfs_class.lseek = sftpfs_cb_lseek;
    sftpfs_class.unlink = sftpfs_cb_unlink;
    sftpfs_class.rename = sftpfs_cb_rename;
    sftpfs_class.ferrno = sftpfs_cb_errno;
}

/* --------------------------------------------------------------------------------------------- */

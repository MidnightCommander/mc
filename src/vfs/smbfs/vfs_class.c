/* Virtual File System: Samba file system.
   The VFS class functions

   Copyright (C) 2013
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2013

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
#include "lib/vfs/gc.h"
#include "lib/vfs/utilvfs.h"
#include "lib/tty/tty.h"        /* tty_enable_interrupt_key () */

#include "internal.h"

/*** global variables ****************************************************************************/

struct vfs_class smbfs_class;

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
smbfs_cb_init (struct vfs_class *me)
{
    SMBCCTX *smb_context;

    (void) me;

    if (smbc_init (smbfs_cb_authdata_provider, 0))
    {
        fprintf (stderr, "smbc_init returned %s (%i)\n", strerror (errno), errno);
        return FALSE;
    }

    smb_context = smbc_set_context (NULL);
    smbc_setDebug (smb_context, 0);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for VFS-class deinit action.
 *
 * @param me structure of VFS class
 */

static void
smbfs_cb_done (struct vfs_class *me)
{
    (void) me;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for opening directory.
 *
 * @param vpath path to directory
 * @return directory data handler if success, NULL otherwise
 */

static void *
smbfs_cb_opendir (const vfs_path_t * vpath)
{
    GError *error = NULL;
    void *ret_value;

    /* reset interrupt flag */
    tty_got_interrupt ();

    ret_value = smbfs_dir_open (vpath, &error);
    vfs_show_gerror (&error);
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
smbfs_cb_readdir (void *data)
{
    GError *error = NULL;
    union vfs_dirent *smbfs_dirent;

    if (tty_got_interrupt ())
    {
        tty_disable_interrupt_key ();
        return NULL;
    }

    smbfs_dirent = smbfs_dir_read (data, &error);
    if (!vfs_show_gerror (&error))
    {
        if (smbfs_dirent != NULL)
            vfs_print_message (_("smbfs: (Ctrl-G break) Listing... %s"), smbfs_dirent->dent.d_name);
        else
            vfs_print_message (_("smbfs: Listing done."));
    }

    return smbfs_dirent;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for closing directory.
 *
 * @param data directory data handler
 * @return 0 if sucess, negative value otherwise
 */

static int
smbfs_cb_closedir (void *data)
{
    int rc;
    GError *error = NULL;

    rc = smbfs_dir_close (data, &error);
    vfs_show_gerror (&error);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Callback for fill_names VFS function.
 * Add Samba connections to the 'Active VFS connections'  list
 *
 * @param me   unused
 * @param func callback function for adding Samba-connection to list of active connections
 */

static void
smbfs_cb_fill_names (struct vfs_class *me, fill_names_f func)
{
    GList *iter;

    (void) me;

    for (iter = smbfs_subclass.supers; iter != NULL; iter = g_list_next (iter))
    {
        const struct vfs_s_super *super = (const struct vfs_s_super *) iter->data;
        char *name;

        name = vfs_path_element_build_pretty_path_str (super->path_element);

        func (name);
        g_free (name);
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for mkdir VFS-function.
 *
 * @param vpath path directory
 * @param mode  mode (see man 2 open)
 * @return 0 if sucess, negative value otherwise
 */

static int
smbfs_cb_mkdir (const vfs_path_t * vpath, mode_t mode)
{
    int rc;
    GError *error = NULL;

    rc = smbfs_dir_make (vpath, mode, &error);
    vfs_show_gerror (&error);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for rmdir VFS-function.
 *
 * @param vpath path directory
 * @param mode  mode (see man 2 open)
 * @return 0 if sucess, negative value otherwise
 */

static int
smbfs_cb_rmdir (const vfs_path_t * vpath)
{
    int rc;
    GError *error = NULL;

    rc = smbfs_dir_remove (vpath, &error);
    vfs_show_gerror (&error);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for lstat VFS-function.
 *
 * @param vpath path to file or directory
 * @param buf   buffer for store stat-info
 * @return 0 if sucess, negative value otherwise
 */

static int
smbfs_cb_lstat (const vfs_path_t * vpath, struct stat *buf)
{
    int rc;
    GError *error = NULL;

    rc = smbfs_lstat (vpath, buf, &error);
    vfs_show_gerror (&error);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for stat VFS-function.
 *
 * @param vpath path to file or directory
 * @param buf   buffer for store stat-info
 * @return 0 if sucess, negative value otherwise
 */

static int
smbfs_cb_stat (const vfs_path_t * vpath, struct stat *buf)
{
    int rc;
    GError *error = NULL;

    rc = smbfs_stat (vpath, buf, &error);
    vfs_show_gerror (&error);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for fstat VFS-function.
 *
 * @param data file data handler
 * @param buf  buffer for store stat-info
 * @return 0 if sucess, negative value otherwise
 */

static int
smbfs_cb_fstat (void *data, struct stat *buf)
{
    int rc;
    GError *error = NULL;
    vfs_file_handler_t *file_handler = (vfs_file_handler_t *) data;

    rc = smbfs_file_stat (file_handler, buf, &error);
    if (rc < 0)
        vfs_show_gerror (&error);

    return rc;
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
smbfs_cb_open (const vfs_path_t * vpath, int flags, mode_t mode)
{
    vfs_file_handler_t *file_handler;
    struct vfs_s_super *super;
    GError *error = NULL;

    super = vfs_get_super_by_vpath (vpath, TRUE);
    if (super == NULL)
        return NULL;

    file_handler = vfs_s_create_file_handler (super, vpath, flags);
    if (file_handler == NULL)
        return NULL;

    if (!smbfs_file_open (file_handler, vpath, flags, mode, &error))
    {
        vfs_show_gerror (&error);
        g_free (file_handler);
        return NULL;
    }
    vfs_s_open_file_post_action (vpath, super, file_handler);

    return file_handler;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for reading file content.
 *
 * @param data   file data handler
 * @param buffer buffer for data
 * @param count  data size
 * @return 0 if sucess, negative value otherwise
 */

static ssize_t
smbfs_cb_read (void *data, char *buffer, size_t count)
{
    int rc;
    GError *error = NULL;
    vfs_file_handler_t *fh = (vfs_file_handler_t *) data;

    if (tty_got_interrupt ())
    {
        tty_disable_interrupt_key ();
        return 0;
    }

    rc = smbfs_file_read (fh, buffer, count, &error);
    vfs_show_gerror (&error);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for writing file content.
 *
 * @param data  file data handler
 * @param buf   buffer for data
 * @param count data size
 * @return 0 if sucess, negative value otherwise
 */

static ssize_t
smbfs_cb_write (void *data, const char *buf, size_t nbyte)
{
    int rc;
    GError *error = NULL;
    vfs_file_handler_t *fh = (vfs_file_handler_t *) data;

    rc = smbfs_file_write (fh, buf, nbyte, &error);
    vfs_show_gerror (&error);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for close file.
 *
 * @param data file data handler
 * @return 0 if sucess, negative value otherwise
 */

static int
smbfs_cb_close (void *data)
{
    int rc;
    GError *error = NULL;
    struct vfs_s_super *super;
    vfs_file_handler_t *file_handler = (vfs_file_handler_t *) data;

    super = file_handler->ino->super;

    super->fd_usage--;
    if (super->fd_usage == 0)
        vfs_stamp_create (&smbfs_class, super);

    rc = smbfs_file_close (file_handler, &error);
    vfs_show_gerror (&error);
    vfs_s_free_inode (&smbfs_class, file_handler->ino);
    g_free (file_handler);

    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for lseek VFS-function.
 *
 * @param data   file data handler
 * @param offset file offset
 * @param whence method of seek (at begin, at current, at end)
 * @return 0 if sucess, negative value otherwise
 */

static off_t
smbfs_cb_lseek (void *data, off_t offset, int whence)
{
    off_t ret_offset;
    vfs_file_handler_t *file_handler = (vfs_file_handler_t *) data;
    GError *error = NULL;

    ret_offset = smbfs_file_lseek (file_handler, offset, whence, &error);
    vfs_show_gerror (&error);
    return ret_offset;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for chmod VFS-function.
 *
 * @param vpath path to file or directory
 * @param mode  mode (see man 2 open)
 * @return 0 if sucess, negative value otherwise
 */

static int
smbfs_cb_chmod (const vfs_path_t * vpath, mode_t mode)
{
    int rc;
    GError *error = NULL;

    rc = smbfs_attr_chmod (vpath, mode, &error);
    vfs_show_gerror (&error);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for unlink VFS-function.
 *
 * @param vpath path to file or directory
 * @return 0 if sucess, negative value otherwise
 */

static int
smbfs_cb_unlink (const vfs_path_t * vpath)
{
    int rc;
    GError *error = NULL;

    rc = smbfs_unlink (vpath, &error);
    vfs_show_gerror (&error);
    return rc;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for rename VFS-function.
 *
 * @param vpath1 path to source file or directory
 * @param vpath2 path to destination file or directory
 * @return 0 if sucess, negative value otherwise
 */

static int
smbfs_cb_rename (const vfs_path_t * vpath1, const vfs_path_t * vpath2)
{
    int rc;
    GError *error = NULL;

    rc = smbfs_rename (vpath1, vpath2, &error);
    vfs_show_gerror (&error);
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
smbfs_cb_utime (const vfs_path_t * vpath, struct utimbuf *times)
{
    int rc;
    GError *error = NULL;

    rc = smbfs_file_change_modification_time (vpath, times, &error);

    vfs_show_gerror (&error);
    return rc;
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
smbfs_init_class (void)
{
    memset (&smbfs_class, 0, sizeof (struct vfs_class));
    smbfs_class.name = "smbfs";
    smbfs_class.prefix = "smb";
    smbfs_class.flags = VFSF_NOLINKS;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Initialization of VFS class callbacks.
 */

void
smbfs_init_class_callbacks (void)
{
    smbfs_class.init = smbfs_cb_init;
    smbfs_class.done = smbfs_cb_done;

    smbfs_class.fill_names = smbfs_cb_fill_names;
    smbfs_class.opendir = smbfs_cb_opendir;
    smbfs_class.readdir = smbfs_cb_readdir;
    smbfs_class.closedir = smbfs_cb_closedir;
    smbfs_class.mkdir = smbfs_cb_mkdir;
    smbfs_class.rmdir = smbfs_cb_rmdir;
    smbfs_class.stat = smbfs_cb_stat;
    smbfs_class.lstat = smbfs_cb_lstat;
    smbfs_class.fstat = smbfs_cb_fstat;
    smbfs_class.utime = smbfs_cb_utime;
    smbfs_class.chmod = smbfs_cb_chmod;
    smbfs_class.open = smbfs_cb_open;
    smbfs_class.read = smbfs_cb_read;
    smbfs_class.write = smbfs_cb_write;
    smbfs_class.close = smbfs_cb_close;
    smbfs_class.lseek = smbfs_cb_lseek;
    smbfs_class.unlink = smbfs_cb_unlink;
    smbfs_class.rename = smbfs_cb_rename;
}

/* --------------------------------------------------------------------------------------------- */

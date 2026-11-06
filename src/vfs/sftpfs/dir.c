/* Virtual File System: SFTP file system.
   The internal functions: dirs

   Copyright (C) 2011-2026
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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <errno.h>
#include <string.h>

#include <libssh2.h>
#include <libssh2_sftp.h>

#include "lib/global.h"
#include "lib/util.h"
#include "lib/tty/tty.h"  // tty_got_interrupt ()
#include "lib/vfs/utilvfs.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Read the target of a symbolic link found in the directory being loaded.
 *
 * @param super  connection data
 * @param path   full remote path of the symbolic link
 * @return newly allocated link target, NULL on failure
 */

static char *
sftpfs_dir_load_linkname (sftpfs_super_t *super, const char *path)
{
    char buf[MC_MAXPATHLEN];
    int res;

    do
        res = libssh2_sftp_symlink_ex (super->sftp_session, path, (unsigned int) strlen (path), buf,
                                       sizeof (buf), LIBSSH2_SFTP_READLINK);
    while (res == LIBSSH2_ERROR_EAGAIN && sftpfs_waitsocket (super, res, NULL));

    return (res > 0 ? g_strndup (buf, (gsize) res) : NULL);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Load the content of a directory into the directory cache.
 *
 * The attributes returned by READDIR are stored in the cached inodes, so the following lstat()
 * calls for the entries are answered from the cache without a round trip per file.
 *
 * @param me          VFS class
 * @param dir         inode of the directory to fill
 * @param remote_path path of the directory
 * @param mcerror     pointer to the error handler
 * @return 0 if success, -1 otherwise
 */

int
sftpfs_dir_load (struct vfs_class *me, struct vfs_s_inode *dir, const char *remote_path,
                 GError **mcerror)
{
    sftpfs_super_t *super = SFTP_SUPER (dir->super);
    LIBSSH2_SFTP_HANDLE *handle;
    char *path;
    size_t path_len;
    int rc;

    mc_return_val_if_error (mcerror, -1);

    if (super->sftp_session == NULL)
    {
        errno = me->verrno = ECONNRESET;
        return -1;
    }

    // sftpfs_fix_filename() returns a shared buffer, keep a copy of the path
    path = g_strdup (sftpfs_fix_filename (remote_path)->str);
    path_len = strlen (path);

    vfs_print_message (_ ("sftp: Reading directory %s..."), path);

    while (TRUE)
    {
        int err = 0;

        handle = libssh2_sftp_open_ex (super->sftp_session, path, (unsigned int) path_len, 0, 0,
                                       LIBSSH2_SFTP_OPENDIR);
        if (handle != NULL)
            break;

        rc = libssh2_session_last_errno (super->session);

        // a missing or unreadable directory is not worth an error dialog
        if (sftpfs_is_sftp_error (super->sftp_session, rc, LIBSSH2_FX_NO_SUCH_FILE))
            err = ENOENT;
        else if (sftpfs_is_sftp_error (super->sftp_session, rc, LIBSSH2_FX_PERMISSION_DENIED))
            err = EACCES;

        if (err != 0)
        {
            errno = me->verrno = err;
            g_free (path);
            return -1;
        }

        if (!sftpfs_waitsocket (super, rc, mcerror))
        {
            errno = me->verrno = EIO;
            g_free (path);
            return -1;
        }
    }

    // reset interrupt flag
    tty_got_interrupt ();

    while (TRUE)
    {
        char name[BUF_MEDIUM];
        LIBSSH2_SFTP_ATTRIBUTES attrs;
        struct stat st;
        struct vfs_s_entry *ent;

        rc = libssh2_sftp_readdir (handle, name, sizeof (name), &attrs);
        if (rc == 0)
            break;

        if (rc < 0)
        {
            if (rc == LIBSSH2_ERROR_EAGAIN && sftpfs_waitsocket (super, rc, mcerror))
                continue;
            break;
        }

        if (tty_got_interrupt ())
        {
            tty_disable_interrupt_key ();
            rc = LIBSSH2_ERROR_EAGAIN;
            break;
        }

        if (DIR_IS_DOT (name) || DIR_IS_DOTDOT (name))
            continue;

        /* Fill the stat before creating the inode: vfs_s_new_inode() sets the inode number and
         * the device of the cache entry on top of it. */
        st = *vfs_s_default_stat (me, S_IFREG | 0644);
        sftpfs_attr_to_stat (&attrs, &st);
        ent = vfs_s_new_entry (me, name, vfs_s_new_inode (me, dir->super, &st));

        if (S_ISLNK (ent->ino->st.st_mode))
        {
            char *link_path;

            link_path = g_strconcat (path, IS_PATH_SEP (path[path_len - 1]) ? "" : PATH_SEP_STR,
                                     name, (char *) NULL);
            ent->ino->linkname = sftpfs_dir_load_linkname (super, link_path);
            g_free (link_path);
        }

        vfs_s_insert_entry (me, dir, ent);
    }

    libssh2_sftp_closedir (handle);
    g_free (path);

    if (rc < 0)
    {
        if (*mcerror == NULL)
            vfs_print_message ("%s", _ ("sftp: failure"));
        errno = me->verrno = EIO;
        return -1;
    }

    dir->timestamp = g_get_monotonic_time () + SFTPFS_DIR_CACHE_TIMEOUT * G_USEC_PER_SEC;
    vfs_print_message ("%s", _ ("sftp: Listing done."));

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Create a new directory.
 *
 * @param vpath   path directory
 * @param mode    mode (see man 2 open)
 * @param mcerror pointer to the error handler
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_mkdir (const vfs_path_t *vpath, mode_t mode, GError **mcerror)
{
    int res;
    sftpfs_super_t *sftpfs_super;
    const vfs_path_element_t *path_element;
    const GString *fixfname;

    if (!sftpfs_op_init (&sftpfs_super, &path_element, vpath, mcerror))
        return -1;

    fixfname = sftpfs_fix_filename (path_element->path);

    do
    {
        res =
            libssh2_sftp_mkdir_ex (sftpfs_super->sftp_session, fixfname->str, fixfname->len, mode);
        if (res >= 0)
            break;

        if (!sftpfs_waitsocket (sftpfs_super, res, mcerror))
            return -1;
    }
    while (res == LIBSSH2_ERROR_EAGAIN);

    return res;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Remove a directory.
 *
 * @param vpath   path directory
 * @param mcerror pointer to the error handler
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_rmdir (const vfs_path_t *vpath, GError **mcerror)
{
    int res;
    sftpfs_super_t *sftpfs_super;
    const vfs_path_element_t *path_element;
    const GString *fixfname;

    if (!sftpfs_op_init (&sftpfs_super, &path_element, vpath, mcerror))
        return -1;

    fixfname = sftpfs_fix_filename (path_element->path);

    do
    {
        res = libssh2_sftp_rmdir_ex (sftpfs_super->sftp_session, fixfname->str, fixfname->len);
        if (res >= 0)
            break;

        if (!sftpfs_waitsocket (sftpfs_super, res, mcerror))
            return -1;
    }
    while (res == LIBSSH2_ERROR_EAGAIN);

    return res;
}

/* --------------------------------------------------------------------------------------------- */

/* Virtual File System: SFTP file system.
   The internal functions: dirs

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

#include <libssh2.h>
#include <libssh2_sftp.h>

#include "lib/global.h"
#include "lib/util.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef struct
{
    LIBSSH2_SFTP_HANDLE *handle;
    sftpfs_super_data_t *super_data;
} sftpfs_dir_data_t;

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Open a directory stream corresponding to the directory name.
 *
 * @param vpath   path to directory
 * @param mcerror pointer to the error handler
 * @return directory data handler if success, NULL otherwise
 */

void *
sftpfs_opendir (const vfs_path_t * vpath, GError ** mcerror)
{
    sftpfs_dir_data_t *sftpfs_dir;
    struct vfs_s_super *super;
    sftpfs_super_data_t *super_data;
    const vfs_path_element_t *path_element;
    LIBSSH2_SFTP_HANDLE *handle;

    mc_return_val_if_error (mcerror, NULL);

    path_element = vfs_path_get_by_index (vpath, -1);

    if (vfs_s_get_path (vpath, &super, 0) == NULL)
        return NULL;

    super_data = (sftpfs_super_data_t *) super->data;

    while (TRUE)
    {
        const char *fixfname;
        unsigned int fixfname_len = 0;
        int libssh_errno;

        fixfname = sftpfs_fix_filename (path_element->path, &fixfname_len);

        handle =
            libssh2_sftp_open_ex (super_data->sftp_session, fixfname, fixfname_len, 0, 0,
                                  LIBSSH2_SFTP_OPENDIR);
        if (handle != NULL)
            break;

        libssh_errno = libssh2_session_last_errno (super_data->session);
        if (libssh_errno != LIBSSH2_ERROR_EAGAIN)
        {
            sftpfs_ssherror_to_gliberror (super_data, libssh_errno, mcerror);
            return NULL;
        }
        sftpfs_waitsocket (super_data, mcerror);

        mc_return_val_if_error (mcerror, NULL);
    }

    sftpfs_dir = g_new0 (sftpfs_dir_data_t, 1);
    sftpfs_dir->handle = handle;
    sftpfs_dir->super_data = super_data;

    return (void *) sftpfs_dir;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Get a pointer to a structure representing the next directory entry.
 *
 * @param data    directory data handler
 * @param mcerror pointer to the error handler
 * @return information about direntry if success, NULL otherwise
 */

void *
sftpfs_readdir (void *data, GError ** mcerror)
{
    char mem[BUF_MEDIUM];
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    sftpfs_dir_data_t *sftpfs_dir = (sftpfs_dir_data_t *) data;
    static union vfs_dirent sftpfs_dirent;
    int rc;

    mc_return_val_if_error (mcerror, NULL);

    do
    {
        rc = libssh2_sftp_readdir (sftpfs_dir->handle, mem, sizeof (mem), &attrs);
        if (rc >= 0)
            break;

        if (rc != LIBSSH2_ERROR_EAGAIN)
        {
            sftpfs_ssherror_to_gliberror (sftpfs_dir->super_data, rc, mcerror);
            return NULL;
        }

        sftpfs_waitsocket (sftpfs_dir->super_data, mcerror);
        mc_return_val_if_error (mcerror, NULL);
    }
    while (rc == LIBSSH2_ERROR_EAGAIN);

    if (rc == 0)
        return NULL;

    g_strlcpy (sftpfs_dirent.dent.d_name, mem, BUF_MEDIUM);
    return &sftpfs_dirent;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Close the directory stream.
 *
 * @param data    directory data handler
 * @param mcerror pointer to the error handler
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_closedir (void *data, GError ** mcerror)
{
    int rc;
    sftpfs_dir_data_t *sftpfs_dir = (sftpfs_dir_data_t *) data;

    mc_return_val_if_error (mcerror, -1);

    rc = libssh2_sftp_closedir (sftpfs_dir->handle);
    g_free (sftpfs_dir);
    return rc;
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
sftpfs_mkdir (const vfs_path_t * vpath, mode_t mode, GError ** mcerror)
{
    int res;
    struct vfs_s_super *super;
    sftpfs_super_data_t *super_data;
    const vfs_path_element_t *path_element;

    mc_return_val_if_error (mcerror, -1);

    path_element = vfs_path_get_by_index (vpath, -1);

    if (vfs_s_get_path (vpath, &super, 0) == NULL)
        return -1;

    if (super == NULL)
        return -1;

    super_data = (sftpfs_super_data_t *) super->data;
    if (super_data->sftp_session == NULL)
        return -1;

    do
    {
        const char *fixfname;
        unsigned int fixfname_len = 0;

        fixfname = sftpfs_fix_filename (path_element->path, &fixfname_len);

        res = libssh2_sftp_mkdir_ex (super_data->sftp_session, fixfname, fixfname_len, mode);
        if (res >= 0)
            break;

        if (res != LIBSSH2_ERROR_EAGAIN)
        {
            sftpfs_ssherror_to_gliberror (super_data, res, mcerror);
            return -1;
        }

        sftpfs_waitsocket (super_data, mcerror);
        mc_return_val_if_error (mcerror, -1);
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
sftpfs_rmdir (const vfs_path_t * vpath, GError ** mcerror)
{
    int res;
    struct vfs_s_super *super;
    sftpfs_super_data_t *super_data;
    const vfs_path_element_t *path_element;

    mc_return_val_if_error (mcerror, -1);

    path_element = vfs_path_get_by_index (vpath, -1);

    if (vfs_s_get_path (vpath, &super, 0) == NULL)
        return -1;

    if (super == NULL)
        return -1;

    super_data = (sftpfs_super_data_t *) super->data;
    if (super_data->sftp_session == NULL)
        return -1;

    do
    {
        const char *fixfname;
        unsigned int fixfname_len = 0;

        fixfname = sftpfs_fix_filename (path_element->path, &fixfname_len);

        res = libssh2_sftp_rmdir_ex (super_data->sftp_session, fixfname, fixfname_len);
        if (res >= 0)
            break;

        if (res != LIBSSH2_ERROR_EAGAIN)
        {
            sftpfs_ssherror_to_gliberror (super_data, res, mcerror);
            return -1;
        }

        sftpfs_waitsocket (super_data, mcerror);
        mc_return_val_if_error (mcerror, -1);
    }
    while (res == LIBSSH2_ERROR_EAGAIN);

    return res;
}

/* --------------------------------------------------------------------------------------------- */

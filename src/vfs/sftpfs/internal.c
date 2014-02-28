/* Virtual File System: SFTP file system.
   The internal functions

   Copyright (C) 2011-2014
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

#include "internal.h"

/*** global variables ****************************************************************************/

GString *sftpfs_filename_buffer = NULL;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Show error message (if error have raised) and cleanup GError object.
 *
 * @param error pointer to object contains error message
 * @return TRUE if error message was printed, FALSE otherwise
 */

gboolean
sftpfs_show_error (GError ** error)
{
    if (error == NULL || *error == NULL)
        return FALSE;

    vfs_print_message ("(%d) %s", (*error)->code, (*error)->message);
    g_error_free (*error);
    *error = NULL;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Convert libssh error to GError object.
 *
 * @param super_data   extra data for SFTP connection
 * @param libssh_errno errno from libssh
 * @param error        pointer to the error object
 */

void
sftpfs_ssherror_to_gliberror (sftpfs_super_data_t * super_data, int libssh_errno, GError ** error)
{
    char *err = NULL;
    int err_len;

    g_return_if_fail (error == NULL || *error == NULL);

    libssh2_session_last_error (super_data->session, &err, &err_len, 1);
    g_set_error (error, MC_ERROR, libssh_errno, "%s", err);
    g_free (err);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Fix filename for SFTP operations: add leading slash to file name.
 *
 * @param file_name file name
 * @return newly allocated string contains the file name with leading slash
 */

const char *
sftpfs_fix_filename (const char *file_name)
{
    g_string_printf (sftpfs_filename_buffer, "%c%s", PATH_SEP, file_name);
    return sftpfs_filename_buffer->str;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Awaiting for any activity on socket.
 *
 * @param super_data extra data for SFTP connection
 * @param error      unused
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_waitsocket (sftpfs_super_data_t * super_data, GError ** error)
{
    struct timeval timeout = { 10, 0 };
    fd_set fd;
    fd_set *writefd = NULL;
    fd_set *readfd = NULL;
    int dir;

    (void) error;

    FD_ZERO (&fd);
    FD_SET (super_data->socket_handle, &fd);

    /* now make sure we wait in the correct direction */
    dir = libssh2_session_block_directions (super_data->session);

    if ((dir & LIBSSH2_SESSION_BLOCK_INBOUND) != 0)
        readfd = &fd;

    if ((dir & LIBSSH2_SESSION_BLOCK_OUTBOUND) != 0)
        writefd = &fd;

    return select (super_data->socket_handle + 1, readfd, writefd, NULL, &timeout);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Getting information about a symbolic link.
 *
 * @param vpath path to file, directory or symbolic link
 * @param buf   buffer for store stat-info
 * @param error pointer to error object
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_lstat (const vfs_path_t * vpath, struct stat *buf, GError ** error)
{
    struct vfs_s_super *super;
    sftpfs_super_data_t *super_data;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int res;
    const vfs_path_element_t *path_element;

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

        fixfname = sftpfs_fix_filename (path_element->path);

        res = libssh2_sftp_stat_ex (super_data->sftp_session, fixfname,
                                    sftpfs_filename_buffer->len, LIBSSH2_SFTP_LSTAT, &attrs);
        if (res >= 0)
            break;

        if (res != LIBSSH2_ERROR_EAGAIN)
        {
            sftpfs_ssherror_to_gliberror (super_data, res, error);
            return -1;
        }

        sftpfs_waitsocket (super_data, error);
        if (error != NULL && *error != NULL)
            return -1;
    }
    while (res == LIBSSH2_ERROR_EAGAIN);

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_UIDGID) != 0)
    {
        buf->st_uid = attrs.uid;
        buf->st_gid = attrs.gid;
    }

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) != 0)
    {
        buf->st_atime = attrs.atime;
        buf->st_mtime = attrs.mtime;
        buf->st_ctime = attrs.mtime;
    }

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) != 0)
        buf->st_size = attrs.filesize;

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) != 0)
        buf->st_mode = attrs.permissions;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Getting information about a file or directory.
 *
 * @param vpath path to file or directory
 * @param buf   buffer for store stat-info
 * @param error pointer to error object
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_stat (const vfs_path_t * vpath, struct stat *buf, GError ** error)
{
    struct vfs_s_super *super;
    sftpfs_super_data_t *super_data;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int res;
    const vfs_path_element_t *path_element;

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

        fixfname = sftpfs_fix_filename (path_element->path);

        res =
            libssh2_sftp_stat_ex (super_data->sftp_session,
                                  fixfname, sftpfs_filename_buffer->len, LIBSSH2_SFTP_STAT, &attrs);
        if (res >= 0)
            break;

        if (res != LIBSSH2_ERROR_EAGAIN)
        {
            sftpfs_ssherror_to_gliberror (super_data, res, error);
            return -1;
        }

        sftpfs_waitsocket (super_data, error);
        if (error != NULL && *error != NULL)
            return -1;
    }
    while (res == LIBSSH2_ERROR_EAGAIN);

    buf->st_nlink = 1;
    if ((attrs.flags & LIBSSH2_SFTP_ATTR_UIDGID) != 0)
    {
        buf->st_uid = attrs.uid;
        buf->st_gid = attrs.gid;
    }

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) != 0)
    {
        buf->st_atime = attrs.atime;
        buf->st_mtime = attrs.mtime;
        buf->st_ctime = attrs.mtime;
    }

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) != 0)
        buf->st_size = attrs.filesize;

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) != 0)
        buf->st_mode = attrs.permissions;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Read value of a symbolic link.
 *
 * @param vpath path to file or directory
 * @param buf   buffer for store stat-info
 * @param size  buffer size
 * @param error pointer to error object
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_readlink (const vfs_path_t * vpath, char *buf, size_t size, GError ** error)
{
    struct vfs_s_super *super;
    sftpfs_super_data_t *super_data;
    int res;
    const vfs_path_element_t *path_element;

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

        fixfname = sftpfs_fix_filename (path_element->path);

        res = libssh2_sftp_readlink (super_data->sftp_session, fixfname, buf, size);
        if (res >= 0)
            break;

        if (res != LIBSSH2_ERROR_EAGAIN)
        {
            sftpfs_ssherror_to_gliberror (super_data, res, error);
            return -1;
        }

        sftpfs_waitsocket (super_data, error);
        if (error != NULL && *error != NULL)
            return -1;
    }
    while (res == LIBSSH2_ERROR_EAGAIN);

    return res;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Create symlink to file or directory
 *
 * @param vpath1 path to file or directory
 * @param vpath2 path to symlink
 * @param error  pointer to error object
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_symlink (const vfs_path_t * vpath1, const vfs_path_t * vpath2, GError ** error)
{
    struct vfs_s_super *super;
    sftpfs_super_data_t *super_data;
    const vfs_path_element_t *path_element1;
    const vfs_path_element_t *path_element2;
    char *tmp_path;
    int res;

    path_element2 = vfs_path_get_by_index (vpath2, -1);

    if (vfs_s_get_path (vpath2, &super, 0) == NULL)
        return -1;

    if (super == NULL)
        return -1;

    super_data = (sftpfs_super_data_t *) super->data;
    if (super_data->sftp_session == NULL)
        return -1;

    tmp_path = g_strdup_printf ("%c%s", PATH_SEP, path_element2->path);
    path_element1 = vfs_path_get_by_index (vpath1, -1);

    do
    {
        const char *fixfname;

        fixfname = sftpfs_fix_filename (path_element1->path);

        res =
            libssh2_sftp_symlink_ex (super_data->sftp_session,
                                     fixfname,
                                     sftpfs_filename_buffer->len, tmp_path, strlen (tmp_path),
                                     LIBSSH2_SFTP_SYMLINK);
        if (res >= 0)
            break;

        if (res != LIBSSH2_ERROR_EAGAIN)
        {
            sftpfs_ssherror_to_gliberror (super_data, res, error);
            g_free (tmp_path);
            return -1;
        }

        sftpfs_waitsocket (super_data, error);
        if (error != NULL && *error != NULL)
        {
            g_free (tmp_path);
            return -1;
        }
    }
    while (res == LIBSSH2_ERROR_EAGAIN);
    g_free (tmp_path);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Changes the permissions of the file.
 *
 * @param vpath path to file or directory
 * @param mode  mode (see man 2 open)
 * @param error pointer to error object
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_chmod (const vfs_path_t * vpath, mode_t mode, GError ** error)
{
    struct vfs_s_super *super;
    sftpfs_super_data_t *super_data;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int res;
    const vfs_path_element_t *path_element;

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

        fixfname = sftpfs_fix_filename (path_element->path);

        res = libssh2_sftp_stat_ex (super_data->sftp_session, fixfname,
                                    sftpfs_filename_buffer->len, LIBSSH2_SFTP_LSTAT, &attrs);
        if (res >= 0)
            break;

        if (res != LIBSSH2_ERROR_EAGAIN)
        {
            sftpfs_ssherror_to_gliberror (super_data, res, error);
            return -1;
        }

        sftpfs_waitsocket (super_data, error);
        if (error != NULL && *error != NULL)
            return -1;
    }
    while (res == LIBSSH2_ERROR_EAGAIN);

    attrs.permissions = mode;

    do
    {
        const char *fixfname;

        fixfname = sftpfs_fix_filename (path_element->path);

        res = libssh2_sftp_stat_ex (super_data->sftp_session, fixfname,
                                    sftpfs_filename_buffer->len, LIBSSH2_SFTP_SETSTAT, &attrs);
        if (res >= 0)
            break;
        else if (res != LIBSSH2_ERROR_EAGAIN)
        {
            sftpfs_ssherror_to_gliberror (super_data, res, error);
            return -1;
        }

        sftpfs_waitsocket (super_data, error);
        if (error != NULL && *error != NULL)
            return -1;
    }
    while (res == LIBSSH2_ERROR_EAGAIN);
    return res;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Delete a name from the file system.
 *
 * @param vpath path to file or directory
 * @param error pointer to error object
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_unlink (const vfs_path_t * vpath, GError ** error)
{
    struct vfs_s_super *super;
    sftpfs_super_data_t *super_data;
    int res;
    const vfs_path_element_t *path_element;

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

        fixfname = sftpfs_fix_filename (path_element->path);

        res =
            libssh2_sftp_unlink_ex (super_data->sftp_session, fixfname,
                                    sftpfs_filename_buffer->len);
        if (res >= 0)
            break;

        if (res != LIBSSH2_ERROR_EAGAIN)
        {
            sftpfs_ssherror_to_gliberror (super_data, res, error);
            return -1;
        }

        sftpfs_waitsocket (super_data, error);
        if (error != NULL && *error != NULL)
            return -1;
    }
    while (res == LIBSSH2_ERROR_EAGAIN);

    return res;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Rename a file, moving it between directories if required.
 *
 * @param vpath1 path to source file or directory
 * @param vpath2 path to destination file or directory
 * @param error  pointer to error object
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_rename (const vfs_path_t * vpath1, const vfs_path_t * vpath2, GError ** error)
{
    struct vfs_s_super *super;
    sftpfs_super_data_t *super_data;
    const vfs_path_element_t *path_element1;
    const vfs_path_element_t *path_element2;
    char *tmp_path;
    int res;

    path_element2 = vfs_path_get_by_index (vpath2, -1);

    if (vfs_s_get_path (vpath2, &super, 0) == NULL)
        return -1;

    if (super == NULL)
        return -1;

    super_data = (sftpfs_super_data_t *) super->data;
    if (super_data->sftp_session == NULL)
        return -1;

    tmp_path = g_strdup_printf ("%c%s", PATH_SEP, path_element2->path);
    path_element1 = vfs_path_get_by_index (vpath1, -1);

    do
    {
        const char *fixfname;

        fixfname = sftpfs_fix_filename (path_element1->path);

        res = libssh2_sftp_rename_ex
            (super_data->sftp_session,
             fixfname,
             sftpfs_filename_buffer->len, tmp_path, strlen (tmp_path), LIBSSH2_SFTP_SYMLINK);
        if (res >= 0)
            break;

        if (res != LIBSSH2_ERROR_EAGAIN)
        {
            sftpfs_ssherror_to_gliberror (super_data, res, error);
            g_free (tmp_path);
            return -1;
        }

        sftpfs_waitsocket (super_data, error);
        if (error != NULL && *error != NULL)
        {
            g_free (tmp_path);
            return -1;
        }
    }
    while (res == LIBSSH2_ERROR_EAGAIN);
    g_free (tmp_path);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

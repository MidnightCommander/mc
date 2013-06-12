/* Virtual File System: SFTP file system.
   The internal functions: files

   Copyright (C) 2011
   The Free Software Foundation, Inc.

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

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef struct
{
    LIBSSH2_SFTP_HANDLE *handle;
    int flags;
    mode_t mode;
} sftpfs_file_handler_data_t;

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Reopen file by file handle.
 *
 * @param file_handler the file handler data
 * @param error        pointer to the error handler
 */
static void
sftpfs_reopen (vfs_file_handler_t * file_handler, GError ** error)
{
    sftpfs_file_handler_data_t *file_handler_data;

    file_handler_data = (sftpfs_file_handler_data_t *) file_handler->data;

    sftpfs_close_file (file_handler, error);
    if (error == NULL || *error == NULL)
        sftpfs_open_file (file_handler, file_handler_data->flags, file_handler_data->mode, error);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Open new SFTP file.
 *
 * @param file_handler the file handler data
 * @param flags        flags (see man 2 open)
 * @param mode         mode (see man 2 open)
 * @param error        pointer to the error handler
 * @return TRUE if connection was created successfully, FALSE otherwise
 */

gboolean
sftpfs_open_file (vfs_file_handler_t * file_handler, int flags, mode_t mode, GError ** error)
{
    unsigned long sftp_open_flags = 0;
    int sftp_open_mode = 0;
    gboolean do_append = FALSE;
    sftpfs_file_handler_data_t *file_handler_data;
    sftpfs_super_data_t *super_data;
    char *name;

    (void) mode;

    name = vfs_s_fullpath (&sftpfs_class, file_handler->ino);
    if (name == NULL)
        return FALSE;

    super_data = (sftpfs_super_data_t *) file_handler->ino->super->data;
    file_handler_data = g_new0 (sftpfs_file_handler_data_t, 1);

    if ((flags & O_CREAT) != 0 || (flags & O_WRONLY) != 0)
    {
        sftp_open_flags = (flags & O_WRONLY) != 0 ? LIBSSH2_FXF_WRITE : 0;
        sftp_open_flags |= (flags & O_CREAT) != 0 ? LIBSSH2_FXF_CREAT : 0;
        if ((flags & O_APPEND) != 0)
        {
            sftp_open_flags |= LIBSSH2_FXF_APPEND;
            do_append = TRUE;
        }
        sftp_open_flags |= (flags & O_TRUNC) != 0 ? LIBSSH2_FXF_TRUNC : 0;

        sftp_open_mode = LIBSSH2_SFTP_S_IRUSR |
            LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH;
    }
    else
        sftp_open_flags = LIBSSH2_FXF_READ;

    while (TRUE)
    {
        int libssh_errno;

        file_handler_data->handle =
            libssh2_sftp_open (super_data->sftp_session, sftpfs_fix_filename (name),
                               sftp_open_flags, sftp_open_mode);

        if (file_handler_data->handle != NULL)
            break;

        libssh_errno = libssh2_session_last_errno (super_data->session);
        if (libssh_errno != LIBSSH2_ERROR_EAGAIN)
        {
            sftpfs_ssherror_to_gliberror (super_data, libssh_errno, error);
            g_free (name);
            return FALSE;
        }
    }

    g_free (name);

    file_handler_data->flags = flags;
    file_handler_data->mode = mode;
    file_handler->data = file_handler_data;

    if (do_append)
    {
        struct stat file_info;

        if (sftpfs_fstat (file_handler, &file_info, error) == 0)
            libssh2_sftp_seek64 (file_handler_data->handle, file_info.st_size);
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Stats the file specified by the file descriptor.
 *
 * @param data  file data handler
 * @param buf   buffer for store stat-info
 * @param error pointer to the error handler
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_fstat (void *data, struct stat *buf, GError ** error)
{
    int res;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    vfs_file_handler_t *fh = (vfs_file_handler_t *) data;
    sftpfs_file_handler_data_t *sftpfs_fh = fh->data;
    struct vfs_s_super *super = fh->ino->super;
    sftpfs_super_data_t *super_data = (sftpfs_super_data_t *) super->data;

    if (sftpfs_fh->handle == NULL)
        return -1;

    do
    {
        res = libssh2_sftp_fstat_ex (sftpfs_fh->handle, &attrs, 0);
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
 * Read up to 'count' bytes from the file descriptor 'file_handler' to the buffer starting at 'buffer'.
 *
 * @param file_handler file data handler
 * @param buffer buffer for data
 * @param count data size
 * @param error pointer to the error handler
 *
 * @return 0 on success, negative value otherwise
 */

ssize_t
sftpfs_read_file (vfs_file_handler_t * file_handler, char *buffer, size_t count, GError ** error)
{
    ssize_t rc;
    sftpfs_file_handler_data_t *file_handler_data;
    sftpfs_super_data_t *super_data;

    if (file_handler == NULL || file_handler->data == NULL)
    {
        g_set_error (error, MC_ERROR, -1, _("sftp: No file handler data present for reading file"));
        return -1;
    }

    file_handler_data = file_handler->data;
    super_data = (sftpfs_super_data_t *) file_handler->ino->super->data;

    do
    {
        rc = libssh2_sftp_read (file_handler_data->handle, buffer, count);
        if (rc >= 0)
            break;

        if (rc != LIBSSH2_ERROR_EAGAIN)
        {
            sftpfs_ssherror_to_gliberror (super_data, rc, error);
            return -1;
        }

        sftpfs_waitsocket (super_data, error);
        if (error != NULL && *error != NULL)
            return -1;
    }
    while (rc == LIBSSH2_ERROR_EAGAIN);

    file_handler->pos = (off_t) libssh2_sftp_tell64 (file_handler_data->handle);

    return rc;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Write up to 'count' bytes from  the buffer starting at 'buffer' to the descriptor 'file_handler'.
 *
 * @param file_handler file data handler
 * @param buffer       buffer for data
 * @param count        data size
 * @param error        pointer to the error handler
 *
 * @return 0 on success, negative value otherwise
 */

ssize_t
sftpfs_write_file (vfs_file_handler_t * file_handler, const char *buffer, size_t count,
                   GError ** error)
{
    ssize_t rc;
    sftpfs_file_handler_data_t *file_handler_data;
    sftpfs_super_data_t *super_data;

    file_handler_data = (sftpfs_file_handler_data_t *) file_handler->data;
    super_data = (sftpfs_super_data_t *) file_handler->ino->super->data;

    file_handler->pos = (off_t) libssh2_sftp_tell64 (file_handler_data->handle);

    do
    {
        rc = libssh2_sftp_write (file_handler_data->handle, buffer, count);
        if (rc >= 0)
            break;

        if (rc != LIBSSH2_ERROR_EAGAIN)
        {
            sftpfs_ssherror_to_gliberror (super_data, rc, error);
            return -1;
        }

        sftpfs_waitsocket (super_data, error);
        if (error != NULL && *error != NULL)
            return -1;
    }
    while (rc == LIBSSH2_ERROR_EAGAIN);

    return rc;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Close a file descriptor.
 *
 * @param file_handler    file data handler
 * @param error           pointer to the error handler
 *
 * @return 0 on success, negative value otherwise
 */

int
sftpfs_close_file (vfs_file_handler_t * file_handler, GError ** error)
{
    sftpfs_file_handler_data_t *file_handler_data;

    (void) error;

    file_handler_data = (sftpfs_file_handler_data_t *) file_handler->data;
    if (file_handler_data == NULL)
        return -1;

    libssh2_sftp_close (file_handler_data->handle);

    g_free (file_handler_data);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Reposition the offset of the open file associated with the file descriptor.
 *
 * @param file_handler   file data handler
 * @param offset         file offset
 * @param whence         method of seek (at begin, at current, at end)
 * @param error          pointer to the error handler
 *
 * @return 0 on success, negative value otherwise
 */

off_t
sftpfs_lseek (vfs_file_handler_t * file_handler, off_t offset, int whence, GError ** error)
{
    sftpfs_file_handler_data_t *file_handler_data;

    file_handler_data = (sftpfs_file_handler_data_t *) file_handler->data;

    switch (whence)
    {
    case SEEK_SET:
        /* Need reopen file because:
           "You MUST NOT seek during writing or reading a file with SFTP, as the internals use
           outstanding packets and changing the "file position" during transit will results in
           badness." */
        if (file_handler->pos > offset || offset == 0)
        {
            sftpfs_reopen (file_handler, error);
            if (error != NULL && *error != NULL)
                return 0;
        }
        file_handler->pos = offset;
        break;
    case SEEK_CUR:
        file_handler->pos += offset;
        break;
    case SEEK_END:
        if (file_handler->pos > file_handler->ino->st.st_size - offset)
        {
            sftpfs_reopen (file_handler, error);
            if (error != NULL && *error != NULL)
                return 0;
        }
        file_handler->pos = file_handler->ino->st.st_size - offset;
        break;
    }

    libssh2_sftp_seek64 (file_handler_data->handle, file_handler->pos);
    file_handler->pos = (off_t) libssh2_sftp_tell64 (file_handler_data->handle);

    return file_handler->pos;
}

/* --------------------------------------------------------------------------------------------- */

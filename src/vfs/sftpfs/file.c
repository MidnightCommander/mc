/* Virtual File System: SFTP file system.
   The internal functions: files

   Copyright (C) 2011-2024
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

#include <errno.h>              /* ENOENT, EACCES */

#include <libssh2.h>
#include <libssh2_sftp.h>

#include "lib/global.h"
#include "lib/util.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define SFTP_FILE_HANDLER(a) ((sftpfs_file_handler_t *) a)

/*** file scope type declarations ****************************************************************/

typedef struct
{
    vfs_file_handler_t base;    /* base class */

    LIBSSH2_SFTP_HANDLE *handle;
    int flags;
    mode_t mode;
} sftpfs_file_handler_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Reopen file by file handle.
 *
 * @param fh      the file handler
 * @param mcerror pointer to the error handler
 */
static void
sftpfs_reopen (vfs_file_handler_t * fh, GError ** mcerror)
{
    sftpfs_file_handler_t *file = SFTP_FILE_HANDLER (fh);
    int flags;
    mode_t mode;

    g_return_if_fail (mcerror == NULL || *mcerror == NULL);

    flags = file->flags;
    mode = file->mode;

    sftpfs_close_file (fh, mcerror);
    sftpfs_open_file (fh, flags, mode, mcerror);
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_file__handle_error (sftpfs_super_t * super, int sftp_res, GError ** mcerror)
{
    if (sftpfs_is_sftp_error (super->sftp_session, sftp_res, LIBSSH2_FX_PERMISSION_DENIED))
        return -EACCES;

    if (sftpfs_is_sftp_error (super->sftp_session, sftp_res, LIBSSH2_FX_NO_SUCH_FILE))
        return -ENOENT;

    if (!sftpfs_waitsocket (super, sftp_res, mcerror))
        return -1;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

vfs_file_handler_t *
sftpfs_fh_new (struct vfs_s_inode * ino, gboolean changed)
{
    sftpfs_file_handler_t *fh;

    fh = g_new0 (sftpfs_file_handler_t, 1);
    vfs_s_init_fh (VFS_FILE_HANDLER (fh), ino, changed);

    return VFS_FILE_HANDLER (fh);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Open new SFTP file.
 *
 * @param fh      the file handler
 * @param flags   flags (see man 2 open)
 * @param mode    mode (see man 2 open)
 * @param mcerror pointer to the error handler
 * @return TRUE if connection was created successfully, FALSE otherwise
 */

gboolean
sftpfs_open_file (vfs_file_handler_t * fh, int flags, mode_t mode, GError ** mcerror)
{
    unsigned long sftp_open_flags = 0;
    int sftp_open_mode = 0;
    gboolean do_append = FALSE;
    sftpfs_file_handler_t *file = SFTP_FILE_HANDLER (fh);
    sftpfs_super_t *super = SFTP_SUPER (fh->ino->super);
    char *name;
    const GString *fixfname;

    (void) mode;
    mc_return_val_if_error (mcerror, FALSE);

    name = vfs_s_fullpath (vfs_sftpfs_ops, fh->ino);
    if (name == NULL)
        return FALSE;

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

    fixfname = sftpfs_fix_filename (name);

    while (TRUE)
    {
        int libssh_errno;

        file->handle =
            libssh2_sftp_open_ex (super->sftp_session, fixfname->str, fixfname->len,
                                  sftp_open_flags, sftp_open_mode, LIBSSH2_SFTP_OPENFILE);
        if (file->handle != NULL)
            break;

        libssh_errno = libssh2_session_last_errno (super->session);
        if (libssh_errno != LIBSSH2_ERROR_EAGAIN)
        {
            sftpfs_ssherror_to_gliberror (super, libssh_errno, mcerror);
            g_free (name);
            return FALSE;
        }
    }

    g_free (name);

    file->flags = flags;
    file->mode = mode;

    if (do_append)
    {
        struct stat file_info = {
            .st_dev = 0
        };
        /* In case of

           struct stat file_info = { 0 };

           gcc < 4.7 [1] generates the following:

           error: missing initializer [-Werror=missing-field-initializers]
           error: (near initialization for 'file_info.st_dev') [-Werror=missing-field-initializers]

           [1] http://stackoverflow.com/questions/13373695/how-to-remove-the-warning-in-gcc-4-6-missing-initializer-wmissing-field-initi/27461062#27461062
         */

        if (sftpfs_fstat (fh, &file_info, mcerror) == 0)
            libssh2_sftp_seek64 (file->handle, file_info.st_size);
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Stats the file specified by the file descriptor.
 *
 * @param data    file handler
 * @param buf     buffer for store stat-info
 * @param mcerror pointer to the error handler
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_fstat (void *data, struct stat *buf, GError ** mcerror)
{
    int res;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    vfs_file_handler_t *fh = VFS_FILE_HANDLER (data);
    sftpfs_file_handler_t *sftpfs_fh = (sftpfs_file_handler_t *) data;
    struct vfs_s_super *super = VFS_FILE_HANDLER_SUPER (fh);
    sftpfs_super_t *sftpfs_super = SFTP_SUPER (super);

    mc_return_val_if_error (mcerror, -1);

    if (sftpfs_fh->handle == NULL)
        return -1;

    do
    {
        int err;

        res = libssh2_sftp_fstat_ex (sftpfs_fh->handle, &attrs, 0);
        if (res >= 0)
            break;

        err = sftpfs_file__handle_error (sftpfs_super, res, mcerror);
        if (err < 0)
            return err;
    }
    while (res == LIBSSH2_ERROR_EAGAIN);

    sftpfs_attr_to_stat (&attrs, buf);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Read up to 'count' bytes from the file descriptor 'fh' to the buffer starting at 'buffer'.
 *
 * @param fh      file handler
 * @param buffer  buffer for data
 * @param count   data size
 * @param mcerror pointer to the error handler
 *
 * @return 0 on success, negative value otherwise
 */

ssize_t
sftpfs_read_file (vfs_file_handler_t * fh, char *buffer, size_t count, GError ** mcerror)
{
    ssize_t rc;
    sftpfs_file_handler_t *file = SFTP_FILE_HANDLER (fh);
    sftpfs_super_t *super;

    mc_return_val_if_error (mcerror, -1);

    if (fh == NULL)
    {
        mc_propagate_error (mcerror, 0, "%s",
                            _("sftp: No file handler data present for reading file"));
        return -1;
    }

    super = SFTP_SUPER (VFS_FILE_HANDLER_SUPER (fh));

    do
    {
        int err;

        rc = libssh2_sftp_read (file->handle, buffer, count);
        if (rc >= 0)
            break;

        err = sftpfs_file__handle_error (super, (int) rc, mcerror);
        if (err < 0)
            return err;
    }
    while (rc == LIBSSH2_ERROR_EAGAIN);

    fh->pos = (off_t) libssh2_sftp_tell64 (file->handle);

    return rc;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Write up to 'count' bytes from  the buffer starting at 'buffer' to the descriptor 'fh'.
 *
 * @param fh      file handler
 * @param buffer  buffer for data
 * @param count   data size
 * @param mcerror pointer to the error handler
 *
 * @return 0 on success, negative value otherwise
 */

ssize_t
sftpfs_write_file (vfs_file_handler_t * fh, const char *buffer, size_t count, GError ** mcerror)
{
    ssize_t rc;
    sftpfs_file_handler_t *file = SFTP_FILE_HANDLER (fh);
    sftpfs_super_t *super = SFTP_SUPER (VFS_FILE_HANDLER_SUPER (fh));

    mc_return_val_if_error (mcerror, -1);

    fh->pos = (off_t) libssh2_sftp_tell64 (file->handle);

    do
    {
        int err;

        rc = libssh2_sftp_write (file->handle, buffer, count);
        if (rc >= 0)
            break;

        err = sftpfs_file__handle_error (super, (int) rc, mcerror);
        if (err < 0)
            return err;
    }
    while (rc == LIBSSH2_ERROR_EAGAIN);

    return rc;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Close a file descriptor.
 *
 * @param fh      file handler
 * @param mcerror pointer to the error handler
 *
 * @return 0 on success, negative value otherwise
 */

int
sftpfs_close_file (vfs_file_handler_t * fh, GError ** mcerror)
{
    int ret;

    mc_return_val_if_error (mcerror, -1);

    ret = libssh2_sftp_close (SFTP_FILE_HANDLER (fh)->handle);

    return ret == 0 ? 0 : -1;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Reposition the offset of the open file associated with the file descriptor.
 *
 * @param fh      file handler
 * @param offset  file offset
 * @param whence  method of seek (at begin, at current, at end)
 * @param mcerror pointer to the error handler
 *
 * @return 0 on success, negative value otherwise
 */

off_t
sftpfs_lseek (vfs_file_handler_t * fh, off_t offset, int whence, GError ** mcerror)
{
    sftpfs_file_handler_t *file = SFTP_FILE_HANDLER (fh);

    mc_return_val_if_error (mcerror, 0);

    switch (whence)
    {
    case SEEK_SET:
        /* Need reopen file because:
           "You MUST NOT seek during writing or reading a file with SFTP, as the internals use
           outstanding packets and changing the "file position" during transit will results in
           badness." */
        if (fh->pos > offset || offset == 0)
        {
            sftpfs_reopen (fh, mcerror);
            mc_return_val_if_error (mcerror, 0);
        }
        fh->pos = offset;
        break;
    case SEEK_CUR:
        fh->pos += offset;
        break;
    case SEEK_END:
        if (fh->pos > fh->ino->st.st_size - offset)
        {
            sftpfs_reopen (fh, mcerror);
            mc_return_val_if_error (mcerror, 0);
        }
        fh->pos = fh->ino->st.st_size - offset;
        break;
    default:
        break;
    }

    libssh2_sftp_seek64 (file->handle, fh->pos);
    fh->pos = (off_t) libssh2_sftp_tell64 (file->handle);

    return fh->pos;
}

/* --------------------------------------------------------------------------------------------- */

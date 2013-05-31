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
#include <errno.h>

#include "lib/global.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Open file.
 *
 * @param file_handler the file handler data
 * @param flags        flags (see man 2 open)
 * @param mode         mode (see man 2 open)
 * @param error        pointer to the error handler
 * @return TRUE if connection was created successfully, FALSE otherwise
 */

gboolean
smbfs_file_open (vfs_file_handler_t * file_handler, const vfs_path_t * vpath, int flags,
                 mode_t mode, GError ** error)
{
    const vfs_path_element_t *path_element;
    char *smb_url;

    path_element = vfs_path_get_by_index (vpath, -1);
    smb_url = smbfs_make_url (path_element, TRUE);

    errno = 0;
    file_handler->handle = smbc_open (smb_url, flags, mode);
    g_free (smb_url);

    if (file_handler->handle < 0)
        g_set_error (error, MC_ERROR, errno, "%s", smbfs_strerror (errno));

    return (file_handler->handle >= 0);
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
 * @return 0 on sucess, negative value otherwise
 */

ssize_t
smbfs_file_read (vfs_file_handler_t * file_handler, char *buffer, size_t count, GError ** error)
{
    ssize_t rc;

    if (file_handler == NULL)
    {
        g_set_error (error, MC_ERROR, -1,
                     _("smbfs: No file handler data present for reading file"));
        return -1;
    }

    errno = 0;
    smbfs_file_lseek (file_handler, file_handler->pos, SEEK_SET, NULL);
    rc = smbc_read (file_handler->handle, buffer, count);
    if (rc < 0)
        g_set_error (error, MC_ERROR, errno, "%s", smbfs_strerror (errno));
    else
        file_handler->pos += rc;

    return rc;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Close a file descriptor.
 *
 * @param file_handler    file data handler
 * @param error           pointer to the error handler
 *
 * @return 0 on sucess, negative value otherwise
 */

int
smbfs_file_close (vfs_file_handler_t * file_handler, GError ** error)
{
    int rc;

    if (file_handler == NULL)
    {
        g_set_error (error, MC_ERROR, -1,
                     _("smbfs: No file handler data present for closing file"));
        return -1;
    }

    errno = 0;
    rc = smbc_close (file_handler->handle);
    if (rc < 0)
        g_set_error (error, MC_ERROR, errno, "%s", smbfs_strerror (errno));

    return rc;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Stats the file specified by the file descriptor.
 *
 * @param data  file data handler
 * @param buf   buffer for store stat-info
 * @param error pointer to the error handler
 * @return 0 if sucess, negative value otherwise
 */

int
smbfs_file_stat (vfs_file_handler_t * file_handler, struct stat *buf, GError ** error)
{
    int rc;

    if (file_handler == NULL)
    {
        g_set_error (error, MC_ERROR, -1, _("smbfs: No file handler data present for fstat file"));
        return -1;
    }
    errno = 0;
    rc = smbc_fstat (file_handler->handle, buf);

    if (rc < 0)
        g_set_error (error, MC_ERROR, errno, "%s", smbfs_strerror (errno));

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
 * @return 0 on sucess, negative value otherwise
 */

ssize_t
smbfs_file_write (vfs_file_handler_t * file_handler, const char *buffer, size_t count,
                  GError ** error)
{
    ssize_t rc;

    if (file_handler == NULL)
    {
        g_set_error (error, MC_ERROR, -1, _("smbfs: No file handler data present for fstat file"));
        return -1;
    }
    errno = 0;
    rc = smbc_write (file_handler->handle, buffer, count);

    if (rc < 0)
        g_set_error (error, MC_ERROR, errno, "%s", smbfs_strerror (errno));

    return rc;
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
 * @return 0 on sucess, negative value otherwise
 */
off_t
smbfs_file_lseek (vfs_file_handler_t * file_handler, off_t offset, int whence, GError ** error)
{
    off_t rc;

    if (file_handler == NULL)
    {
        g_set_error (error, MC_ERROR, -1, _("smbfs: No file handler data present for fstat file"));
        return -1;
    }
    errno = 0;
    rc = smbc_lseek (file_handler->handle, offset, whence);

    if (rc == -1)
        g_set_error (error, MC_ERROR, errno, "%s", smbfs_strerror (errno));
    else
        file_handler->pos = rc;

    return rc;
}

/* --------------------------------------------------------------------------------------------- */

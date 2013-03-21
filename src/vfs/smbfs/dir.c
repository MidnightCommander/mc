/* Virtual File System: Samba file system.
   The internal functions: dirs

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

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef struct
{
    int handle;
    smbfs_super_data_t *super_data;
} smbfs_dir_data_t;

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Open a directory stream corresponding to the directory name.
 *
 * @param vpath path to directory
 * @param error pointer to the error handler
 * @return directory data handler if success, NULL otherwise
 */

void *
smbfs_opendir (const vfs_path_t * vpath, GError ** error)
{
    smbfs_dir_data_t *smbfs_dir = NULL;
    struct vfs_s_super *super;
    smbfs_super_data_t *super_data;
    const vfs_path_element_t *path_element;
    int handle;
    char *smb_url;

    super = vfs_get_super_by_vpath (vpath, TRUE);
    if (super == NULL)
        return NULL;

    path_element = vfs_path_get_by_index (vpath, -1);

    super_data = (smbfs_super_data_t *) super->data;

    smb_url = smbfs_make_url (path_element, TRUE);
    errno = 0;
    handle = smbc_opendir (smb_url);
    g_free (smb_url);

    if (handle < 0)
        g_set_error (error, MC_ERROR, errno, "%s", smbfs_strerror (errno));
    else
    {
        smbfs_dir = g_new0 (smbfs_dir_data_t, 1);
        smbfs_dir->handle = handle;
        smbfs_dir->super_data = super_data;
    }

    return (void *) smbfs_dir;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Get a pointer to a structure representing the next directory entry.
 *
 * @param data  directory data handler
 * @param error pointer to the error handler
 * @return information about direntry if success, NULL otherwise
 */

void *
smbfs_readdir (void *data, GError ** error)
{
    struct smbc_dirent *smb_direntry;
    smbfs_dir_data_t *smbfs_dir = (smbfs_dir_data_t *) data;

    static union vfs_dirent smbfs_dirent;

    errno = 0;
    smb_direntry = smbc_readdir (smbfs_dir->handle);
    if (smb_direntry == NULL)
    {
        if (errno != 0)
            g_set_error (error, MC_ERROR, errno, "%s", smbfs_strerror (errno));
        return NULL;
    }

    g_strlcpy (smbfs_dirent.dent.d_name, smb_direntry->name, BUF_MEDIUM);
    compute_namelen (&smbfs_dirent.dent);
    return &smbfs_dirent;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Close the directory stream.
 *
 * @param data  directory data handler
 * @param error pointer to the error handler
 * @return 0 if sucess, negative value otherwise
 */

int
smbfs_closedir (void *data, GError ** error)
{
    int return_code;
    smbfs_dir_data_t *smbfs_dir = (smbfs_dir_data_t *) data;

    errno = 0;
    return_code = smbc_closedir (smbfs_dir->handle);

    if (return_code < 0)
        g_set_error (error, MC_ERROR, errno, "%s", smbfs_strerror (errno));

    g_free (smbfs_dir);
    return return_code;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Create a new directory.
 *
 * @param vpath path directory
 * @param mode  mode (see man 2 open)
 * @param error pointer to the error handler
 * @return 0 if sucess, negative value otherwise
 */

int
smbfs_mkdir (const vfs_path_t * vpath, mode_t mode, GError ** error)
{
    int return_code;
    char *smb_url;
    const vfs_path_element_t *path_element;

    path_element = vfs_path_get_by_index (vpath, -1);
    smb_url = smbfs_make_url (path_element, TRUE);
    errno = 0;
    return_code = smbc_mkdir (smb_url, mode);
    g_free (smb_url);

    if (return_code != 0)
        g_set_error (error, MC_ERROR, errno, "%s", smbfs_strerror (errno));

    return return_code;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Remove a directory.
 *
 * @param vpath path directory
 * @param error pointer to the error handler
 * @return 0 if sucess, negative value otherwise
 */

int
smbfs_rmdir (const vfs_path_t * vpath, GError ** error)
{
    int return_code;
    char *smb_url;
    const vfs_path_element_t *path_element;

    path_element = vfs_path_get_by_index (vpath, -1);
    smb_url = smbfs_make_url (path_element, TRUE);
    errno = 0;
    return_code = smbc_rmdir (smb_url);
    g_free (smb_url);

    if (return_code != 0)
        g_set_error (error, MC_ERROR, errno, "%s", smbfs_strerror (errno));

    return return_code;
}

/* --------------------------------------------------------------------------------------------- */

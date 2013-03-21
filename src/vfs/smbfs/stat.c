/* Virtual File System: SFTP file system.
   The stat functions

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

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Getting information about a symbolic link.
 *
 * @param vpath path to file, directory or symbolic link
 * @param buf   buffer for store stat-info
 * @param error pointer to error object
 * @return 0 if sucess, negative value otherwise
 */

int
smbfs_lstat (const vfs_path_t * vpath, struct stat *buf, GError ** error)
{
    int return_code;
    char *smb_url;
    const vfs_path_element_t *path_element;

    path_element = vfs_path_get_by_index (vpath, -1);
    smb_url = smbfs_make_url (path_element, TRUE);

    errno = 0;
    return_code = smbc_stat (smb_url, buf);

    if (return_code != 0)
        g_set_error (error, MC_ERROR, errno, "%s", smbfs_strerror (errno));

    g_free (smb_url);
    return return_code;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Getting information about a file or directory.
 *
 * @param vpath path to file or directory
 * @param buf   buffer for store stat-info
 * @param error pointer to error object
 * @return 0 if sucess, negative value otherwise
 */

int
smbfs_stat (const vfs_path_t * vpath, struct stat *buf, GError ** error)
{
    return smbfs_lstat (vpath, buf, error);
}

/* --------------------------------------------------------------------------------------------- */

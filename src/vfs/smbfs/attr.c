/* Virtual File System: SFTP file system.
   The internal functions: attributes

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
 * Changes the permissions of the file.
 *
 * @param vpath path to file or directory
 * @param mode  mode (see man 2 open)
 * @param error pointer to error object
 * @return 0 if sucess, negative value otherwise
 */

int
smbfs_attr_chmod (const vfs_path_t * vpath, mode_t mode, GError ** error)
{
    int rc;
    char *smb_url;
    const vfs_path_element_t *path_element;

    path_element = vfs_path_get_by_index (vpath, -1);
    smb_url = smbfs_make_url (path_element, TRUE);
    errno = 0;
    rc = smbc_chmod (smb_url, mode);
    g_free (smb_url);

    if (rc < 0)
        g_set_error (error, MC_ERROR, errno, "%s", smbfs_strerror (errno));

    return rc;
}

/* --------------------------------------------------------------------------------------------- */

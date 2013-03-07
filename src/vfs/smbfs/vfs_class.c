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
    (void) me;

    if (smbc_init (smbfs_cb_authdata_provider, 0))
    {
        fprintf (stderr, "smbc_init returned %s (%i)\n", strerror (errno), errno);
        return FALSE;
    }

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
}

/* --------------------------------------------------------------------------------------------- */

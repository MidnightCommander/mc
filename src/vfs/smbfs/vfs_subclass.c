/* Virtual File System: Samba file system.
   The VFS subclass functions

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

#include "lib/global.h"

#include "internal.h"

/*** global variables ****************************************************************************/

struct vfs_s_subclass smbfs_subclass;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Initialization of VFS subclass structure.
 *
 * @return VFS subclass structure.
 */

void
smbfs_init_subclass (void)
{
    memset (&smbfs_subclass, 0, sizeof (struct vfs_s_subclass));
    smbfs_subclass.flags = VFS_S_REMOTE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Initialization of VFS subclass callbacks.
 */

void
smbfs_init_subclass_callbacks (void)
{

}

/* --------------------------------------------------------------------------------------------- */

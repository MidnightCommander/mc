/* Virtual File System: Samba file system.
   The interface function

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

#include "lib/vfs/netutil.h"

#include "internal.h"

#include "init.h"

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
 * Initialization of Samba Virtual File Sysytem.
 */

void
init_smbfs (void)
{
    tcp_init ();

    smbfs_init_class ();
    smbfs_init_subclass ();

    vfs_s_init_class (&smbfs_class, &smbfs_subclass);

    smbfs_init_class_callbacks ();
    smbfs_init_subclass_callbacks ();

    vfs_register_class (&smbfs_class);
}

/* --------------------------------------------------------------------------------------------- */

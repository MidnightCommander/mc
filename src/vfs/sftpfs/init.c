/* Virtual File System: SFTP file system.
   The interface function

   Copyright (C) 2011-2020
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

#include "lib/global.h"
#include "lib/vfs/netutil.h"

#include "init.h"
#include "internal.h"

/*** global variables ****************************************************************************/

struct vfs_s_subclass sftpfs_subclass;
struct vfs_class *sftpfs_class = VFS_CLASS (&sftpfs_subclass);

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Initialization of SFTP Virtual File Sysytem.
 */

void
vfs_init_sftpfs (void)
{
    tcp_init ();

    vfs_init_subclass (&sftpfs_subclass, "sftpfs", VFSF_NOLINKS | VFSF_REMOTE, "sftp");
    sftpfs_init_class ();
    sftpfs_init_subclass ();
    vfs_register_class (sftpfs_class);
}

/* --------------------------------------------------------------------------------------------- */

/*
   Init VFS plugins.

   Copyright (C) 2011-2024
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011.

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

/** \file
 *  \brief This is a template file (here goes brief description).
 *  \author Author1
 *  \author Author2
 *  \date 20xx
 *
 *  Detailed description.
 */

#include <config.h>

#include "lib/global.h"

#include "local/local.h"

#ifdef ENABLE_VFS_CPIO
#include "cpio/cpio.h"
#endif

#ifdef ENABLE_VFS_EXTFS
#include "extfs/extfs.h"
#endif

#ifdef ENABLE_VFS_SHELL
#include "shell/shell.h"
#endif

#ifdef ENABLE_VFS_FTP
#include "ftpfs/ftpfs.h"
#endif

#ifdef ENABLE_VFS_SFTP
#include "sftpfs/sftpfs.h"
#endif

#ifdef ENABLE_VFS_SFS
#include "sfs/sfs.h"
#endif

#ifdef ENABLE_VFS_TAR
#include "tar/tar.h"
#endif

#ifdef ENABLE_VFS_UNDELFS
#include "undelfs/undelfs.h"
#endif

#include "plugins_init.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
vfs_plugins_init (void)
{
    /* localfs needs to be the first one */
    vfs_init_localfs ();

#ifdef ENABLE_VFS_CPIO
    vfs_init_cpiofs ();
#endif /* ENABLE_VFS_CPIO */
#ifdef ENABLE_VFS_TAR
    vfs_init_tarfs ();
#endif /* ENABLE_VFS_TAR */
#ifdef ENABLE_VFS_SFS
    vfs_init_sfs ();
#endif /* ENABLE_VFS_SFS */
#ifdef ENABLE_VFS_EXTFS
    vfs_init_extfs ();
#endif /* ENABLE_VFS_EXTFS */
#ifdef ENABLE_VFS_UNDELFS
    vfs_init_undelfs ();
#endif /* ENABLE_VFS_UNDELFS */

#ifdef ENABLE_VFS_FTP
    vfs_init_ftpfs ();
#endif /* ENABLE_VFS_FTP */
#ifdef ENABLE_VFS_SFTP
    vfs_init_sftpfs ();
#endif /* ENABLE_VFS_SFTP */
#ifdef ENABLE_VFS_SHELL
    vfs_init_shell ();
#endif /* ENABLE_VFS_SHELL */
}

/* --------------------------------------------------------------------------------------------- */

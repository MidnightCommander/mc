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
#include <errno.h>

#include "lib/global.h"
#include "lib/vfs/utilvfs.h"

#include "internal.h"

/*** global variables ****************************************************************************/

struct vfs_s_subclass smbfs_subclass;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for checking if connection is equal to existing connection.
 *
 * @param vpath_element path element with connetion data
 * @param super         data with exists connection
 * @param vpath         unused
 * @param cookie        unused
 * @return TRUE if connections is equal, FALSE otherwise
 */

static gboolean
smbfs_cb_is_equal_connection (const vfs_path_element_t * vpath_element, struct vfs_s_super *super,
                              const vfs_path_t * vpath, void *cookie)
{
    gboolean result = TRUE;
    char *url1, *url2;

    (void) vpath;
    (void) cookie;

    url1 = smbfs_make_url (vpath_element, FALSE);
    url2 = smbfs_make_url (super->path_element, FALSE);
    result = (strcmp (url1, url2) == 0);
    g_free (url2);
    g_free (url1);

    return result;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for opening new connection.
 *
 * @param super         connection data
 * @param vpath         unused
 * @param vpath_element path element with connetion data
 * @return 0 if success, -1 otherwise
 */

static int
smbfs_cb_open_connection (struct vfs_s_super *super,
                          const vfs_path_t * vpath, const vfs_path_element_t * vpath_element)
{
    smbfs_super_data_t *smbfs_super_data;

    (void) vpath;

    smbfs_super_data = g_new0 (smbfs_super_data_t, 1);

    super->data = (void *) smbfs_super_data;

    super->path_element = vfs_path_element_clone (vpath_element);

    super->name = g_strdup (PATH_SEP_STR);
    super->root =
        vfs_s_new_inode (vpath_element->class, super,
                         vfs_s_default_stat (vpath_element->class, S_IFDIR | 0755));

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for closing connection.
 *
 * @param me    unused
 * @param super connection data
 */

static void
smbfs_cb_close_connection (struct vfs_class *me, struct vfs_s_super *super)
{
    GError *error = NULL;

    (void) me;
    vfs_show_gerror (&error);
    g_free (super->data);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for getting directory content.
 *
 * @param me          unused
 * @param dir         unused
 * @param remote_path unused
 * @return always 0
 */

static int
smbfs_cb_dir_load (struct vfs_class *me, struct vfs_s_inode *dir, char *remote_path)
{
    (void) me;
    (void) dir;
    (void) remote_path;

    return 0;
}

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
    smbfs_subclass.archive_same = smbfs_cb_is_equal_connection;
    smbfs_subclass.open_archive = smbfs_cb_open_connection;
    smbfs_subclass.free_archive = smbfs_cb_close_connection;
    smbfs_subclass.dir_load = smbfs_cb_dir_load;
}

/* --------------------------------------------------------------------------------------------- */

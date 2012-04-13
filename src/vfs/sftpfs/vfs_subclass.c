/* Virtual File System: SFTP file system.
   The VFS subclass functions

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
#include "lib/vfs/utilvfs.h"

#include "internal.h"

/*** global variables ****************************************************************************/

struct vfs_s_subclass sftpfs_subclass;

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
sftpfs_cb_is_equal_connection (const vfs_path_element_t * vpath_element, struct vfs_s_super *super,
                               const vfs_path_t * vpath, void *cookie)
{
    int port;
    char *user_name;
    int result;

    (void) vpath;
    (void) cookie;

    if (vpath_element->user != NULL)
        user_name = vpath_element->user;
    else
        user_name = vfs_get_local_username ();

    if (vpath_element->port != 0)
        port = vpath_element->port;
    else
        port = SFTP_DEFAULT_PORT;

    result = ((strcmp (vpath_element->host, super->path_element->host) == 0)
              && (strcmp (user_name, super->path_element->user) == 0)
              && (port == super->path_element->port));

    if (user_name != vpath_element->user)
        g_free (user_name);

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
sftpfs_cb_open_connection (struct vfs_s_super *super,
                           const vfs_path_t * vpath, const vfs_path_element_t * vpath_element)
{
    GError *error = NULL;
    int ret_value;

    (void) vpath;

    if (vpath_element->host == NULL || *vpath_element->host == '\0')
    {
        vfs_print_message (_("sftp: Invalid host name."));
        vpath_element->class->verrno = EPERM;
        return -1;
    }

    super->data = g_new0 (sftpfs_super_data_t, 1);
    super->path_element = vfs_path_element_clone (vpath_element);

    sftpfs_fill_connection_data_from_config (super, &error);
    if (error != NULL)
    {
        vpath_element->class->verrno = error->code;
        sftpfs_show_error (&error);
        return -1;
    }

    super->name = g_strdup (PATH_SEP_STR);
    super->root =
        vfs_s_new_inode (vpath_element->class, super,
                         vfs_s_default_stat (vpath_element->class, S_IFDIR | 0755));

    ret_value = sftpfs_open_connection (super, &error);
    sftpfs_show_error (&error);
    return ret_value;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for closing connection.
 *
 * @param me    unused
 * @param super connection data
 */

static void
sftpfs_cb_close_connection (struct vfs_class *me, struct vfs_s_super *super)
{
    GError *error = NULL;

    (void) me;
    sftpfs_close_connection (super, "Normal Shutdown", &error);
    sftpfs_show_error (&error);
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
sftpfs_cb_dir_load (struct vfs_class *me, struct vfs_s_inode *dir, char *remote_path)
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
sftpfs_init_subclass (void)
{
    memset (&sftpfs_subclass, 0, sizeof (struct vfs_s_subclass));
    sftpfs_subclass.flags = VFS_S_REMOTE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Initialization of VFS subclass callbacks.
 */

void
sftpfs_init_subclass_callbacks (void)
{
    sftpfs_subclass.archive_same = sftpfs_cb_is_equal_connection;
    sftpfs_subclass.open_archive = sftpfs_cb_open_connection;
    sftpfs_subclass.free_archive = sftpfs_cb_close_connection;
    sftpfs_subclass.dir_load = sftpfs_cb_dir_load;
}

/* --------------------------------------------------------------------------------------------- */

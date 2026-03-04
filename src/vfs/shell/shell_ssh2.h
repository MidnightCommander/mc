/**
 * \file
 * \brief Header: Shell VFS: libssh2 transport layer
 */

#ifndef MC__VFS_SHELL_SSH2_H
#define MC__VFS_SHELL_SSH2_H

#ifdef ENABLE_SHELL_SSH2

#include <libssh2.h>

#include "lib/vfs/xdirentry.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    int socket_fd;
    LIBSSH2_SESSION *session;
    LIBSSH2_CHANNEL *channel;
    LIBSSH2_KNOWNHOSTS *known_hosts;
    char *known_hosts_file;
    LIBSSH2_AGENT *agent;
} shell_ssh2_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

shell_ssh2_t *shell_ssh2_open (struct vfs_s_super *super, GError **mcerror);
void shell_ssh2_close (shell_ssh2_t *ssh2);
ssize_t shell_ssh2_read (shell_ssh2_t *ssh2, void *buf, size_t len);
ssize_t shell_ssh2_write (shell_ssh2_t *ssh2, const void *buf, size_t len);

/*** inline functions ****************************************************************************/

#endif /* ENABLE_SHELL_SSH2 */

#endif /* MC__VFS_SHELL_SSH2_H */

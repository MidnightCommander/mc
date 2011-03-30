/* Dialog boxes for sftpfs.

   Copyright (C)
   2011 Free Software Foundation, Inc.

   Authors:
   2011 Maslakov Ilia

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/** \file dialogs.c
 *  \brief Source: Dialog boxes for the SFTP
 */

#include <config.h>
#include <stdlib.h>

#include "lib/global.h"
#include "lib/mcconfig.h"       /* Load/save user formats */
#include "lib/widget.h"

#include "sftpfs.h"
#include "dialogs.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define VFSX 60
#define VFSY 19

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static const char *
sftpfs_prepare_buffers (const char *from, char *to, size_t to_len)
{
    if (from != NULL)
        g_strlcpy (to, from, to_len);
    else
        *to = '\0';
    return to;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
void
configure_sftpfs (void)
{
    return;
}

/* --------------------------------------------------------------------------------------------- */

void
configure_sftpfs_conn (const char *sftpfs_sessionname)
{
    char buffer1[BUF_TINY] = "\0";      /* port */
    char buffer2[BUF_1K] = "\0";        /* host */
    char buffer3[BUF_4K] = "\0";        /* private key */
    char buffer4[BUF_4K] = "\0";        /* public key */
    char buffer6[BUF_1K] = "\0";        /* user name */

    char *tmp_pubkey = NULL;
    char *tmp_privkey = NULL;
    char *tmp_sftp_host = NULL;
    char *tmp_sftp_port = NULL;
    int tmp_auth_method = sftpfs_auth_method;
    char *tmp_username = NULL;
    char *tmp_pre_port = g_strdup_printf ("%i", sftpfs_port);

    const char *auth_names[] = {
        N_("Password"),
        N_("Publickey"),
        N_("SSH-Agent"),
    };

    QuickWidget confvfs_widgets[] = {
        /*  0 */ QUICK_BUTTON (30, VFSX, VFSY - 3, VFSY, N_("&Cancel"), B_CANCEL, NULL),
        /*  1 */ QUICK_BUTTON (12, VFSX, VFSY - 3, VFSY, N_("&Save"), B_EXIT, NULL),
        /*  2 */ QUICK_INPUT (4, VFSX, 13, VFSY,
                              sftpfs_prepare_buffers (sftpfs_pubkey, buffer4, sizeof (buffer4)),
                              VFSX - 14, 2, "input-sftp-pub-key", &tmp_pubkey),
        /*  3 */ QUICK_LABEL (4, VFSX, 12, VFSY, N_("SSH public key:")),
        /*  4 */ QUICK_INPUT (4, VFSX, 11, VFSY,
                              sftpfs_prepare_buffers (sftpfs_privkey, buffer3, sizeof (buffer3)),
                              VFSX - 14, 2, "input-sftp-priv-key", &tmp_privkey),
        /*  5 */ QUICK_LABEL (4, VFSX, 10, VFSY, N_("SSH private key:")),

        /*  6 */ QUICK_RADIO (4, VFSX, 7, VFSY, 3, auth_names, (int *) &tmp_auth_method),
        /*  3 */ QUICK_LABEL (4, VFSX, 6, VFSY, N_("Auth method:")),
        /*  2 */ QUICK_INPUT (4, VFSX, 5, VFSY,
                              sftpfs_prepare_buffers (sftpfs_user, buffer6, sizeof (buffer6)),
                              VFSX - 13, 2, "input-sftp-user", &tmp_username),
        /*  3 */ QUICK_LABEL (4, VFSX, 4, VFSY, N_("User name:")),
        /*  6 */ QUICK_INPUT (37, VFSX, 3, VFSY,
                              sftpfs_prepare_buffers (tmp_pre_port, buffer1, sizeof (buffer1)),
                              10, 0, "input-sftp-port", &tmp_sftp_port),
        /*  7 */ QUICK_LABEL (37, VFSX, 2, VFSY, N_("Port:")),
        /*  8 */ QUICK_INPUT (4, VFSX, 3, VFSY,
                              sftpfs_prepare_buffers (sftpfs_host, buffer2, sizeof (buffer2)),
                              30, 2, "input-sftp-host", &tmp_sftp_host),
        /*  9 */ QUICK_LABEL (4, VFSX, 2, VFSY, N_("Host:")),

        QUICK_END
    };

    QuickDialog confvfs_dlg = {
        VFSX, VFSY, -1, -1, N_("SFTP File System Settings"),
        "[Virtual FS]", confvfs_widgets,
        NULL,
        FALSE
    };

    g_free (tmp_pre_port);

    if (quick_dialog (&confvfs_dlg) != B_CANCEL)
    {
        g_free (sftpfs_pubkey);
        g_free (sftpfs_privkey);
        g_free (sftpfs_host);
        g_free (sftpfs_user);

        sftpfs_pubkey = tmp_pubkey;
        sftpfs_privkey = tmp_privkey;
        sftpfs_host = tmp_sftp_host;
        sftpfs_port = atoi (tmp_sftp_port);
        sftpfs_auth_method = tmp_auth_method;
        sftpfs_user = tmp_username;
        sftpfs_save_param (sftpfs_sessionname);
    }
}

/* --------------------------------------------------------------------------------------------- */

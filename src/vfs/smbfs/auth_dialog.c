/* Virtual File System: Samba file system.
   Show authentication dialog

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
#include "lib/widget.h"

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

void
smbfs_auth_dialog (const char *server, const char *share,
                   char **workgroup, char **username, char **password)
{
    char *smb_path;

    smb_path =
        g_strconcat (_("Samba share: "), PATH_SEP_STR, PATH_SEP_STR, server, PATH_SEP_STR, share,
                     NULL);

    {
        char *new_workgroup = NULL;
        char *new_username = NULL;
        char *new_password = NULL;
        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_LABEL (smb_path, NULL),
            QUICK_SEPARATOR (TRUE),
            QUICK_LABELED_INPUT (N_("Workgroup:"), input_label_above,
                                 *workgroup, "input-3", &new_workgroup, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_LABELED_INPUT (N_("User name:"), input_label_above,
                                 *username, "input-2", &new_username, NULL, FALSE, FALSE, INPUT_COMPLETE_USERNAMES),
            QUICK_LABELED_INPUT (N_("Password:"), input_label_above,
                                 *password, "input-1", &new_password, NULL, TRUE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 64,
            N_("Samba credentials"), "[Samba credentials]",
            quick_widgets, NULL, NULL
        };

        if (quick_dialog (&qdlg) != B_CANCEL)
        {
            smbfs_assign_value_if_not_null (new_workgroup, workgroup);
            smbfs_assign_value_if_not_null (new_username, username);
            smbfs_assign_value_if_not_null (new_password, password);
        }
        else
        {
            g_free (new_password);
            g_free (new_username);
            g_free (new_workgroup);
        }
    }
    g_free (smb_path);
}

/*
   Editor's events definitions.

   Copyright (C) 2012-2014
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2014

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
#include "lib/event.h"

#include "event.h"

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
mc_filemanager_init_events (GError ** error)
{
    /* *INDENT-OFF* */
    event_init_group_t treeview_group_events[] =
    {
        {"help", mc_tree_cmd_help, NULL},
        {"forget", mc_tree_cmd_forget, NULL},
        {"navigation_mode_toggle", mc_tree_cmd_navigation_mode_toggle, NULL},
        {"copy", mc_tree_cmd_copy, NULL},
        {"move", mc_tree_cmd_move, NULL},
        {"goto_up", mc_tree_cmd_goto_up, NULL},
        {"goto_down", mc_tree_cmd_goto_down, NULL},
        {"goto_home", mc_tree_cmd_goto_home, NULL},
        {"goto_end", mc_tree_cmd_goto_end, NULL},
        {"goto_page_up", mc_tree_cmd_goto_page_up, NULL},
        {"goto_page_down", mc_tree_cmd_goto_page_down, NULL},
        {"goto_left", mc_tree_cmd_goto_left, NULL},
        {"goto_right", mc_tree_cmd_goto_right, NULL},
        {"enter", mc_tree_cmd_enter, NULL},
        {"rescan", mc_tree_cmd_rescan, NULL},
        {"search_begin", mc_tree_cmd_search_begin, NULL},
        {"rmdir", mc_tree_cmd_rmdir, NULL},
        {"chdir", mc_tree_cmd_chdir, NULL},
        {"show_box", mc_tree_cmd_show_box, NULL},

        {NULL, NULL, NULL}
    };
    /* *INDENT-ON* */

    /* *INDENT-OFF* */
    event_init_t standard_events[] =
    {
        {MCEVENT_GROUP_TREEVIEW, treeview_group_events},
        {NULL, NULL}
    };
    /* *INDENT-ON* */

    mc_event_mass_add (standard_events, error);

}

/* --------------------------------------------------------------------------------------------- */

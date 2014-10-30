/*
   Event callbacks initialization

   Copyright (C) 2011-2015
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

#include <config.h>

#include "lib/global.h"

#ifdef ENABLE_BACKGROUND
#include "background.h"         /* (background_parent_call), background_parent_call_string() */
#endif /* ENABLE_BACKGROUND */
#include "clipboard.h"          /* clipboard events */
#include "execute.h"            /* execute_suspend() */
#include "help.h"               /* help_interactive_display() */

#include "events_init.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/


/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
events_init (GError ** mcerror)
{
    /* *INDENT-OFF* */
    event_init_group_t core_group_events[] =
    {
        {"clipboard_file_to_ext_clip", clipboard_file_to_ext_clip, NULL},
        {"clipboard_file_from_ext_clip", clipboard_file_from_ext_clip, NULL},
        {"clipboard_text_to_file", clipboard_text_to_file, NULL},
        {"clipboard_text_from_file", clipboard_text_from_file, NULL},
        {"help", mc_help_cmd_interactive_display, NULL},
        {"suspend", execute_suspend, NULL},
        {"configuration_learn_keys_show_dialog", mc_core_cmd_configuration_learn_keys_show_dialog, NULL},
        {"save_setup", mc_core_cmd_save_setup, NULL},

#ifdef ENABLE_BACKGROUND
        {"background_parent_call", background_parent_call, NULL},
        {"background_parent_call_string", background_parent_call_string, NULL},
#endif /* ENABLE_BACKGROUND */

        {NULL, NULL, NULL}
    };
    /* *INDENT-ON* */

    /* *INDENT-OFF* */
    event_init_group_t help_group_events[] =
    {
        {"show_dialog", mc_help_cmd_show_dialog, NULL},
        {"go_index", mc_help_cmd_go_index, NULL},
        {"go_back", mc_help_cmd_go_back, NULL},
        {"go_down", mc_help_cmd_go_down, NULL},
        {"go_next_link", mc_help_cmd_go_next_link, NULL},
        {"go_up", mc_help_cmd_go_up, NULL},
        {"go_prev_link", mc_help_cmd_go_prev_link, NULL},
        {"go_page_down", mc_help_cmd_go_page_down, NULL},
        {"go_pageup", mc_help_cmd_go_page_up, NULL},
        {"go_half_page_down", mc_help_cmd_go_half_page_down, NULL},
        {"go_half_pageup", mc_help_cmd_go_half_page_up, NULL},
        {"go_top", mc_help_cmd_go_top, NULL},
        {"go_bottom", mc_help_cmd_go_bottom, NULL},
        {"select_link", mc_help_cmd_select_link, NULL},
        {"go_next_node", mc_help_cmd_go_next_node, NULL},
        {"go_prev_node", mc_help_cmd_go_prev_node, NULL},
        {"quit", mc_help_cmd_quit, NULL},

        {NULL, NULL, NULL}
    };
    /* *INDENT-ON* */

    /* *INDENT-OFF* */
    event_init_t standard_events[] =
    {
        {MCEVENT_GROUP_CORE, core_group_events},
        {MCEVENT_GROUP_HELP, help_group_events},
        {NULL, NULL}
    };
    /* *INDENT-ON* */

    if (!mc_event_init (mcerror))
        return FALSE;

    return mc_event_mass_add (standard_events, mcerror);
}

/* --------------------------------------------------------------------------------------------- */

/*
   Macros implementstion.

   Copyright (C) 2009-2014
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2009
   Slava Zanko <slavazanko@gmail.com>, 2009

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

#include <stdlib.h>

#include "lib/global.h"
#include "lib/tty/key.h"

#include "src/history.h"
#include "src/setup.h"
#include "src/keybind-defaults.h"

#include "edit-impl.h"
#include "editwidget.h"
#include "editcmd_dialogs.h"

#include "event.h"
#include "macro.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
edit_macro_comparator (gconstpointer * macro1, gconstpointer * macro2)
{
    const macros_t *m1 = (const macros_t *) macro1;
    const macros_t *m2 = (const macros_t *) macro2;

    return m1->hotkey - m2->hotkey;
}

/* --------------------------------------------------------------------------------------------- */

static void
edit_macro_sort_by_hotkey (void)
{
    if (macros_list != NULL && macros_list->len != 0)
        g_array_sort (macros_list, (GCompareFunc) edit_macro_comparator);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
edit_get_macro (WEdit * edit, int hotkey, const macros_t ** macros, guint * indx)
{
    const macros_t *array_start = &g_array_index (macros_list, struct macros_t, 0);
    macros_t *result;
    macros_t search_macro;

    (void) edit;

    search_macro.hotkey = hotkey;
    result = bsearch (&search_macro, macros_list->data, macros_list->len,
                      sizeof (macros_t), (GCompareFunc) edit_macro_comparator);

    if (result != NULL && result->macro != NULL)
    {
        *indx = (result - array_start);
        *macros = result;
        return TRUE;
    }
    *indx = 0;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/** returns FALSE on error */

static gboolean
edit_delete_macro (WEdit * edit, int hotkey)
{
    mc_config_t *macros_config = NULL;
    const char *section_name = "editor";
    gchar *macros_fname;
    guint indx;
    char *skeyname;
    const macros_t *macros = NULL;

    /* clear array of actions for current hotkey */
    while (edit_get_macro (edit, hotkey, &macros, &indx))
    {
        if (macros->macro != NULL)
            g_array_free (macros->macro, TRUE);
        macros = NULL;
        g_array_remove_index (macros_list, indx);
        edit_macro_sort_by_hotkey ();
    }

    macros_fname = mc_config_get_full_path (MC_MACRO_FILE);
    macros_config = mc_config_init (macros_fname, FALSE);
    g_free (macros_fname);

    if (macros_config == NULL)
        return FALSE;

    skeyname = lookup_key_by_code (hotkey);
    while (mc_config_del_key (macros_config, section_name, skeyname))
        ;
    g_free (skeyname);
    mc_config_save_file (macros_config, NULL);
    mc_config_deinit (macros_config);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* event callback */

gboolean
mc_editor_cmd_macro_delete (event_info_t * event_info, gpointer data, GError ** error)
{
    WEdit *edit = (WEdit *) data;

    int hotkey;

    (void) event_info;
    (void) error;

    hotkey = editcmd_dialog_raw_key_query (_("Delete macro"), _("Press macro hotkey:"), TRUE);

    if (hotkey != 0 && !edit_delete_macro (edit, hotkey))
        message (D_ERROR, _("Delete macro"), _("Macro not deleted"));

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/** returns FALSE on error */

gboolean
edit_execute_macro (WEdit * edit, int hotkey)
{
    gboolean res = FALSE;

    if (hotkey != 0)
    {
        const macros_t *macros;
        guint indx;

        if (edit_get_macro (edit, hotkey, &macros, &indx) &&
            macros->macro != NULL && macros->macro->len != 0)
        {
            guint i;

            edit->force |= REDRAW_PAGE;

            for (i = 0; i < macros->macro->len; i++)
            {
                const macro_action_t *m_act;

                m_act = &g_array_index (macros->macro, struct macro_action_t, i);
                edit_execute_cmd (edit, m_act->action, m_act->ch);
                res = TRUE;
            }
        }
    }

    return res;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

gboolean
mc_editor_cmd_macro_store (event_info_t * event_info, gpointer data, GError ** error)
{
    WEdit *edit = (WEdit *) data;

    int i;
    int hotkey;
    GString *marcros_string;
    mc_config_t *macros_config = NULL;
    const char *section_name = "editor";
    gchar *macros_fname;
    GArray *macros;             /* current macro */
    int tmp_act;
    gboolean have_macro = FALSE;
    char *skeyname = NULL;

    (void) event_info;
    (void) error;

    hotkey =
        editcmd_dialog_raw_key_query (_("Save macro"), _("Press the macro's new hotkey:"), TRUE);
    if (hotkey == ESC_CHAR)
        return TRUE;

    tmp_act = keybind_lookup_keymap_command (editor_map, hotkey);

    /* return FALSE if try assign macro into restricted hotkeys */
    if (tmp_act == CK_MacroStartRecord
        || tmp_act == CK_MacroStopRecord || tmp_act == CK_MacroStartStopRecord)
        return TRUE;

    edit_delete_macro (edit, hotkey);

    macros_fname = mc_config_get_full_path (MC_MACRO_FILE);
    macros_config = mc_config_init (macros_fname, FALSE);
    g_free (macros_fname);

    if (macros_config == NULL)
        return TRUE;

    edit_push_undo_action (edit, KEY_PRESS + edit->start_display);

    marcros_string = g_string_sized_new (250);
    macros = g_array_new (TRUE, FALSE, sizeof (macro_action_t));

    skeyname = lookup_key_by_code (hotkey);

    for (i = 0; i < macro_index; i++)
    {
        macro_action_t m_act;
        const char *action_name;

        action_name = keybind_lookup_actionname (record_macro_buf[i].action);

        if (action_name == NULL)
            break;

        m_act.action = record_macro_buf[i].action;
        m_act.ch = record_macro_buf[i].ch;
        g_array_append_val (macros, m_act);
        have_macro = TRUE;
        g_string_append_printf (marcros_string, "%s:%i;", action_name,
                                (int) record_macro_buf[i].ch);
    }
    if (have_macro)
    {
        macros_t macro;
        macro.hotkey = hotkey;
        macro.macro = macros;
        g_array_append_val (macros_list, macro);
        mc_config_set_string (macros_config, section_name, skeyname, marcros_string->str);
    }
    else
        mc_config_del_key (macros_config, section_name, skeyname);

    g_free (skeyname);
    edit_macro_sort_by_hotkey ();

    g_string_free (marcros_string, TRUE);
    mc_config_save_file (macros_config, NULL);
    mc_config_deinit (macros_config);

    return TRUE;
}

 /* --------------------------------------------------------------------------------------------- */
/* event callback */

gboolean
mc_editor_cmd_macro_repeat (event_info_t * event_info, gpointer data, GError ** error)
{
    WEdit *edit = (WEdit *) data;

    int i, j;
    char *f;
    long count_repeat;
    char *error_str = NULL;

    (void) event_info;
    (void) error;

    f = input_dialog (_("Repeat last commands"), _("Repeat times:"), MC_HISTORY_EDIT_REPEAT, NULL,
                      INPUT_COMPLETE_NONE);
    if (f == NULL || *f == '\0')
    {
        g_free (f);
        return TRUE;
    }

    count_repeat = strtol (f, &error_str, 0);

    if (*error_str != '\0')
    {
        g_free (f);
        return TRUE;
    }

    g_free (f);

    edit_push_undo_action (edit, KEY_PRESS + edit->start_display);
    edit->force |= REDRAW_PAGE;

    for (j = 0; j < count_repeat; j++)
        for (i = 0; i < macro_index; i++)
            edit_execute_cmd (edit, record_macro_buf[i].action, record_macro_buf[i].ch);
    edit_update_screen (edit);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/** return FALSE on error */

gboolean
edit_load_macro_cmd (WEdit * edit)
{
    mc_config_t *macros_config = NULL;
    gchar **profile_keys, **keys;
    gchar **values, **curr_values;
    gsize len, values_len;
    const char *section_name = "editor";
    gchar *macros_fname;

    (void) edit;

    macros_fname = mc_config_get_full_path (MC_MACRO_FILE);
    macros_config = mc_config_init (macros_fname, TRUE);
    g_free (macros_fname);

    if (macros_config == NULL || macros_list == NULL || macros_list->len != 0)
        return FALSE;

    keys = mc_config_get_keys (macros_config, section_name, &len);
    for (profile_keys = keys; *profile_keys != NULL; profile_keys++)
    {
        int hotkey;
        gboolean have_macro = FALSE;
        GArray *macros;
        macros_t macro;

        macros = g_array_new (TRUE, FALSE, sizeof (macro_action_t));
        values =
            mc_config_get_string_list (macros_config, section_name, *profile_keys, &values_len);
        hotkey = lookup_key (*profile_keys, NULL);

        for (curr_values = values; *curr_values != NULL && *curr_values[0] != '\0'; curr_values++)
        {
            char **macro_pair = NULL;

            macro_pair = g_strsplit (*curr_values, ":", 2);
            if (macro_pair != NULL)
            {
                macro_action_t m_act;
                if (macro_pair[0] == NULL || macro_pair[0][0] == '\0')
                    m_act.action = 0;
                else
                {
                    m_act.action = keybind_lookup_action (macro_pair[0]);
                    MC_PTR_FREE (macro_pair[0]);
                }
                if (macro_pair[1] == NULL || macro_pair[1][0] == '\0')
                    m_act.ch = -1;
                else
                {
                    m_act.ch = strtol (macro_pair[1], NULL, 0);
                    MC_PTR_FREE (macro_pair[1]);
                }
                if (m_act.action != 0)
                {
                    /* a shell command */
                    if ((m_act.action / CK_PipeBlock (0)) == 1)
                    {
                        m_act.action = CK_PipeBlock (0) + (m_act.ch > 0 ? m_act.ch : 0);
                        m_act.ch = -1;
                    }
                    g_array_append_val (macros, m_act);
                    have_macro = TRUE;
                }
                g_strfreev (macro_pair);
                macro_pair = NULL;
            }
        }
        if (have_macro)
        {
            macro.hotkey = hotkey;
            macro.macro = macros;
            g_array_append_val (macros_list, macro);
        }
        g_strfreev (values);
    }
    g_strfreev (keys);
    mc_config_deinit (macros_config);
    edit_macro_sort_by_hotkey ();

    return TRUE;
}

/* }}} Macro stuff end here */

/* --------------------------------------------------------------------------------------------- */
/* event callback */

gboolean
mc_editor_cmd_macro_record_start_stop (event_info_t * event_info, gpointer data, GError ** error)
{
    WEdit *edit = (WEdit *) data;

    (void) event_info;
    (void) error;

    /* edit is a pointer to the widget */
    if (edit != NULL)
    {
        unsigned long command = macro_index < 0 ? CK_MacroStartRecord : CK_MacroStopRecord;
        edit_execute_key_command (edit, command, -1);
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

gboolean
mc_editor_cmd_macro_repeat_start_stop (event_info_t * event_info, gpointer data, GError ** error)
{
    WEdit *edit = (WEdit *) data;

    (void) event_info;
    (void) error;

    /* edit is a pointer to the widget */
    if (edit != NULL)
    {
        unsigned long command = macro_index < 0 ? CK_RepeatStartRecord : CK_RepeatStopRecord;
        edit_execute_key_command (edit, command, -1);
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

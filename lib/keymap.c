/*
   Handle keyspresses and bind them to events.

   Copyright (C) 2005-2014
   Free Software Foundation, Inc.

   Written by:
   Vitja Makarov, 2005
   Ilia Maslakov <il.smind@gmail.com>, 2009, 2012
   Andrew Borodin <aborodin@vmail.ru>, 2009, 2010, 2011, 2012
   Totally rewritten by:
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
#include "lib/keymap.h"
#include "lib/tty/key.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define MC_KEYMAP_EVENT_GROUP "keymap"
#define MC_KEYMAP_EVENT_SWITCH_GROUP_NAME "switch_group"

#define KEYMAP_ERR_NOT_INITIALIZED N_ ("Keymap system not initialized")
#define KEYMAP_ERR_INPUT_DATA N_ ("Check input data! Some of parameters are NULL!")

/*** file scope type declarations ****************************************************************/

typedef struct
{
    long code;
    const char *name;
} mc_keymap_keycode_t;


typedef struct
{
    char *group;
    char *name;
    char *switch_group;
} mc_keymap_event_t;

typedef struct
{
    GTree *keymap_group;
    const char *name;
} mc_keymap_clear_keycodes_t;

/*** file scope variables ************************************************************************/

static GTree *all_key_codes = NULL;

static GTree *all_key_events = NULL;

static const char *switched_group = NULL;

static gboolean switch_group_cmd_was_called = FALSE;


/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_keymap_cmd_switch_group (event_info_t * event_info, gpointer data, GError ** error)
{
    (void) event_info;
    (void) error;

    switched_group = (const char *) data;
    switch_group_cmd_was_called = TRUE;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_keymap_is_initialised (GError ** error)
{
    if (all_key_codes == NULL)
    {
        g_propagate_error (error, g_error_new (MC_ERROR, 1, _(KEYMAP_ERR_NOT_INITIALIZED)));
        return FALSE;
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gint
mc_keymap_keycode_compare (gconstpointer a, gconstpointer b)
{
    return (a > b) ? 1 : (a < b) ? -1 : 0;
}

/* --------------------------------------------------------------------------------------------- */

static GTree *
mc_keymap_get_key_group (const char *group, GError ** error)
{
    GTree *keymap_group;

    keymap_group = (GTree *) g_tree_lookup (all_key_codes, (gconstpointer) group);

    if (keymap_group == NULL)
    {
        keymap_group =
            g_tree_new_full ((GCompareDataFunc) mc_keymap_keycode_compare, NULL, NULL,
                             (GDestroyNotify) g_free);

        if (keymap_group == NULL)
            g_propagate_error (error,
                               g_error_new (MC_ERROR, 1,
                                            _("Unable to create group '%s' for keymaps!"), group));
        else
            g_tree_insert (all_key_codes, g_strdup (group), (gpointer) keymap_group);
    }
    return keymap_group;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_keymap_clear_keycodes (gpointer key, gpointer value, gpointer data)
{
    mc_keymap_clear_keycodes_t *keymap_data = (mc_keymap_clear_keycodes_t *) data;

    if (strcmp (value, keymap_data->name) == 0)
        g_tree_remove (keymap_data->keymap_group, key);

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_keymap_add_keycode_to_group (GTree * keymap_group, const char *name,
                                const char *pressed_keynames, gboolean isDeleteOld)
{
    char **keyname_list, **one_keyname;

    keyname_list = g_strsplit_set (pressed_keynames, ";", -1);

    if (isDeleteOld)
    {
        mc_keymap_clear_keycodes_t data = {
            .keymap_group = keymap_group,
            .name = name
        };

        g_tree_foreach (keymap_group, mc_keymap_clear_keycodes, &data);
    }

    for (one_keyname = keyname_list; one_keyname != NULL && *one_keyname != NULL; one_keyname++)
    {
        long keycode;
        char *caption = NULL;

        if ((*one_keyname)[0] == '\0')
            continue;

        keycode = lookup_key (*one_keyname, &caption);
        g_tree_replace (keymap_group, (void *) keycode, g_strdup (name));
        g_free (caption);
    }

    g_strfreev (keyname_list);
}

/* --------------------------------------------------------------------------------------------- */

static char *
keymap_make_event_key (const char *group, const char *name)
{
    return g_strdup_printf ("[%s][%s]", group, name);
}

/* --------------------------------------------------------------------------------------------- */

static mc_keymap_event_t *
keymap_get_event (const char *group, const char *name)
{
    char *event_key;
    mc_keymap_event_t *ret_value;

    event_key = keymap_make_event_key (group, name);
    ret_value = (mc_keymap_event_t *) g_tree_lookup (all_key_events, (gconstpointer) event_key);
    g_free (event_key);

    return ret_value;
}

/* --------------------------------------------------------------------------------------------- */

static void
keymap_event_free (mc_keymap_event_t * keymap_event)
{
    g_free (keymap_event->group);
    g_free (keymap_event->name);
    g_free (keymap_event->switch_group);
    g_free (keymap_event);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_keymap_bind_event_with_data (const char *group, const char *name, const char *event_group,
                                const char *event_name, const char *switch_group, GError ** error)
{
    char *event_key;
    mc_keymap_event_t *keymap_event;


    if (!mc_keymap_is_initialised (error))
        return FALSE;

    event_key = keymap_make_event_key (group, name);

    keymap_event = g_new (mc_keymap_event_t, 1);
    keymap_event->group = g_strdup (event_group);
    keymap_event->name = g_strdup (event_name);
    keymap_event->switch_group = g_strdup (switch_group);
    g_tree_replace (all_key_events, event_key, (gpointer) keymap_event);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
mc_keymap_init (GError ** error)
{
    if (all_key_codes != NULL)
    {
        g_propagate_error (error,
                           g_error_new (MC_ERROR, 1, _("Keymap system already initialized")));
        return FALSE;
    }

    all_key_codes =
        g_tree_new_full ((GCompareDataFunc) g_ascii_strcasecmp,
                         NULL, (GDestroyNotify) g_free, (GDestroyNotify) g_tree_destroy);

    if (all_key_codes == NULL)
    {
        g_propagate_error (error, g_error_new (MC_ERROR, 2, _(KEYMAP_ERR_NOT_INITIALIZED)));
        return FALSE;
    }

    all_key_events = g_tree_new_full ((GCompareDataFunc) g_ascii_strcasecmp,
                                      NULL, (GDestroyNotify) g_free,
                                      (GDestroyNotify) keymap_event_free);

    if (all_key_events == NULL)
    {
        g_propagate_error (error, g_error_new (MC_ERROR, 2, _(KEYMAP_ERR_NOT_INITIALIZED)));
        g_tree_destroy (all_key_codes);
        return FALSE;
    }

    mc_event_add (MC_KEYMAP_EVENT_GROUP, MC_KEYMAP_EVENT_SWITCH_GROUP_NAME,
                  mc_keymap_cmd_switch_group, NULL, error);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_keymap_deinit (GError ** error)
{
    if (!mc_keymap_is_initialised (error))
        return FALSE;

    g_tree_destroy (all_key_codes);
    all_key_codes = NULL;
    g_tree_destroy (all_key_events);
    all_key_events = NULL;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_keymap_bind_keycode (const char *group, const char *name, const char *pressed_keynames,
                        gboolean isDeleteOld, GError ** error)
{
    GTree *keymap_group;

    if (!mc_keymap_is_initialised (error))
        return FALSE;

    if (group == NULL || name == NULL)
    {
        g_propagate_error (error, g_error_new (MC_ERROR, 1, _(KEYMAP_ERR_INPUT_DATA)));
        return FALSE;
    }

    keymap_group = mc_keymap_get_key_group (group, error);

    if (keymap_group == NULL)
        return FALSE;

    mc_keymap_add_keycode_to_group (keymap_group, name, pressed_keynames, isDeleteOld);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_keymap_bind_event (const char *group, const char *name, const char *event_group,
                      const char *event_name, GError ** error)
{
    return mc_keymap_bind_event_with_data (group, name, event_group, event_name, NULL, error);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_keymap_bind_switch_event (const char *group, const char *name, const char *switch_group,
                             GError ** error)
{
    return mc_keymap_bind_event_with_data (group, name, MC_KEYMAP_EVENT_GROUP,
                                           MC_KEYMAP_EVENT_SWITCH_GROUP_NAME, switch_group, error);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_keymap_mass_bind_event (const mc_keymap_event_init_t * group_keymap_events, GError ** error)
{
    size_t array_group_index;
    gboolean ret_value = TRUE;

    for (array_group_index = 0; group_keymap_events[array_group_index].group != NULL;
         array_group_index++)
    {
        size_t array_index;
        const char *group_name = group_keymap_events[array_group_index].group;
        const mc_keymap_event_init_group_t *keymap_events =
            group_keymap_events[array_group_index].keymap_events;


        for (array_index = 0; keymap_events[array_index].name != NULL; array_index++)
            if (!mc_keymap_bind_event (group_name, keymap_events[array_index].name,
                                       keymap_events[array_index].event_group,
                                       keymap_events[array_index].event_name, error))
            {
                ret_value = FALSE;
                break;
            }
    }

    return ret_value;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_keymap_process_group (const char *group, long pressed_keycode, void *data, event_return_t * ret,
                         GError ** error)
{

    const char *key_name;
    mc_keymap_event_t *keymap_event;
    gboolean ret_value;

    if (switched_group != NULL)
    {
        group = switched_group;
        switched_group = NULL;
    }

    key_name = mc_keymap_get_key_name_by_code (group, pressed_keycode, error);
    if (key_name == NULL)
        return FALSE;

    keymap_event = keymap_get_event (group, key_name);
    if (keymap_event == NULL)
        return FALSE;

    if (keymap_event->switch_group != NULL)
        data = keymap_event->switch_group;

    ret_value = mc_event_raise (keymap_event->group, keymap_event->name, data, ret, error);

    if (switch_group_cmd_was_called && switched_group == NULL)
    {
        switched_group = group;
        switch_group_cmd_was_called = FALSE;
    }
    return ret_value;
}

/* --------------------------------------------------------------------------------------------- */

const char *
mc_keymap_get_key_name_by_code (const char *group, long pressed_keycode, GError ** error)
{
    GTree *keymap_group;

    keymap_group = mc_keymap_get_key_group (group, error);

    if (keymap_group == NULL)
        return FALSE;

    return (const char *) g_tree_lookup (keymap_group, (gconstpointer) pressed_keycode);
}

/* --------------------------------------------------------------------------------------------- */

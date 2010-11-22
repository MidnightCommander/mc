/*
 * Copyright (c) 2009, 2010 Free Software Foundation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Original idea and code: Oleg "Olegarch" Konovalov <olegarch@linuxinside.com>
 * Written by: 2007 Daniel Borca <dborca@yahoo.com>
 *             2010 Andrew Borodin <aborodin@vmail.ru>
 */

/** \file dialog-switch.c
 *  \brief Source: support of multiply editors and viewers.
 */

#include <config.h>

#include "lib/global.h"
#include "lib/tty/tty.h"        /* LINES, COLS */
#include "lib/widget.h"

/* TODO: these includes should be removed! */
#include "src/filemanager/layout.h"     /* repaint_screen() */
#include "src/filemanager/midnight.h"   /* midnight_dlg */
#include "src/main.h"           /* midnight_shutdown */

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* List of dialogs: filemanagers, editors, viewers */
static GList *mc_dialogs = NULL;
/* Currently active dialog */
static GList *mc_current = NULL;
/* Is there any dialogs that we have to run after returning to the manager from another dialog */
static gboolean dialog_switch_pending = FALSE;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static unsigned char
get_hotkey (int n)
{
    return (n <= 9) ? '0' + n : 'a' + n - 10;
}

/* --------------------------------------------------------------------------------------------- */

static void
dialog_switch_goto (GList * dlg)
{
    if (mc_current != dlg)
    {
        Dlg_head *old = (Dlg_head *) mc_current->data;

        mc_current = dlg;

        if (old == midnight_dlg)
        {
            /* switch from panels to another dialog (editor, viewer, etc) */
            dialog_switch_pending = TRUE;
            dialog_switch_process_pending ();
        }
        else
        {
            /* switch from editor, viewer, etc to another dialog */
            old->state = DLG_SUSPENDED;

            if ((Dlg_head *) dlg->data != midnight_dlg)
                /* switch to another editor, viewer, etc */
                /* return to panels before run the required dialog */
                dialog_switch_pending = TRUE;
            else
                /* switch to panels */
                do_refresh ();
        }
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
dialog_switch_add (Dlg_head * h)
{
    GList *dlg;

    dlg = g_list_find (mc_dialogs, h);

    if (dlg != NULL)
        mc_current = dlg;
    else
    {
        mc_dialogs = g_list_prepend (mc_dialogs, h);
        mc_current = mc_dialogs;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
dialog_switch_remove (Dlg_head * h)
{
    GList *this;

    if ((Dlg_head *) mc_current->data == h)
        this = mc_current;
    else
        this = g_list_find (mc_dialogs, h);

    mc_dialogs = g_list_delete_link (mc_dialogs, this);

    /* adjust current dialog */
    if (top_dlg != NULL)
        mc_current = g_list_find (mc_dialogs, (Dlg_head *) top_dlg->data);
    else
        mc_current = mc_dialogs;
}

/* --------------------------------------------------------------------------------------------- */

size_t
dialog_switch_num (void)
{
    return g_list_length (mc_dialogs);
}

/* --------------------------------------------------------------------------------------------- */

void
dialog_switch_next (void)
{
    GList *next;

    if (midnight_shutdown || mc_current == NULL)
        return;

    next = g_list_next (mc_current);
    if (next == NULL)
        next = mc_dialogs;

    dialog_switch_goto (next);
}

/* --------------------------------------------------------------------------------------------- */

void
dialog_switch_prev (void)
{
    GList *prev;

    if (midnight_shutdown || mc_current == NULL)
        return;

    prev = g_list_previous (mc_current);
    if (prev == NULL)
        prev = g_list_last (mc_dialogs);

    dialog_switch_goto (prev);
}

/* --------------------------------------------------------------------------------------------- */

void
dialog_switch_list (void)
{
    const size_t dlg_num = g_list_length (mc_dialogs);
    int lines, cols;
    Listbox *listbox;
    GList *h;
    int i = 0;
    int rv;

    if (midnight_shutdown || mc_current == NULL)
        return;

    lines = min ((size_t) (LINES * 2 / 3), dlg_num);
    cols = COLS * 2 / 3;

    listbox = create_listbox_window (lines, cols, _("Screens"), "[Screen selector]");

    for (h = mc_dialogs; h != NULL; h = g_list_next (h))
    {
        Dlg_head *dlg;
        char *title;

        dlg = (Dlg_head *) h->data;

        if ((dlg != NULL) && (dlg->get_title != NULL))
            title = dlg->get_title (dlg, listbox->list->widget.cols - 2);       /* FIXME! */
        else
            title = g_strdup ("");

        listbox_add_item (listbox->list, LISTBOX_APPEND_BEFORE, get_hotkey (i++), title, NULL);

        g_free (title);
    }

    listbox_select_entry (listbox->list, dlg_num - 1 - g_list_position (mc_dialogs, mc_current));
    rv = run_listbox (listbox);

    if (rv >= 0)
    {
        h = g_list_nth (mc_dialogs, dlg_num - 1 - rv);
        dialog_switch_goto (h);
    }
}

/* --------------------------------------------------------------------------------------------- */

int
dialog_switch_process_pending (void)
{
    int ret = 0;

    while (dialog_switch_pending)
    {
        Dlg_head *h = (Dlg_head *) mc_current->data;

        dialog_switch_pending = FALSE;
        h->state = DLG_SUSPENDED;
        ret = run_dlg (h);
        if (h->state == DLG_CLOSED)
        {
            destroy_dlg (h);
            /* return to panels */
            if (mc_run_mode == MC_RUN_FULL)
            {
                mc_current = g_list_find (mc_dialogs, midnight_dlg);
                update_panels (UP_OPTIMIZE, UP_KEEPSEL);
            }
        }
    }

    repaint_screen ();

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

void
dialog_switch_got_winch (void)
{
    GList *dlg;

    for (dlg = mc_dialogs; dlg != NULL; dlg = g_list_next (dlg))
        if (dlg != mc_current)
            ((Dlg_head *) dlg->data)->winch_pending = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
dialog_switch_shutdown (void)
{
    while (mc_dialogs != NULL)
    {
        Dlg_head *dlg = (Dlg_head *) mc_dialogs->data;

        run_dlg (dlg);
        destroy_dlg (dlg);
    }
}

/* --------------------------------------------------------------------------------------------- */

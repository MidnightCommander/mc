/*
   Support of multiply editors and viewers.

   Original idea and code: Oleg "Olegarch" Konovalov <olegarch@linuxinside.com>

   Copyright (c) 2009, 2010, 2011
   The Free Software Foundation

   Written by:
   Daniel Borca <dborca@yahoo.com>, 2007
   Andrew Borodin <aborodin@vmail.ru>, 2010

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

/** \file dialog-switch.c
 *  \brief Source: support of multiply editors and viewers.
 */

#include <config.h>

/* If TIOCGWINSZ supported, make it available here, because window resizing code
 * depends on it... */
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <termios.h>

#include "lib/global.h"
#include "lib/tty/tty.h"        /* LINES, COLS */
#include "lib/tty/win.h"        /* do_enter_ca_mode() */
#include "lib/tty/color.h"      /* tty_set_normal_attrs() */
#include "lib/widget.h"
#include "lib/event.h"

/*** global variables ****************************************************************************/

Dlg_head *midnight_dlg = NULL;

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
dialog_switch_suspend (void *data, void *user_data)
{
    (void) user_data;

    if (data != mc_current->data)
        ((Dlg_head *) data)->state = DLG_SUSPENDED;
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
            {
                /* switch to panels */
                midnight_dlg->state = DLG_ACTIVE;
                do_refresh ();
            }
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

#if defined TIOCGWINSZ
static void
dlg_resize_cb (void *data, void *user_data)
{
    Dlg_head *d = data;

    (void) user_data;
    if (d->state == DLG_ACTIVE)
        d->callback (d, NULL, DLG_RESIZE, 0, NULL);
    else
        d->winch_pending = TRUE;
}
#endif

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

    /* suspend forced all other screens */
    g_list_foreach (mc_dialogs, dialog_switch_suspend, NULL);
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

    /* resume forced the current screen */
    if (mc_current != NULL)
        ((Dlg_head *) mc_current->data)->state = DLG_ACTIVE;
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

    if (mc_global.midnight_shutdown || mc_current == NULL)
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

    if (mc_global.midnight_shutdown || mc_current == NULL)
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

    if (mc_global.midnight_shutdown || mc_current == NULL)
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
            if (mc_global.mc_run_mode == MC_RUN_FULL)
            {
                mc_current = g_list_find (mc_dialogs, midnight_dlg);
                mc_event_raise (MCEVENT_GROUP_FILEMANAGER, "update_panels", NULL);
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

void
clr_scr (void)
{
    tty_set_normal_attrs ();
    tty_fill_region (0, 0, LINES, COLS, ' ');
    tty_refresh ();
}

/* --------------------------------------------------------------------------------------------- */

void
repaint_screen (void)
{
    do_refresh ();
    tty_refresh ();
}

/* --------------------------------------------------------------------------------------------- */

void
mc_refresh (void)
{
#ifdef ENABLE_BACKGROUND
    if (mc_global.we_are_background)
        return;
#endif /* ENABLE_BACKGROUND */
    if (!mc_global.tty.winch_flag)
        tty_refresh ();
    else
    {
        /* if winch was caugth, we should do not only redraw screen, but
           reposition/resize all */
        dialog_change_screen_size ();
    }
}

/* --------------------------------------------------------------------------------------------- */

void
dialog_change_screen_size (void)
{
    mc_global.tty.winch_flag = FALSE;
#if defined(HAVE_SLANG) || NCURSES_VERSION_MAJOR >= 4
#if defined TIOCGWINSZ

#ifndef NCURSES_VERSION
    tty_noraw_mode ();
    tty_reset_screen ();
#endif
    tty_change_screen_size ();
#ifdef HAVE_SLANG
    /* XSI Curses spec states that portable applications shall not invoke
     * initscr() more than once.  This kludge could be done within the scope
     * of the specification by using endwin followed by a refresh (in fact,
     * more than one curses implementation does this); it is guaranteed to work
     * only with slang.
     */
    SLsmg_init_smg ();
    do_enter_ca_mode ();
    tty_keypad (TRUE);
    tty_nodelay (FALSE);
#endif

    /* Inform all suspending dialogs */
    dialog_switch_got_winch ();
    /* Inform all running dialogs */
    g_list_foreach (top_dlg, (GFunc) dlg_resize_cb, NULL);

    /* Now, force the redraw */
    repaint_screen ();

#endif /* TIOCGWINSZ */
#endif /* defined(HAVE_SLANG) || NCURSES_VERSION_MAJOR >= 4 */
}

/* --------------------------------------------------------------------------------------------- */

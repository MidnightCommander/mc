/*
   Learn keys

   Copyright (C) 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007, 2009, 2011
   The Free Software Foundation, Inc.

   Written by:
   Jakub Jelinek, 1995

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

/** \file learn.c
 *  \brief Source: learn keys module
 */

#include <config.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/key.h"
#include "lib/mcconfig.h"       /* Save profile */
#include "lib/strescape.h"
#include "lib/strutil.h"
#include "lib/util.h"           /* convert_controls() */
#include "lib/widget.h"

#include "src/filemanager/layout.h"     /* mc_refresh()  */

#include "setup.h"
#include "learn.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define UX 4
#define UY 3

#define BY UY + 17

#define ROWS     13
#define COLSHIFT 23

#define BUTTONS 2

/*** file scope type declarations ****************************************************************/

typedef struct
{
    Widget *button;
    Widget *label;
    int ok;
    char *sequence;
} learnkey;

/*** file scope variables ************************************************************************/

static struct
{
    int ret_cmd, flags, y, x;
    const char *text;
} learn_but[BUTTONS] =
{
    /* *INDENT-OFF */
    {
    B_CANCEL, NORMAL_BUTTON, 0, 39, N_("&Cancel")},
    {
    B_ENTER, DEFPUSH_BUTTON, 0, 25, N_("&Save")}
    /* *INDENT-ON */
};

static Dlg_head *learn_dlg;

static learnkey *learnkeys = NULL;
static int learn_total;
static int learnok;
static int learnchanged;
static const char *learn_title = N_("Learn keys");

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
learn_button (WButton * button, int action)
{
    Dlg_head *d;
    char *seq;

    (void) button;

    d = create_message (D_ERROR, _("Teach me a key"),
                        _("Please press the %s\n"
                          "and then wait until this message disappears.\n\n"
                          "Then, press it again to see if OK appears\n"
                          "next to its button.\n\n"
                          "If you want to escape, press a single Escape key\n"
                          "and wait as well."), _(key_name_conv_tab[action - B_USER].longname));
    mc_refresh ();
    if (learnkeys[action - B_USER].sequence != NULL)
    {
        g_free (learnkeys[action - B_USER].sequence);
        learnkeys[action - B_USER].sequence = NULL;
    }
    seq = learn_key ();

    if (seq)
    {
        /* Esc hides the dialog and do not allow definitions of
         * regular characters
         */
        gboolean seq_ok = FALSE;

        if (*seq && strcmp (seq, "\\e") && strcmp (seq, "\\e\\e")
            && strcmp (seq, "^m") && strcmp (seq, "^i") && (seq[1] || (*seq < ' ' || *seq > '~')))
        {

            learnchanged = 1;
            learnkeys[action - B_USER].sequence = seq;
            seq = convert_controls (seq);
            seq_ok = define_sequence (key_name_conv_tab[action - B_USER].code, seq, MCKEY_NOACTION);
        }

        if (!seq_ok)
            message (D_NORMAL, _("Cannot accept this key"), _("You have entered \"%s\""), seq);

        g_free (seq);
    }

    dlg_run_done (d);
    destroy_dlg (d);
    dlg_select_widget (learnkeys[action - B_USER].button);
    return 0;                   /* Do not kill learn_dlg */
}

/* --------------------------------------------------------------------------------------------- */

static int
learn_move (int right)
{
    int i, totalcols;

    totalcols = (learn_total - 1) / ROWS + 1;
    for (i = 0; i < learn_total; i++)
        if (learnkeys[i].button == WIDGET (learn_dlg->current->data))
        {
            if (right)
            {
                if (i < learn_total - ROWS)
                    i += ROWS;
                else
                    i %= ROWS;
            }
            else
            {
                if (i / ROWS)
                    i -= ROWS;
                else if (i + (totalcols - 1) * ROWS >= learn_total)
                    i += (totalcols - 2) * ROWS;
                else
                    i += (totalcols - 1) * ROWS;
            }
            dlg_select_widget (learnkeys[i].button);
            return 1;
        }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
learn_check_key (int c)
{
    int i;

    for (i = 0; i < learn_total; i++)
    {
        if (key_name_conv_tab[i].code != c || learnkeys[i].ok)
            continue;

        dlg_select_widget (learnkeys[i].button);
        /* TRANSLATORS: This label appears near learned keys.  Keep it short.  */
        label_set_text ((WLabel *) learnkeys[i].label, _("OK"));
        learnkeys[i].ok = 1;
        learnok++;
        if (learnok >= learn_total)
        {
            learn_dlg->ret_value = B_CANCEL;
            if (learnchanged)
            {
                if (query_dialog (learn_title,
                                  _
                                  ("It seems that all your keys already\n"
                                   "work fine. That's great."), D_ERROR, 2,
                                  _("&Save"), _("&Discard")) == 0)
                    learn_dlg->ret_value = B_ENTER;
            }
            else
            {
                message (D_ERROR, learn_title,
                         _
                         ("Great! You have a complete terminal database!\n"
                          "All your keys work well."));
            }
            dlg_stop (learn_dlg);
        }
        return 1;
    }
    switch (c)
    {
    case KEY_LEFT:
    case 'h':
        return learn_move (0);
    case KEY_RIGHT:
    case 'l':
        return learn_move (1);
    case 'j':
        dlg_one_down (learn_dlg);
        return 1;
    case 'k':
        dlg_one_up (learn_dlg);
        return 1;
    }

    /* Prevent from disappearing if a non-defined sequence is pressed
       and contains a button hotkey.  Only recognize hotkeys with ALT.  */
    if (c < 255 && g_ascii_isalnum (c))
        return 1;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
learn_callback (Dlg_head * h, Widget * sender, dlg_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case DLG_DRAW:
        common_dialog_repaint (h);
        return MSG_HANDLED;

    case DLG_KEY:
        return learn_check_key (parm);

    default:
        return default_dlg_callback (h, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
init_learn (void)
{
    int x, y, i, j;
    const key_code_name_t *key;
    char buffer[BUF_TINY];

#ifdef ENABLE_NLS
    static int i18n_flag = 0;
    if (!i18n_flag)
    {
        learn_but[0].text = _(learn_but[0].text);
        learn_but[0].x = 78 / 2 + 4;

        learn_but[1].text = _(learn_but[1].text);
        learn_but[1].x = 78 / 2 - (str_term_width1 (learn_but[1].text) + 9);

        learn_title = _(learn_title);
        i18n_flag = 1;
    }
#endif /* ENABLE_NLS */

    do_refresh ();

    learn_dlg =
        create_dlg (TRUE, 0, 0, 23, 78, dialog_colors, learn_callback, NULL,
                    "[Learn keys]", learn_title, DLG_CENTER | DLG_REVERSE);

    for (i = 0; i < BUTTONS; i++)
        add_widget (learn_dlg,
                    button_new (BY + learn_but[i].y, learn_but[i].x,
                                learn_but[i].ret_cmd, learn_but[i].flags, _(learn_but[i].text), 0));

    x = UX;
    y = UY;
    for (key = key_name_conv_tab, j = 0;
         key->name != NULL && strcmp (key->name, "kpleft"); key++, j++)
        ;
    learnkeys = g_new (learnkey, j);
    x += ((j - 1) / ROWS) * COLSHIFT;
    y += (j - 1) % ROWS;
    learn_total = j;
    learnok = 0;
    learnchanged = 0;
    for (i = j - 1, key = key_name_conv_tab + j - 1; i >= 0; i--, key--)
    {
        learnkeys[i].ok = 0;
        learnkeys[i].sequence = NULL;
        g_snprintf (buffer, sizeof (buffer), "%-16s", _(key->longname));
        learnkeys[i].button =
                WIDGET (button_new (y, x, B_USER + i, NARROW_BUTTON, buffer, learn_button));
        add_widget (learn_dlg, learnkeys[i].button);
        learnkeys[i].label = WIDGET (label_new (y, x + 19, ""));
        add_widget (learn_dlg, learnkeys[i].label);
        if (i % 13)
            y--;
        else
        {
            x -= COLSHIFT;
            y = UY + ROWS - 1;
        }
    }
    add_widget (learn_dlg,
                label_new (UY + 14, 5,
                           _("Press all the keys mentioned here. After you have done it, check")));
    add_widget (learn_dlg,
                label_new (UY + 15, 5,
                           _("which keys are not marked with OK.  Press space on the missing")));
    add_widget (learn_dlg,
                label_new (UY + 16, 5,
                           _("key, or click with the mouse to define it. Move around with Tab.")));
}

/* --------------------------------------------------------------------------------------------- */

static void
learn_done (void)
{
    destroy_dlg (learn_dlg);
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

static void
learn_save (void)
{
    int i;
    int profile_changed = 0;
    char *section = g_strconcat ("terminal:", getenv ("TERM"), (char *) NULL);
    char *esc_str;

    for (i = 0; i < learn_total; i++)
    {
        if (learnkeys[i].sequence != NULL)
        {
            profile_changed = 1;

            esc_str = strutils_escape (learnkeys[i].sequence, -1, ";\\", TRUE);

            mc_config_set_string_raw_value (mc_main_config, section, key_name_conv_tab[i].name,
                                            esc_str);

            g_free (esc_str);
        }
    }

    /* On the one hand no good idea to save the complete setup but 
     * without 'Auto save setup' the new key-definitions will not be 
     * saved unless the user does an 'Options/Save Setup'. 
     * On the other hand a save-button that does not save anything to 
     * disk is much worse.
     */
    if (profile_changed)
        mc_config_save_file (mc_main_config, NULL);

    g_free (section);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
learn_keys (void)
{
    int save_old_esc_mode = old_esc_mode;
    gboolean save_alternate_plus_minus = mc_global.tty.alternate_plus_minus;

    /* old_esc_mode cannot work in learn keys dialog */
    old_esc_mode = 0;

    /* don't translate KP_ADD, KP_SUBTRACT and
       KP_MULTIPLY to '+', '-' and '*' in
       correct_key_code */
    mc_global.tty.alternate_plus_minus = TRUE;
    application_keypad_mode ();
    init_learn ();

    run_dlg (learn_dlg);

    old_esc_mode = save_old_esc_mode;
    mc_global.tty.alternate_plus_minus = save_alternate_plus_minus;

    if (!mc_global.tty.alternate_plus_minus)
        numeric_keypad_mode ();

    switch (learn_dlg->ret_value)
    {
    case B_ENTER:
        learn_save ();
        break;
    }

    learn_done ();
}

/* --------------------------------------------------------------------------------------------- */

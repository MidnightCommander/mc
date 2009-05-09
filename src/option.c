/* Configure box module for the Midnight Commander
   Copyright (C) 1994, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006,
   2007, 2009 Free Software Foundation, Inc. 

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  
 */

/** \file option.c
 *  \brief Source: configure box module
 */

#include <config.h>

#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "global.h"

#include "../src/tty/tty.h"
#include "../src/tty/color.h"
#include "../src/tty/win.h"

#include "dialog.h"
#include "widget.h"
#include "setup.h"		/* For save_setup() */
#include "main.h"
#include "../src/mcconfig/mcconfig.h"	/* For mc_config_save_file */
#include "strutil.h"
#include "panel.h"		/* Needed for the externs */
#include "file.h"		/* safe_delete */
#include "layout.h"		/* For nice_rotating_dash */
#include "option.h"

static Dlg_head *conf_dlg;

#define TOGGLE_VARIABLE 0

static int first_width, second_width;

static struct {
    const char   *text;
    int    *variable;
    void   (*toggle_function)(void);
    WCheck *widget;
} check_options [] = {
   /* other options */
   {N_("safe de&Lete"),       &safe_delete,       TOGGLE_VARIABLE,       0 },
   {N_("cd follows lin&Ks"),  &cd_symlinks,       TOGGLE_VARIABLE,       0 },
   {N_("L&ynx-like motion"),  &navigate_with_arrows,TOGGLE_VARIABLE,     0 },
   {N_("rotatin&G dash"),     &nice_rotating_dash,TOGGLE_VARIABLE,       0 },
   {N_("co&Mplete: show all"),&show_all_if_ambiguous,TOGGLE_VARIABLE,    0 },
   {N_("&Use internal view"), &use_internal_view, TOGGLE_VARIABLE,       0 },
   {N_("use internal ed&It"), &use_internal_edit, TOGGLE_VARIABLE,       0 },
   {N_("auto m&Enus"),        &auto_menu,         TOGGLE_VARIABLE,       0 },
   {N_("&Auto save setup"),   &auto_save_setup,   TOGGLE_VARIABLE,       0 },
   {N_("shell &Patterns"),    &easy_patterns,     TOGGLE_VARIABLE,       0 },
   {N_("Compute &Totals"),    &file_op_compute_totals, TOGGLE_VARIABLE,  0 },
   {N_("&Verbose operation"), &verbose,           TOGGLE_VARIABLE,       0 },
   {N_("Mkdir autoname"),     &auto_fill_mkdir_name, TOGGLE_VARIABLE,    0 },
   /* panel options */
   {N_("&Fast dir reload"),   &fast_reload,       toggle_fast_reload,    0 },
   {N_("mi&X all files"),     &mix_all_files,     toggle_mix_all_files,  0 },
   {N_("&Drop down menus"),   &drop_menus,        TOGGLE_VARIABLE,       0 },
   {N_("ma&Rk moves down"),   &mark_moves_down,   TOGGLE_VARIABLE,       0 },
   {N_("show &Hidden files"), &show_dot_files,    toggle_show_hidden,    0 },
   {N_("show &Backup files"), &show_backups,      toggle_show_backup,    0 },
   { 0, 0, 0, 0 }
};

/* Make sure this corresponds to the check_options structure */
#define OTHER_OPTIONS 13
#define PANEL_OPTIONS 6

static WRadio *pause_radio;

static const char *pause_options [3] = {
    N_("&Never"),
    N_("on dumb &Terminals"),
    N_("Alwa&ys") };

#define PAUSE_OPTIONS (sizeof(pause_options) / sizeof(pause_options[0]))

/* Heights of the panes */
#define PY	3
#define OY	PY
/* Align bottoms of "pause after run" and "other options" */
#define RY	(OTHER_OPTIONS - PAUSE_OPTIONS + OY)
#define DLG_Y   (OTHER_OPTIONS + 9)
#define BY	(DLG_Y - 3)

/* Horizontal dimensions */
#define X_MARGIN 3
#define X_PANE_GAP 1
#define PX X_MARGIN
#define RX X_MARGIN
#define OX (first_width + X_MARGIN + X_PANE_GAP)

/* Create the "Configure options" dialog */
static void
init_configure (void)
{
    int i;
    static int i18n_config_flag = 0;
    static int b1, b2, b3;
    const char *ok_button = _("&OK");
    const char *cancel_button = _("&Cancel");
    const char *save_button = _("&Save");
    static const char *title1, *title2, *title3;

    if (!i18n_config_flag) {
	register int l1;

	/* Similar code is in layout.c (init_layout())  */

	title1 = _(" Panel options ");
	title2 = _(" Pause after run... ");
	title3 = _(" Other options ");

        first_width = str_term_width1 (title1) + 1;
        second_width = str_term_width1 (title3) + 1;

	for (i = 0; check_options[i].text; i++) {
	    check_options[i].text = _(check_options[i].text);
            l1 = str_term_width1 (check_options[i].text) + 7;
	    if (i >= OTHER_OPTIONS) {
		if (l1 > first_width)
		    first_width = l1;
	    } else {
		if (l1 > second_width)
		    second_width = l1;
	    }
	}

	i = PAUSE_OPTIONS;
	while (i--) {
	    pause_options[i] = _(pause_options[i]);
            l1 = str_term_width1 (pause_options[i]) + 7;
	    if (l1 > first_width)
		first_width = l1;
	}

        l1 = str_term_width1 (title2) + 1;
	if (l1 > first_width)
	    first_width = l1;

        l1 = 11 + str_term_width1 (ok_button)
                + str_term_width1 (save_button)
                + str_term_width1 (cancel_button);

	i = (first_width + second_width - l1) / 4;
	b1 = 5 + i;
        b2 = b1 + str_term_width1 (ok_button) + i + 6;
        b3 = b2 + str_term_width1 (save_button) + i + 4;

	i18n_config_flag = 1;
    }

    conf_dlg =
	create_dlg (0, 0, DLG_Y,
		    first_width + second_width + 2 * X_MARGIN + X_PANE_GAP,
		    dialog_colors, NULL, "[Configuration]",
		    _("Configure options"), DLG_CENTER | DLG_REVERSE);

    add_widget (conf_dlg,
		groupbox_new (PY, PX, PANEL_OPTIONS + 2, first_width, title1));

    add_widget (conf_dlg,
		groupbox_new (RY, RX, PAUSE_OPTIONS + 2, first_width, title2));

    add_widget (conf_dlg,
		groupbox_new (OY, OX, OTHER_OPTIONS + 2, second_width, title3));

    add_widget (conf_dlg,
		button_new (BY, b3, B_CANCEL, NORMAL_BUTTON,
			    cancel_button, 0));

    add_widget (conf_dlg,
		button_new (BY, b2, B_EXIT, NORMAL_BUTTON,
			    save_button, 0));

    add_widget (conf_dlg,
		button_new (BY, b1, B_ENTER, DEFPUSH_BUTTON,
			    ok_button, 0));

#define XTRACT(i) *check_options[i].variable, check_options[i].text

    /* Add checkboxes for "other options" */
    for (i = 0; i < OTHER_OPTIONS; i++) {
	check_options[i].widget =
	    check_new (OY + (OTHER_OPTIONS - i), OX + 2, XTRACT (i));
	add_widget (conf_dlg, check_options[i].widget);
    }

    pause_radio =
	radio_new (RY + 1, RX + 2, 3, pause_options);
    pause_radio->sel = pause_after_run;
    add_widget (conf_dlg, pause_radio);

    /* Add checkboxes for "panel options" */
    for (i = 0; i < PANEL_OPTIONS; i++) {
	check_options[i + OTHER_OPTIONS].widget =
	    check_new (PY + (PANEL_OPTIONS - i), PX + 2,
		       XTRACT (i + OTHER_OPTIONS));
	add_widget (conf_dlg, check_options[i + OTHER_OPTIONS].widget);
    }
}


void configure_box (void)
{
    int result, i;
    
    init_configure ();
    run_dlg (conf_dlg);

    result = conf_dlg->ret_value;
    if (result == B_ENTER || result == B_EXIT){
	for (i = 0; check_options [i].text; i++)
	    if (check_options [i].widget->state & C_CHANGE){
		if (check_options [i].toggle_function)
		    (*check_options [i].toggle_function)();
		else
		    *check_options [i].variable =
			!(*check_options [i].variable);
	    }
	pause_after_run = pause_radio->sel;
    }

    /* If they pressed the save button */
    if (result == B_EXIT){
	save_configure ();
	mc_config_save_file (mc_main_config);
    }

    destroy_dlg (conf_dlg);
}

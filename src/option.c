/* Configure box module for the Midnight Commander
   Copyright (C) 1994 Radek Doulik

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
 */

#include <config.h>
#include <string.h>
#include <stdio.h>
/* Needed for the extern declarations of integer parameters */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif
#include "tty.h"
#include "mad.h"
#include "util.h"
#include "win.h"
#include "color.h"
#include "dlg.h"
#include "widget.h"
#include "setup.h"		/* For save_setup() */
#include "dialog.h"		/* For do_refresh() */
#include "main.h"
#include "profile.h"		/* For sync_profiles */

#include "dir.h"
#include "panel.h"		/* Needed for the externs */
#include "file.h"
#include "layout.h"		/* For nice_rotating_dash */
#define PX	4
#define PY	2

#define RX      4
#define RY      11

#define CX	4
#define CY	2

#define BX	14
#define BY	16

#define OX	33
#define OY	2

#define TX	35
#define TY	11


static Dlg_head *conf_dlg;

static int r_but;

#define TOGGLE_VARIABLE 0

extern int use_internal_edit;

int dummy;

static struct {
    char   *text;
    int    *variable;
    void   (*toggle_function)(void);
    WCheck *widget;
    char   *tk;
} check_options [] = {
   {"safe de&Lete",       &know_not_what_am_i_doing, TOGGLE_VARIABLE,0, "safe-del" },
   {"cd follows lin&Ks",  &cd_symlinks,       TOGGLE_VARIABLE,       0, "cd-follow" },
   {"advanced cho&Wn",    &advanced_chfns,    TOGGLE_VARIABLE,       0, "achown" },
   {"l&Ynx-like motion",  &navigate_with_arrows,TOGGLE_VARIABLE,     0, "lynx" },
#ifdef HAVE_GNOME
   {"Animation",          &dummy,             TOGGLE_VARIABLE,       0, "dummy" },
#else
   {"ro&Tating dash",     &nice_rotating_dash,TOGGLE_VARIABLE,       0, "rotating" },
#endif
   {"co&Mplete: show all",&show_all_if_ambiguous,TOGGLE_VARIABLE,    0, "completion" },
   {"&Use internal view", &use_internal_view, TOGGLE_VARIABLE,       0, "view-int" },
   {"use internal ed&It", &use_internal_edit, TOGGLE_VARIABLE,       0, "edit-int" },
   {"auto m&Enus",        &auto_menu,         TOGGLE_VARIABLE,       0, "auto-menus" },
   {"&Auto save setup",   &auto_save_setup,   TOGGLE_VARIABLE,       0, "auto-save" },
   {"shell &Patterns",    &easy_patterns,     TOGGLE_VARIABLE,       0, "shell-patt" },
   {"&Verbose operation", &verbose,           TOGGLE_VARIABLE,       0, "verbose" },
   {"&Fast dir reload",   &fast_reload,       toggle_fast_reload,    0, "fast-reload" },
   {"mi&X all files",     &mix_all_files,     toggle_mix_all_files,  0, "mix-files" },
   {"&Drop down menus",   &drop_menus,        TOGGLE_VARIABLE,       0, "drop-menus" },
   {"ma&Rk moves down",   &mark_moves_down,   TOGGLE_VARIABLE,       0, "mark-moves" },
   {"show &Hidden files", &show_dot_files,    toggle_show_hidden,    0, "show-hidden" },
   {"show &Backup files", &show_backups,      toggle_show_backup,    0, "show-backup" },
   { 0, 0, 0, 0 }
};

static WRadio *pause_radio;

static char *pause_options [3] = {
    "Never",
    "on dumb Terminals",
    "alwaYs" };

static int configure_callback (struct Dlg_head *h, int Id, int Msg)
{
    switch (Msg) {
    case DLG_DRAW:
#ifndef HAVE_X    
	attrset (COLOR_NORMAL);
	dlg_erase (h);
	draw_box (h, 1, 1, 17, 62);
	draw_box (h, PY, PX, 8, 27);
	draw_box (h, OY, OX, 14, 27);
	draw_box (h, RY, RX, 5, 27);

	attrset (COLOR_HOT_NORMAL);
	dlg_move (h, 1, (62 - 20)/2);
	addstr (" Configure options ");
	dlg_move (h, OY, OX+1);
	addstr (" Other options ");
	dlg_move (h, RY, RX+1);
	addstr (" Pause after run... ");
	dlg_move (h, PY, PX+1);
	addstr (" Panel options ");
#endif
	break;

    case DLG_END:
	r_but = Id;
	break;
    }
    return 0;
}

static void init_configure (void)
{
    int i;

    conf_dlg = create_dlg (0, 0, 19, 64, dialog_colors,
			   configure_callback, "[Options Menu]",
			   "option", DLG_CENTER | DLG_GRID);
    x_set_dialog_title (conf_dlg, "Configure options");

    add_widgetl (conf_dlg,
	button_new (BY, BX+26, B_CANCEL, NORMAL_BUTTON, "&Cancel", 0, 0, "button-cancel"),
	XV_WLAY_RIGHTOF);

    add_widgetl (conf_dlg,
	button_new (BY, BX+12, B_EXIT, NORMAL_BUTTON, "&Save", 0, 0, "button-save"),
	XV_WLAY_RIGHTOF);
    
    add_widgetl (conf_dlg,
        button_new (BY, BX, B_ENTER, DEFPUSH_BUTTON, "&Ok", 0, 0, "button-ok"),
        XV_WLAY_CENTERROW);

#define XTRACT(i) *check_options[i].variable, check_options[i].text, check_options [i].tk

    /* Add all the checkboxes */
    for (i = 0; i < 12; i++){
	check_options [i].widget = check_new (OY + (12-i), OX+2, XTRACT(i));
	add_widgetl (conf_dlg, check_options [i].widget,
	    XV_WLAY_BELOWCLOSE);
    }

    pause_radio = radio_new (RY+1, RX+1, 3, pause_options, 1, "pause-radio");
    pause_radio->sel = pause_after_run;
    add_widgetl (conf_dlg, pause_radio, XV_WLAY_BELOWCLOSE);

    for (i = 0; i < 6; i++){
	check_options [i+12].widget = check_new (PY + (6-i), PX+2,
						  XTRACT(i+12));
	add_widgetl (conf_dlg, check_options [i+12].widget,
	    XV_WLAY_BELOWCLOSE);
    }
#ifdef HAVE_XVIEW
     add_widgetl (conf_dlg, label_new (OY, OX + 1, "Other options", "label-other"),
         XV_WLAY_NEXTCOLUMN);
     add_widgetl (conf_dlg, label_new (RY, RX + 1, "Pause after run...", "label-pause"),
         XV_WLAY_BELOWOF);
     add_widgetl (conf_dlg, label_new (PY, PX + 1, "Panel options", "label-panel"),
         XV_WLAY_NEXTCOLUMN);
#endif
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
	sync_profiles ();
    }

    destroy_dlg (conf_dlg);
}

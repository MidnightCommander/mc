/* Some misc dialog boxes for the program.
   
   Copyright (C) 1994, 1995 the Free Software Foundation
   
   Authors: 1994, 1995 Miguel de Icaza
            1995 Jakub Jelinek
   
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#include <config.h>

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "global.h"
#include "tty.h"
#include "win.h"		/* Our window tools */
#include "color.h"		/* Color definitions */
#include "dialog.h"		/* The nice dialog manager */
#include "widget.h"		/* The widgets for the nice dialog manager */
#include "wtools.h"
#include "setup.h"		/* For profile_name */
#include "profile.h"		/* Load/save user formats */
#include "key.h"		/* XCTRL and ALT macros  */
#include "command.h"		/* For cmdline */
#include "dir.h"
#include "panel.h"
#include "boxes.h"
#include "main.h"		/* For the confirm_* variables */
#include "tree.h"
#include "layout.h"		/* for get_nth_panel_name proto */
#include "background.h"		/* task_list */

#ifdef HAVE_CHARSET
#include "charsets.h"
#include "selcodepage.h"
#endif

#ifdef USE_NETCODE
#   include "../vfs/ftpfs.h"
#endif

#ifdef USE_VFS
#include "../vfs/gc.h"
#endif

static int DISPLAY_X = 45, DISPLAY_Y = 14;

static Dlg_head *dd;
static WRadio *my_radio;
static WInput *user;
static WInput *status;
static WCheck *check_status;
static int current_mode;

static char **displays_status;

/* Controls whether the array strings have been translated */
static const char *displays [LIST_TYPES] = {
    N_("&Full file list"),
    N_("&Brief file list"),
    N_("&Long file list"),
    N_("&User defined:")
};

/* Index in displays[] for "user defined" */
#define USER_TYPE 3

static int user_hotkey = 'u';

static cb_ret_t
display_callback (struct Dlg_head *h, dlg_msg_t msg, int parm)
{
    switch (msg) {
    case DLG_UNFOCUS:
	if (dlg_widget_active (my_radio)) {
	    assign_text (status, displays_status[my_radio->sel]);
	    input_set_point (status, 0);
	}
	return MSG_HANDLED;

    case DLG_KEY:
	if (parm == '\n') {
	    if (dlg_widget_active (my_radio)) {
		assign_text (status, displays_status[my_radio->sel]);
		dlg_stop (h);
		return MSG_HANDLED;
	    }

	    if (dlg_widget_active (user)) {
		h->ret_value = B_USER + 6;
		dlg_stop (h);
		return MSG_HANDLED;
	    }

	    if (dlg_widget_active (status)) {
		h->ret_value = B_USER + 7;
		dlg_stop (h);
		return MSG_HANDLED;
	    }
	}

	if (tolower (parm) == user_hotkey && dlg_widget_active (user)
	    && dlg_widget_active (status)) {
	    my_radio->sel = 3;
	    dlg_select_widget (my_radio);	/* force redraw */
	    dlg_select_widget (user);
	    return MSG_HANDLED;
	}
	return MSG_NOT_HANDLED;

    default:
	return default_dlg_callback (h, msg, parm);
    }
}

static void
display_init (int radio_sel, char *init_text, int _check_status,
	      char **_status)
{
    static const char *display_title = N_("Listing mode");
    static int i18n_displays_flag;
    const char *user_mini_status = _("user &Mini status");
    const char *ok_button = _("&OK");
    const char *cancel_button = _("&Cancel");

    static int button_start = 30;

    displays_status = _status;

    if (!i18n_displays_flag) {
	int i, l, maxlen = 0;
	const char *cp;

	display_title = _(display_title);
	for (i = 0; i < LIST_TYPES; i++) {
	    displays[i] = _(displays[i]);
	    if ((l = strlen (displays[i])) > maxlen)
		maxlen = l;
	}

	i = strlen (ok_button) + 5;
	l = strlen (cancel_button) + 3;
	l = max (i, l);

	i = maxlen + l + 16;
	if (i > DISPLAY_X)
	    DISPLAY_X = i;

	i = strlen (user_mini_status) + 13;
	if (i > DISPLAY_X)
	    DISPLAY_X = i;

	i = strlen (display_title) + 10;
	if (i > DISPLAY_X)
	    DISPLAY_X = i;

	button_start = DISPLAY_X - l - 5;

	/* get hotkey of user-defined format string */
	cp = strchr (displays[USER_TYPE], '&');
	if (cp != NULL && *++cp != '\0')
	    user_hotkey = tolower ((unsigned char) *cp);

	i18n_displays_flag = 1;
    }
    dd = create_dlg (0, 0, DISPLAY_Y, DISPLAY_X, dialog_colors,
		     display_callback, "[Listing Mode...]", display_title,
		     DLG_CENTER | DLG_REVERSE);

    add_widget (dd,
		button_new (4, button_start, B_CANCEL, NORMAL_BUTTON,
			    cancel_button, 0));

    add_widget (dd,
		button_new (3, button_start, B_ENTER, DEFPUSH_BUTTON,
			    ok_button, 0));

    status =
	input_new (10, 9, INPUT_COLOR, DISPLAY_X - 14, _status[radio_sel],
		   "mini-input");
    add_widget (dd, status);
    input_set_point (status, 0);

    check_status =
	check_new (9, 5, _check_status, user_mini_status);
    add_widget (dd, check_status);

    user =
	input_new (7, 9, INPUT_COLOR, DISPLAY_X - 14, init_text,
		   "user-fmt-input");
    add_widget (dd, user);
    input_set_point (user, 0);

    my_radio = radio_new (3, 5, LIST_TYPES, displays, 1);
    my_radio->sel = my_radio->pos = current_mode;
    add_widget (dd, my_radio);
}

int
display_box (WPanel *panel, char **userp, char **minip, int *use_msformat, int num)
{
    int result, i;
    char *section = NULL;
    const char *p;

    if (!panel) {
        p = get_nth_panel_name (num);
        panel = g_new (WPanel, 1);
        panel->list_type = list_full;
        panel->user_format = g_strdup (DEFAULT_USER_FORMAT);
        panel->user_mini_status = 0;
	for (i = 0; i < LIST_TYPES; i++)
    	    panel->user_status_format[i] = g_strdup (DEFAULT_USER_FORMAT);
        section = g_strconcat ("Temporal:", p, (char *) NULL);
        if (!profile_has_section (section, profile_name)) {
            g_free (section);
            section = g_strdup (p);
        }
        panel_load_setup (panel, section);
        g_free (section);
    }

    current_mode = panel->list_type;
    display_init (current_mode, panel->user_format, 
	panel->user_mini_status, panel->user_status_format);
		  
    run_dlg (dd);

    result = -1;
    
    if (section) {
        g_free (panel->user_format);
	for (i = 0; i < LIST_TYPES; i++)
	   g_free (panel->user_status_format [i]);
        g_free (panel);
    }
    
    if (dd->ret_value != B_CANCEL){
	result = my_radio->sel;
	*userp = g_strdup (user->buffer);
	*minip = g_strdup (status->buffer);
	*use_msformat = check_status->state & C_BOOL;
    }
    destroy_dlg (dd);

    return result;
}

static int SORT_X = 40, SORT_Y = 14;

static const char *sort_orders_names [SORT_TYPES];

sortfn *
sort_box (sortfn *sort_fn, int *reverse, int *case_sensitive)
{
    int i, r, l;
    sortfn *result;
    WCheck *c, *case_sense;

    const char *ok_button = _("&OK");
    const char *cancel_button = _("&Cancel");
    const char *reverse_label = _("&Reverse");
    const char *case_label = _("case sensi&tive");
    const char *sort_title = _("Sort order");

    static int i18n_sort_flag = 0, check_pos = 0, button_pos = 0;

    if (!i18n_sort_flag) {
	int maxlen = 0;
	for (i = SORT_TYPES - 1; i >= 0; i--) {
	    sort_orders_names[i] = _(sort_orders[i].sort_name);
	    r = strlen (sort_orders_names[i]);
	    if (r > maxlen)
		maxlen = r;
	}

	check_pos = maxlen + 9;

	r = strlen (reverse_label) + 4;
	i = strlen (case_label) + 4;
	if (i > r)
	    r = i;

	l = strlen (ok_button) + 6;
	i = strlen (cancel_button) + 4;
	if (i > l)
	    l = i;

	i = check_pos + max (r, l) + 2;

	if (i > SORT_X)
	    SORT_X = i;

	i = strlen (sort_title) + 6;
	if (i > SORT_X)
	    SORT_X = i;

	button_pos = SORT_X - l - 2;

	i18n_sort_flag = 1;
    }

    result = 0;

    for (i = 0; i < SORT_TYPES; i++)
	if ((sortfn *) (sort_orders[i].sort_fn) == sort_fn) {
	    current_mode = i;
	    break;
	}

    dd = create_dlg (0, 0, SORT_Y, SORT_X, dialog_colors, NULL,
		     "[Sort Order...]", sort_title, DLG_CENTER | DLG_REVERSE);

    add_widget (dd,
		button_new (10, button_pos, B_CANCEL, NORMAL_BUTTON,
			    cancel_button, 0));

    add_widget (dd,
		button_new (9, button_pos, B_ENTER, DEFPUSH_BUTTON,
			    ok_button, 0));

    case_sense = check_new (4, check_pos, *case_sensitive, case_label);
    add_widget (dd, case_sense);
    c = check_new (3, check_pos, *reverse, reverse_label);
    add_widget (dd, c);

    my_radio = radio_new (3, 3, SORT_TYPES, sort_orders_names, 1);
    my_radio->sel = my_radio->pos = current_mode;

    add_widget (dd, my_radio);
    run_dlg (dd);

    r = dd->ret_value;
    if (r != B_CANCEL) {
	result = (sortfn *) sort_orders[my_radio->sel].sort_fn;
	*reverse = c->state & C_BOOL;
	*case_sensitive = case_sense->state & C_BOOL;
    } else
	result = sort_fn;
    destroy_dlg (dd);

    return result;
}

#define CONFY 11
#define CONFX 46

static int my_delete;
static int my_directory_hotlist_delete;
static int my_overwrite;
static int my_execute;
static int my_exit;

static QuickWidget conf_widgets [] = {
{ quick_button,   4, 6, 4, CONFY, N_("&Cancel"),
      0, B_CANCEL, 0, 0, NULL },
{ quick_button,   4, 6, 3, CONFY, N_("&OK"),
      0, B_ENTER, 0, 0, NULL },

{ quick_checkbox, 1, 13, 7, CONFY, N_(" confirm di&Rectory hotlist delete "),
      11, 0, &my_directory_hotlist_delete, NULL, NULL },
{ quick_checkbox, 1, 13, 6, CONFY, N_(" confirm &Exit "),
      9, 0, &my_exit, 0, NULL },
{ quick_checkbox, 1, 13, 5, CONFY, N_(" confirm e&Xecute "),
      10, 0, &my_execute, 0, NULL },
{ quick_checkbox, 1, 13, 4, CONFY, N_(" confirm o&Verwrite "),
      10, 0, &my_overwrite, 0, NULL },
{ quick_checkbox, 1, 13, 3, CONFY, N_(" confirm &Delete "),
      9, 0, &my_delete, 0, NULL },
NULL_QuickWidget
};

static QuickDialog confirmation =
    { CONFX, CONFY, -1, -1, N_(" Confirmation "), "[Confirmation]",
    conf_widgets, 0
};

void
confirm_box (void)
{

#ifdef ENABLE_NLS
	static int i18n_flag = 0;
	
	if (!i18n_flag)
	{
		register int i = sizeof(conf_widgets)/sizeof(QuickWidget) - 1;
		int l1, maxlen = 0;
		while (i--)
		{
			conf_widgets [i].text = _(conf_widgets [i].text);
			l1 = strlen (conf_widgets [i].text) + 3;
			if (l1 > maxlen)
				maxlen = l1;
		}

		/*
		 * If buttons start on 4/6, checkboxes (with some add'l space)
		 * must take not more than it.
		 */
		confirmation.xlen = (maxlen + 5) * 6 / 4;

		/*
		 * And this for the case when buttons with some space to the right
		 * do not fit within 2/6
		 */
		l1 = strlen (conf_widgets [0].text) + 3;
		i = strlen (conf_widgets [1].text) + 5;
		if (i > l1)
			l1 = i;

		i = (l1 + 3) * 6 / 2;
		if (i > confirmation.xlen)
			confirmation.xlen = i;

		confirmation.title = _(confirmation.title);
		
		i18n_flag = confirmation.i18n = 1;
	}

#endif /* ENABLE_NLS */

    my_delete    = confirm_delete;
    my_overwrite = confirm_overwrite;
    my_execute   = confirm_execute;
    my_exit      = confirm_exit;
    my_directory_hotlist_delete = confirm_directory_hotlist_delete;

    if (quick_dialog (&confirmation) != B_CANCEL){
	confirm_delete    = my_delete;
	confirm_overwrite = my_overwrite;
	confirm_execute   = my_execute;
	confirm_exit      = my_exit;
	confirm_directory_hotlist_delete = my_directory_hotlist_delete;
    }
}

#define DISPY 11
#define DISPX 46


#ifndef HAVE_CHARSET

static int new_mode;
static int new_meta;

static const char *display_bits_str [] =
{ N_("Full 8 bits output"), N_("ISO 8859-1"), N_("7 bits") };

static QuickWidget display_widgets [] = {
{ quick_button,   4,  6,    4, DISPY, N_("&Cancel"),
      0, B_CANCEL, 0, 0, NULL },
{ quick_button,   4,  6,    3, DISPY, N_("&OK"),
      0, B_ENTER, 0, 0, NULL },
{ quick_checkbox, 4, DISPX, 7, DISPY, N_("F&ull 8 bits input"),
      0, 0, &new_meta, 0, NULL },
{ quick_radio,    4, DISPX, 3, DISPY, "", 3, 0,
      &new_mode, const_cast(char **, display_bits_str), NULL },
NULL_QuickWidget
};

static QuickDialog display_bits =
{ DISPX, DISPY, -1, -1, N_(" Display bits "), "[Display bits]",
  display_widgets, 0 };

void
display_bits_box (void)
{
    int current_mode;

#ifdef ENABLE_NLS
	static int i18n_flag = 0;
	if (!i18n_flag)
	{
		register int i;
		int l1, maxlen = 0;
		for (i = 0; i < 3; i++)
		{
			display_widgets [i].text = _(display_widgets[i].text);
			display_bits_str [i] = _(display_bits_str [i]);
			l1 = strlen (display_bits_str [i]);
			if (l1 > maxlen)
				maxlen = l1;
		}
		l1 = strlen (display_widgets [2].text);
		if (l1 > maxlen)
			maxlen = l1;
		

		display_bits.xlen = (maxlen + 5) * 6 / 4;

		/* See above confirm_box */
		l1 = strlen (display_widgets [0].text) + 3;
		i = strlen (display_widgets [1].text) + 5;
		if (i > l1)
			l1 = i;

		i = (l1 + 3) * 6 / 2;
		if (i > display_bits.xlen)
			display_bits.xlen = i;

		display_bits.title = _(display_bits.title);
		i18n_flag = display_bits.i18n = 1;
	}

#endif /* ENABLE_NLS */

    if (full_eight_bits)
	current_mode = 0;
    else if (eight_bit_clean)
	current_mode = 1;
    else
	current_mode = 2;

    display_widgets [3].value = current_mode;
    new_meta = !use_8th_bit_as_meta;
    if (quick_dialog (&display_bits) != B_ENTER)
	    return;

    eight_bit_clean = new_mode < 2;
    full_eight_bits = new_mode == 0;
#ifndef HAVE_SLANG
    meta (stdscr, eight_bit_clean);
#else
    SLsmg_Display_Eight_Bit = full_eight_bits ? 128 : 160;
#endif
    use_8th_bit_as_meta = !new_meta;
}


#else /* HAVE_CHARSET */


static int new_display_codepage;

static WLabel *cplabel;
static WCheck *inpcheck;

static int
sel_charset_button (int action)
{
    const char *cpname;
    char buf[64];
    new_display_codepage = select_charset (new_display_codepage, 1);
    cpname = (new_display_codepage < 0)
	? _("Other 8 bit")
	: codepages[new_display_codepage].name;

    /* avoid strange bug with label repainting */
    g_snprintf (buf, sizeof (buf), "%-27s", cpname);
    label_set_text (cplabel, buf);
    return 0;
}

static Dlg_head *
init_disp_bits_box (void)
{
    const char *cpname;
    Dlg_head *dbits_dlg;

    do_refresh ();

    dbits_dlg =
	create_dlg (0, 0, DISPY, DISPX, dialog_colors, NULL,
		    "[Display bits]", _(" Display bits "), DLG_CENTER | DLG_REVERSE);

    add_widget (dbits_dlg,
		label_new (3, 4, _("Input / display codepage:")));

    cpname = (new_display_codepage < 0)
	? _("Other 8 bit")
	: codepages[new_display_codepage].name;
    cplabel = label_new (4, 4, cpname);
    add_widget (dbits_dlg, cplabel);

    add_widget (dbits_dlg,
		button_new (DISPY - 3, DISPX / 2 + 3, B_CANCEL,
			    NORMAL_BUTTON, _("&Cancel"), 0));
    add_widget (dbits_dlg,
		button_new (DISPY - 3, 7, B_ENTER, NORMAL_BUTTON, _("&OK"),
			    0));

    inpcheck =
	check_new (6, 4, !use_8th_bit_as_meta, _("F&ull 8 bits input"));
    add_widget (dbits_dlg, inpcheck);

    cpname = _("&Select");
    add_widget (dbits_dlg,
		button_new (4, DISPX - 8 - strlen (cpname), B_USER,
			    NORMAL_BUTTON, cpname, sel_charset_button));

    return dbits_dlg;
}

void
display_bits_box (void)
{
    Dlg_head *dbits_dlg;
    new_display_codepage = display_codepage;

    application_keypad_mode ();
    dbits_dlg = init_disp_bits_box ();

    run_dlg (dbits_dlg);

    if (dbits_dlg->ret_value == B_ENTER) {
	const char *errmsg;
	display_codepage = new_display_codepage;
	errmsg =
	    init_translation_table (source_codepage, display_codepage);
	if (errmsg)
	    message (1, MSG_ERROR, "%s", errmsg);
#ifndef HAVE_SLANG
	meta (stdscr, display_codepage != 0);
#else
	SLsmg_Display_Eight_Bit = (display_codepage != 0
				   && display_codepage != 1) ? 128 : 160;
#endif
	use_8th_bit_as_meta = !(inpcheck->state & C_BOOL);
    }
    destroy_dlg (dbits_dlg);
    repaint_screen ();
}

#endif /* HAVE_CHARSET */


#define TREE_Y 20
#define TREE_X 60

static cb_ret_t
tree_callback (struct Dlg_head *h, dlg_msg_t msg, int parm)
{
    switch (msg) {

    case DLG_POST_KEY:
	/* The enter key will be processed by the tree widget */
	if (parm == '\n') {
	    h->ret_value = B_ENTER;
	    dlg_stop (h);
	}
	return MSG_HANDLED;
	
    default:
	return default_dlg_callback (h, msg, parm);
    }
}

/* Show tree in a box, not on a panel */
char *
tree_box (const char *current_dir)
{
    WTree    *mytree;
    Dlg_head *dlg;
    char     *val;
    WButtonBar *bar;

    /* Create the components */
    dlg = create_dlg (0, 0, TREE_Y, TREE_X, dialog_colors,
		      tree_callback, "[Directory Tree]", NULL, DLG_CENTER | DLG_REVERSE);
    mytree = tree_new (0, 2, 2, TREE_Y - 6, TREE_X - 5);
    add_widget (dlg, mytree);
    bar = buttonbar_new(1);
    add_widget (dlg, bar);
    ((Widget *) bar)->x = 0;
    ((Widget *) bar)->y = LINES - 1;
    
    run_dlg (dlg);
    if (dlg->ret_value == B_ENTER)
	val = g_strdup (tree_selected_name (mytree));
    else
	val = 0;
    
    destroy_dlg (dlg);
    return val;
}

#ifdef USE_VFS

#if defined(USE_NETCODE)
#define VFSY 17
#define VFS_WIDGETBASE 10
#else
#define VFSY 8
#define VFS_WIDGETBASE 0
#endif

#define VFSX 56

static char *ret_timeout;

#if defined(USE_NETCODE)
static char *ret_passwd;
static char *ret_directory_timeout;
static char *ret_ftp_proxy;
static int ret_use_netrc;
static int ret_ftpfs_use_passive_connections;
static int ret_ftpfs_use_passive_connections_over_proxy;
#endif

static QuickWidget confvfs_widgets [] = {
{ quick_button,   30,  VFSX,    VFSY - 3, VFSY, N_("&Cancel"),
      0, B_CANCEL, 0, 0, NULL },
{ quick_button,   12, VFSX,    VFSY - 3, VFSY, N_("&OK"),
      0, B_ENTER, 0, 0, NULL },
#if defined(USE_NETCODE)
{ quick_checkbox,  4, VFSX, 12, VFSY, N_("Use passive mode over pro&xy"), 0, 0,
      &ret_ftpfs_use_passive_connections_over_proxy, 0, NULL },
{ quick_checkbox,  4, VFSX, 11, VFSY, N_("Use &passive mode"), 0, 0,
      &ret_ftpfs_use_passive_connections, 0, NULL },
{ quick_checkbox,  4, VFSX, 10, VFSY, N_("&Use ~/.netrc"), 0, 0,
      &ret_use_netrc, 0, NULL },
{ quick_input,     4, VFSX, 9, VFSY, "", 48, 0, 0, &ret_ftp_proxy,
      "input-ftp-proxy" },
{ quick_checkbox,  4, VFSX, 8, VFSY, N_("&Always use ftp proxy"), 0, 0,
      &ftpfs_always_use_proxy, 0, NULL },
{ quick_label,    49, VFSX, 7, VFSY, N_("sec"),
      0, 0, 0, 0, NULL },
{ quick_input,    38, VFSX, 7, VFSY, "", 10, 0, 0, &ret_directory_timeout,
      "input-timeout" },
{ quick_label,     4, VFSX, 7, VFSY, N_("ftpfs directory cache timeout:"),
      0, 0, 0, 0, NULL },
{ quick_input,     4, VFSX, 6, VFSY, "", 48, 0, 0, &ret_passwd,
      "input-passwd" },
{ quick_label,     4, VFSX, 5, VFSY, N_("ftp anonymous password:"),
      0, 0, 0, 0, NULL },
#endif
{ quick_label,    49, VFSX, 3, VFSY, "sec",
      0, 0, 0, 0, NULL },
{ quick_input,    38, VFSX, 3, VFSY, "", 10, 0, 0, &ret_timeout, 
      "input-timo-vfs" },
{ quick_label,    4,  VFSX, 3, VFSY, N_("Timeout for freeing VFSs:"), 
      0, 0, 0, 0, NULL },
NULL_QuickWidget
};

static QuickDialog confvfs_dlg =
    { VFSX, VFSY, -1, -1, N_(" Virtual File System Setting "),
"[Virtual FS]", confvfs_widgets, 0 };

void
configure_vfs (void)
{
    char buffer2[BUF_TINY];
#if defined(USE_NETCODE)
    char buffer3[BUF_TINY];

    ret_use_netrc = use_netrc;
    ret_ftpfs_use_passive_connections = ftpfs_use_passive_connections;
    ret_ftpfs_use_passive_connections_over_proxy = ftpfs_use_passive_connections_over_proxy;
    g_snprintf(buffer3, sizeof (buffer3), "%i", ftpfs_directory_timeout);
    confvfs_widgets[8].text = buffer3;
    confvfs_widgets[10].text = ftpfs_anonymous_passwd;
    confvfs_widgets[5].text = ftpfs_proxy_host;
#endif
    g_snprintf (buffer2, sizeof (buffer2), "%i", vfs_timeout);
    confvfs_widgets [3 + VFS_WIDGETBASE].text = buffer2;

    if (quick_dialog (&confvfs_dlg) != B_CANCEL) {
        vfs_timeout = atoi (ret_timeout);
        g_free (ret_timeout);
        if (vfs_timeout < 0 || vfs_timeout > 10000)
            vfs_timeout = 10;
#if defined(USE_NETCODE)
	g_free (ftpfs_anonymous_passwd);
	ftpfs_anonymous_passwd = ret_passwd;
	g_free (ftpfs_proxy_host);
	ftpfs_proxy_host = ret_ftp_proxy;
	ftpfs_directory_timeout = atoi(ret_directory_timeout);
	use_netrc = ret_use_netrc;
	ftpfs_use_passive_connections = ret_ftpfs_use_passive_connections;
	ftpfs_use_passive_connections_over_proxy = ret_ftpfs_use_passive_connections_over_proxy;
	g_free (ret_directory_timeout);
#endif
    }
}

#endif	/* USE_VFS */

char *
cd_dialog (void)
{
    QuickDialog Quick_input;
    QuickWidget quick_widgets [] = {
	{ quick_input,  6, 57, 2, 0, "", 50, 0, 0, 0, "input" },
	{ quick_label,  3, 57, 2, 0, "",  0, 0, 0, 0, NULL },
	NULL_QuickWidget
    };
    char *my_str;
    int len;

    Quick_input.xlen  = 57;
    Quick_input.title = _("Quick cd");
    Quick_input.help  = "[Quick cd]";
    quick_widgets [0].value = 2; /* want cd like completion */
    quick_widgets [1].text = _("cd");
    quick_widgets [1].y_divisions =
	quick_widgets [0].y_divisions = Quick_input.ylen = 5;

    len = strlen (quick_widgets [1].text);

    quick_widgets [0].relative_x =
	quick_widgets [1].relative_x + len + 1;

    Quick_input.xlen = len + quick_widgets [0].hotkey_pos + 7;
	quick_widgets [0].x_divisions =
		quick_widgets [1].x_divisions = Quick_input.xlen;

    Quick_input.i18n = 1;
    Quick_input.xpos = 2;
    Quick_input.ypos = LINES - 2 - Quick_input.ylen;
    quick_widgets [0].str_result = &my_str;
    
    Quick_input.widgets = quick_widgets;
    if (quick_dialog (&Quick_input) != B_CANCEL){
	return my_str;
    } else
	return 0;
}

void
symlink_dialog (const char *existing, const char *new, char **ret_existing,
		char **ret_new)
{
    QuickDialog Quick_input;
    QuickWidget quick_widgets[] = {
	{quick_button, 50, 80, 6, 8, N_("&Cancel"), 0, B_CANCEL, 0, 0,
	 NULL},
	{quick_button, 16, 80, 6, 8, N_("&OK"), 0, B_ENTER, 0, 0, NULL},
	{quick_input, 4, 80, 5, 8, "", 58, 0, 0, 0, "input-1"},
	{quick_label, 4, 80, 4, 8, N_("Symbolic link filename:"), 0, 0, 0,
	 0, NULL},
	{quick_input, 4, 80, 3, 8, "", 58, 0, 0, 0, "input-2"},
	{quick_label, 4, 80, 2, 8,
	 N_("Existing filename (filename symlink will point to):"), 0, 0,
	 0, 0, NULL},
	NULL_QuickWidget
    };

    Quick_input.xlen = 64;
    Quick_input.ylen = 12;
    Quick_input.title = N_("Symbolic link");
    Quick_input.help = "[File Menu]";
    Quick_input.i18n = 0;
    quick_widgets[2].text = new;
    quick_widgets[4].text = existing;
    Quick_input.xpos = -1;
    quick_widgets[2].str_result = ret_new;
    quick_widgets[4].str_result = ret_existing;

    Quick_input.widgets = quick_widgets;
    if (quick_dialog (&Quick_input) == B_CANCEL) {
	*ret_new = NULL;
	*ret_existing = NULL;
    }
}

#ifdef WITH_BACKGROUND
#define B_STOP   (B_USER+1)
#define B_RESUME (B_USER+2)
#define B_KILL   (B_USER+3)

static int JOBS_X = 60;
#define JOBS_Y 15
static WListbox *bg_list;
static Dlg_head *jobs_dlg;

static void
jobs_fill_listbox (void)
{
    static const char *state_str [2];
    TaskList *tl = task_list;

    if (!state_str [0]){
       state_str [0] = _("Running ");
       state_str [1] = _("Stopped");
    }
    
    while (tl){
	char *s;

	s = g_strconcat (state_str [tl->state], " ", tl->info, (char *) NULL);
	listbox_add_item (bg_list, LISTBOX_APPEND_AT_END, 0, s, (void *) tl);
	g_free (s);
	tl = tl->next;
    }
}
	
static int
task_cb (int action)
{
    TaskList *tl;
    int sig = 0;
    
    if (!bg_list->list)
	return 0;

    /* Get this instance information */
    tl = (TaskList *) bg_list->current->data;
    
#  ifdef SIGTSTP
    if (action == B_STOP){
	sig   = SIGSTOP;
	tl->state = Task_Stopped;
    } else if (action == B_RESUME){
	sig   = SIGCONT;
	tl->state = Task_Running;
    } else 
#  endif
    if (action == B_KILL){
	sig = SIGKILL;
    }
    
    if (sig == SIGINT)
	unregister_task_running (tl->pid, tl->fd);

    kill (tl->pid, sig);
    listbox_remove_list (bg_list);
    jobs_fill_listbox ();

    /* This can be optimized to just redraw this widget :-) */
    dlg_redraw (jobs_dlg);
    
    return 0;
}

static struct 
{
	const char* name;
	int xpos;
	int value;
	int (*callback)(int);
} 
job_buttons [] =
{
	{N_("&Stop"),   3,  B_STOP,   task_cb},
	{N_("&Resume"), 12, B_RESUME, task_cb},
	{N_("&Kill"),   23, B_KILL,   task_cb},
	{N_("&OK"),     35, B_CANCEL, NULL   }
};

void
jobs_cmd (void)
{
	register int i;
	int n_buttons = sizeof (job_buttons) / sizeof (job_buttons[0]);

#ifdef ENABLE_NLS
	static int i18n_flag = 0;
	if (!i18n_flag)
	{
		int startx = job_buttons [0].xpos;
		int len;

		for (i = 0; i < n_buttons; i++)
		{
			job_buttons [i].name = _(job_buttons [i].name);

			len = strlen (job_buttons [i].name) + 4;
			JOBS_X = max (JOBS_X, startx + len + 3);

			job_buttons [i].xpos = startx;
			startx += len;
		}

		/* Last button - Ok a.k.a. Cancel :) */
		job_buttons [n_buttons - 1].xpos =
			JOBS_X - strlen (job_buttons [n_buttons - 1].name) - 7;

		i18n_flag = 1;
	}
#endif /* ENABLE_NLS */

    jobs_dlg = create_dlg (0, 0, JOBS_Y, JOBS_X, dialog_colors, NULL,
			   "[Background jobs]", _("Background Jobs"),
			   DLG_CENTER | DLG_REVERSE);

    bg_list = listbox_new (2, 3, JOBS_X-7, JOBS_Y-9, 0);
    add_widget (jobs_dlg, bg_list);

	i = n_buttons;
	while (i--)
	{
		add_widget (jobs_dlg, button_new (JOBS_Y-4, 
			job_buttons [i].xpos, job_buttons [i].value,
			NORMAL_BUTTON, job_buttons [i].name, 
			job_buttons [i].callback));
	}
	
    /* Insert all of task information in the list */
    jobs_fill_listbox ();
    run_dlg (jobs_dlg);
    
    destroy_dlg (jobs_dlg);
}
#endif /* WITH_BACKGROUND */

#ifdef WITH_SMBFS
struct smb_authinfo *
vfs_smb_get_authinfo (const char *host, const char *share, const char *domain,
		      const char *user)
{
    static int dialog_x = 44;
    enum { b0 = 3, dialog_y = 12};
    struct smb_authinfo *return_value;
    static const char* labs[] = {N_("Domain:"), N_("Username:"), N_("Password:")};
    static const char* buts[] = {N_("&OK"), N_("&Cancel")};
    static int ilen = 30, istart = 14;
    static int b2 = 30;
    char *title;
    WInput *in_password;
    WInput *in_user;
    WInput *in_domain;
    Dlg_head *auth_dlg;

#ifdef ENABLE_NLS
    static int i18n_flag = 0;
    
    if (!i18n_flag)
    {
        register int i = sizeof(labs)/sizeof(labs[0]);
        int l1, maxlen = 0;
        
        while (i--)
        {
            l1 = strlen (labs [i] = _(labs [i]));
            if (l1 > maxlen)
                maxlen = l1;
        }
        i = maxlen + ilen + 7;
        if (i > dialog_x)
            dialog_x = i;
        
        for (i = sizeof(buts)/sizeof(buts[0]), l1 = 0; i--; )
        {
            l1 += strlen (buts [i] = _(buts [i]));
        }
        l1 += 15;
        if (l1 > dialog_x)
            dialog_x = l1;

        ilen = dialog_x - 7 - maxlen; /* for the case of very long buttons :) */
        istart = dialog_x - 3 - ilen;
        
        b2 = dialog_x - (strlen(buts[1]) + 6);
        
        i18n_flag = 1;
    }
    
#endif /* ENABLE_NLS */

    if (!domain)
    domain = "";
    if (!user)
    user = "";

    title = g_strdup_printf (_("Password for \\\\%s\\%s"), host, share);

    auth_dlg = create_dlg (0, 0, dialog_y, dialog_x, dialog_colors, NULL,
			   "[Smb Authinfo]", title, DLG_CENTER | DLG_REVERSE);

    g_free (title);

    in_user  = input_new (5, istart, INPUT_COLOR, ilen, user, "auth_name");
    add_widget (auth_dlg, in_user);

    in_domain = input_new (3, istart, INPUT_COLOR, ilen, domain, "auth_domain");
    add_widget (auth_dlg, in_domain);
    add_widget (auth_dlg, button_new (9, b2, B_CANCEL, NORMAL_BUTTON,
                 buts[1], 0));
    add_widget (auth_dlg, button_new (9, b0, B_ENTER, DEFPUSH_BUTTON,
                 buts[0], 0));

    in_password  = input_new (7, istart, INPUT_COLOR, ilen, "", "auth_password");
    in_password->completion_flags = 0;
    in_password->is_password = 1;
    add_widget (auth_dlg, in_password);

    add_widget (auth_dlg, label_new (7, 3, labs[2]));
    add_widget (auth_dlg, label_new (5, 3, labs[1]));
    add_widget (auth_dlg, label_new (3, 3, labs[0]));

    run_dlg (auth_dlg);

    switch (auth_dlg->ret_value) {
    case B_CANCEL:
        return_value = 0;
        break;
    default:
        return_value = g_new (struct smb_authinfo, 1);
        if (return_value) {
            return_value->host = g_strdup (host);
            return_value->share = g_strdup (share);
            return_value->domain = g_strdup (in_domain->buffer);
            return_value->user = g_strdup (in_user->buffer);
            return_value->password = g_strdup (in_password->buffer);
        }
    }

    destroy_dlg (auth_dlg);
             
    return return_value;
}
#endif /* WITH_SMBFS */

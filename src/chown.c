/* Chown command -- for the Midnight Commander
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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
 */

#include <config.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>	/* For errno on SunOS systems	      */

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include "global.h"
#include "tty.h"
#include "win.h"
#include "color.h"
#include "dlg.h"
#include "widget.h"
#include "dialog.h"	/* For do_refresh() */

/* Needed for the extern declarations of integer parameters */
#include "dir.h"
#include "panel.h"		/* Needed for the externs */
#include "chmod.h"
#include "main.h"
#include "chown.h"
#include "wtools.h"		/* For init_box_colors */
#include "../vfs/vfs.h"

#define UX		5
#define UY		2

#define GX              27
#define GY              2

#define BX		5
#define BY		15

#define TX              50
#define TY              2

#define BUTTONS		5

#define B_SETALL        B_USER
#define B_SETUSR        B_USER + 1
#define B_SETGRP        B_USER + 2

static int need_update, end_chown;
static int current_file;
static int single_set;
static WListbox *l_user, *l_group;

static struct {
    int ret_cmd, flags, y, x;
    char *text;
} chown_but[BUTTONS] = {
    { B_CANCEL, NORMAL_BUTTON,  0, 53, N_("&Cancel") },
    { B_ENTER,  DEFPUSH_BUTTON, 0, 40, N_("&Set") },
    { B_SETUSR, NORMAL_BUTTON,  0, 25, N_("Set &users") },
    { B_SETGRP, NORMAL_BUTTON,  0, 11, N_("Set &groups") },
    { B_SETALL, NORMAL_BUTTON,  0, 0,  N_("Set &all") },
};

#define LABELS 5 
static struct {
    int y, x;
    WLabel *l;
} chown_label [LABELS] = {
{ TY+2, TX+2 },
{ TY+4, TX+2 },
{ TY+6, TX+2 },
{ TY+8, TX+2 },
{ TY+10,TX+2 }
};

static void
chown_refresh (Dlg_head *h)
{
    common_dialog_repaint (h);

    attrset (COLOR_NORMAL);

    draw_box (h, UY, UX, 12, 21);
    draw_box (h, GY, GX, 12, 21);
    draw_box (h, TY, TX, 12, 19);

    dlg_move (h, TY + 1, TX + 1);
    addstr (_(" Name "));
    dlg_move (h, TY + 3, TX + 1);
    addstr (_(" Owner name "));
    dlg_move (h, TY + 5, TX + 1);
    addstr (_(" Group name "));
    dlg_move (h, TY + 7, TX + 1);
    addstr (_(" Size "));
    dlg_move (h, TY + 9, TX + 1);
    addstr (_(" Permission "));
    
    attrset (COLOR_HOT_NORMAL);
    dlg_move (h, UY, UX + 1);
    addstr (_(" User name "));
    dlg_move (h, GY, GX + 1);
    addstr (_(" Group name "));
    dlg_move (h, TY, TX + 1);
    addstr (_(" File "));
}

static char *
next_file (void)
{
    while (!cpanel->dir.list[current_file].f.marked)
	current_file++;

    return cpanel->dir.list[current_file].fname;
}

static cb_ret_t
chown_callback (Dlg_head *h, dlg_msg_t msg, int parm)
{
    switch (msg) {
    case DLG_DRAW:
	chown_refresh (h);
	return MSG_HANDLED;

    default:
	return default_dlg_callback (h, msg, parm);
    }
}

static Dlg_head *
init_chown (void)
{
    int i;
    struct passwd *l_pass;
    struct group *l_grp;
    Dlg_head *ch_dlg;

    do_refresh ();
    end_chown = need_update = current_file = 0;
    single_set = (cpanel->marked < 2) ? 3 : 0;

    ch_dlg =
	create_dlg (0, 0, 18, 74, dialog_colors, chown_callback, "[Chown]",
		    _(" Chown command "), DLG_CENTER);

    for (i = 0; i < BUTTONS - single_set; i++)
	add_widget (ch_dlg,
		    button_new (BY + chown_but[i].y, BX + chown_but[i].x,
				chown_but[i].ret_cmd, chown_but[i].flags,
				_(chown_but[i].text), 0));

    /* Add the widgets for the file information */
    for (i = 0; i < LABELS; i++) {
	chown_label[i].l =
	    label_new (chown_label[i].y, chown_label[i].x, "");
	add_widget (ch_dlg, chown_label[i].l);
    }

    /* get new listboxes */
    l_user = listbox_new (UY + 1, UX + 1, 19, 10, NULL);
    l_group = listbox_new (GY + 1, GX + 1, 19, 10, NULL);

    /* add fields for unknown names (numbers) */
    listbox_add_item (l_user, 0, 0, _("<Unknown user>"), NULL);
    listbox_add_item (l_group, 0, 0, _("<Unknown group>"), NULL);

    /* get and put user names in the listbox */
    setpwent ();
    while ((l_pass = getpwent ())) {
	listbox_add_item (l_user, 0, 0, l_pass->pw_name, NULL);
    }
    endpwent ();

    /* get and put group names in the listbox */
    setgrent ();
    while ((l_grp = getgrent ())) {
	listbox_add_item (l_group, 0, 0, l_grp->gr_name, NULL);
    }
    endgrent ();

    /* add listboxes to the dialogs */
    add_widget (ch_dlg, l_group);
    add_widget (ch_dlg, l_user);

    return ch_dlg;
}

static void
chown_done (void)
{
    if (need_update)
	update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    repaint_screen ();
}

static inline void
do_chown (uid_t u, gid_t g)
{
    if (mc_chown (cpanel->dir.list [current_file].fname, u, g) == -1)
	message (1, MSG_ERROR, _(" Cannot chown \"%s\" \n %s "),
	     cpanel->dir.list [current_file].fname, unix_error_string (errno));

    do_file_mark (cpanel, current_file, 0);
}

static void
apply_chowns (uid_t u, gid_t g)
{
    char *fname;
  
    need_update = end_chown = 1;
    do_chown (u,g);
  
    do {
	fname = next_file ();
    
	do_chown (u,g);
    } while (cpanel->marked);
}

#define chown_label(n,txt) label_set_text (chown_label [n].l, txt)

void
chown_cmd (void)
{
    char *fname;
    struct stat sf_stat;
    WLEntry *fe;
    Dlg_head *ch_dlg;
    uid_t new_user;
    gid_t new_group;
    char  buffer [BUF_TINY];

    do {			/* do while any files remaining */
	ch_dlg = init_chown ();
	new_user = new_group = -1;

	if (cpanel->marked)
	    fname = next_file ();	        /* next marked file */
	else
	    fname = selection (cpanel)->fname;	        /* single file */
	
	if (!stat_file (fname, &sf_stat)){	/* get status of file */
	    destroy_dlg (ch_dlg);
	    break;
	}
	
	/* select in listboxes */
	fe = listbox_search_text (l_user, get_owner(sf_stat.st_uid));
	if (fe)
	    listbox_select_entry (l_user, fe);
    
	fe = listbox_search_text (l_group, get_group(sf_stat.st_gid));
	if (fe)
	    listbox_select_entry (l_group, fe);

        chown_label (0, name_trunc (fname, 15));
        chown_label (1, name_trunc (get_owner (sf_stat.st_uid), 15));
	chown_label (2, name_trunc (get_group (sf_stat.st_gid), 15));
	size_trunc_len (buffer, 15, sf_stat.st_size, 0);
	chown_label (3, buffer);
	chown_label (4, string_perm (sf_stat.st_mode));

	run_dlg (ch_dlg);
    
	switch (ch_dlg->ret_value) {
	case B_CANCEL:
	    end_chown = 1;
	    break;
	    
	case B_SETUSR:
	{
	    struct passwd *user;

	    user = getpwnam (l_user->current->text);
	    if (user){
		new_user = user->pw_uid;
		apply_chowns (new_user, new_group);
	    }
	    break;
	}   
	case B_SETGRP:
	{
	    struct group *grp;

	    grp = getgrnam (l_group->current->text);
	    if (grp){
		new_group = grp->gr_gid;
		apply_chowns (new_user, new_group);
	    }
	    break;
	}
	case B_SETALL:
	case B_ENTER:
	{
	    struct group *grp;
	    struct passwd *user;
	    
	    grp = getgrnam (l_group->current->text);
	    if (grp)
		new_group = grp->gr_gid;
	    user = getpwnam (l_user->current->text);
	    if (user)
		new_user = user->pw_uid;
	    if (ch_dlg->ret_value==B_ENTER) {
		need_update = 1;
		if (mc_chown (fname, new_user, new_group) == -1)
		    message (1, MSG_ERROR, _(" Cannot chown \"%s\" \n %s "),
	 		 fname, unix_error_string (errno));
	    } else
		apply_chowns (new_user, new_group);
	    break;
	}
	}
	
	if (cpanel->marked && ch_dlg->ret_value != B_CANCEL){
	    do_file_mark (cpanel, current_file, 0);
	    need_update = 1;
	}
	destroy_dlg (ch_dlg);
    } while (cpanel->marked && !end_chown);
    
    chown_done ();
}

/* Chmod command for OS/2 operating system

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

#ifndef __os2__
#error This file is for the OS/2 operating system.
#else

#include <config.h>
#define INCL_DOSFILEMGR
#include <os2.h>
#include <string.h>
#include <stdio.h>
/* for chmod and stat */
#include <io.h>
#include <sys\stat.h>
#include <sys\types.h>
#include "tty.h"
#include "mad.h"
#include "util.h"
#include "win.h"
#include "color.h"
#include "dlg.h"
#include "widget.h"
#include "dialog.h"	/* For do_refresh() */

#include "dir.h"
#include "panel.h"		/* Needed for the externs */
#include "file.h"
#include "main.h"
#include "chmod.h"
#include "achown.h"
#include "chown.h"

static int single_set;
struct Dlg_head *ch_dlg;

#define PX		5
#define PY		2

#define FX		40
#define FY		2

#define BX		6
#define BY		17

#define TX		40
#define TY		12

#define PERMISSIONS	4
#define BUTTONS		6

#define B_MARKED	B_USER
#define B_ALL		B_USER+1
#define B_SETMRK        B_USER+2
#define B_CLRMRK        B_USER+3

int     mode_change, need_update;
int     c_file,      end_chmod;

umode_t and_mask,    or_mask,   c_stat;
char    *c_fname,    *c_fown,   *c_fgrp, *c_fperm;
int     c_fsize;

static WLabel        *statl;
static int           normal_color;
static int           title_color;
static int           selection_color;

/* bsedos.h */
struct {
    mode_t      mode;
    char        *text;
    int         selected;
    WCheck      *check;
} check_perm[PERMISSIONS] = {

    {	
	FILE_ARCHIVED, "Archive", 0, 0,
    },
    {
	FILE_READONLY, "Read Only", 0, 0,
    },
    {
	FILE_HIDDEN, "Hidden", 0, 0,
    },
    {
	FILE_SYSTEM, "System", 0, 0,
    },
};

struct {
    int         ret_cmd, y, x;
    char        *text;
    int         hkey, hpos;
} chmod_but[BUTTONS] = {

    {
	B_CANCEL, 2, 33, "[ Cancel ]", 'c', 2,
    },
    {
	B_ENTER, 2, 17, "[ Set ]", 's', 2,
    },
    {
	B_CLRMRK, 0, 42, "[ Clear marked ]", 'l', 3,
    },
    {
	B_SETMRK, 0, 27, "[ Set marked ]", 'e', 3,
    },
    {
	B_MARKED, 0, 12, "[ Marked all ]", 'm', 2,
    },
    {
	B_ALL, 0, 0, "[ Set all ]", 'a', 6,
    },
};


static void mk_chmod (char *filename, ULONG st);


static void chmod_toggle_select (void)
{
    int Id = ch_dlg->current->dlg_id - BUTTONS + single_set * 2;

    attrset (normal_color);
    check_perm[Id].selected ^= 1;

    dlg_move (ch_dlg, PY + PERMISSIONS - Id, PX + 1);
    addch ((check_perm[Id].selected) ? '*' : ' ');
    dlg_move (ch_dlg, PY + PERMISSIONS - Id, PX + 3);
}

static void chmod_refresh (void)
{
    attrset (normal_color);
    dlg_erase (ch_dlg);

    draw_box (ch_dlg, 1, 2, 20 - single_set, 66);
    draw_box (ch_dlg, PY, PX, PERMISSIONS + 2, 33);
    draw_box (ch_dlg, FY, FX, 10, 25);

    dlg_move (ch_dlg, FY + 1, FX + 2);
    addstr ("Name");
    dlg_move (ch_dlg, FY + 3, FX + 2);
    addstr ("Permissions (Octal)");
    dlg_move (ch_dlg, FY + 5, FX + 2);
    addstr ("Owner name");
    dlg_move (ch_dlg, FY + 7, FX + 2);
    addstr ("Group name");

    attrset (title_color);
    dlg_move (ch_dlg, 1, 28);
    addstr (" Chmod command ");
    dlg_move (ch_dlg, PY, PX + 1);
    addstr (" Permission ");
    dlg_move (ch_dlg, FY, FX + 1);
    addstr (" File ");

    attrset (selection_color);

    dlg_move (ch_dlg, TY, TX);
    addstr ("Use SPACE to change");
    dlg_move (ch_dlg, TY + 1, TX);
    addstr ("an option, ARROW KEYS");
    dlg_move (ch_dlg, TY + 2, TX);
    addstr ("to move between options");
    dlg_move (ch_dlg, TY + 3, TX);
    addstr ("and T or INS to mark");
}

static int chmod_callback (Dlg_head *h, int Par, int Msg)
{
    char buffer [10];

    switch (Msg) {
    case DLG_ACTION:
	if (Par >= BUTTONS - single_set * 2){
	    c_stat ^= check_perm[Par - BUTTONS + single_set * 2].mode;
	    sprintf (buffer, "%o", c_stat);
	    label_set_text (statl, buffer);
	    chmod_toggle_select ();
	    mode_change = 1;
	}
	break;

    case DLG_KEY:
	if ((Par == 'T' || Par == 't' || Par == KEY_IC) &&
	    ch_dlg->current->dlg_id >= BUTTONS - single_set * 2) {
	    chmod_toggle_select ();
	    if (Par == KEY_IC)
		dlg_one_down (ch_dlg);
	    return 1;
	}
	break;
#ifndef HAVE_X
    case DLG_DRAW:
	chmod_refresh ();
	break;
#endif
    }
    return 0;
}

static void init_chmod (void)
{
    int i;

    do_refresh ();
    end_chmod = c_file = need_update = 0;
    single_set = (cpanel->marked < 2) ? 2 : 0;

    if (use_colors){
	normal_color = COLOR_NORMAL;
	title_color  = COLOR_HOT_NORMAL;
	selection_color = COLOR_NORMAL;
    } else {
	normal_color = NORMAL_COLOR;
	title_color  = SELECTED_COLOR;
	selection_color = SELECTED_COLOR;
    }

    ch_dlg = create_dlg (0, 0, 22 - single_set, 70, dialog_colors,
			 chmod_callback, "[Chmod]", "chmod", DLG_CENTER);
			
    x_set_dialog_title (ch_dlg, "Chmod command");

#define XTRACT(i) BY+chmod_but[i].y-single_set, BX+chmod_but[i].x, \
                  chmod_but[i].ret_cmd, chmod_but[i].text, chmod_but[i].hkey,                    chmod_but[i].hpos, 0, 0, NULL

    tk_new_frame (ch_dlg, "b.");
    for (i = 0; i < BUTTONS; i++) {
	if (i == 2 && single_set)
	    break;
	else
	    add_widgetl (ch_dlg, button_new (XTRACT (i)), XV_WLAY_RIGHTOF);
    }


#define XTRACT2(i) 0, check_perm [i].text, 0, -1, NULL
    tk_new_frame (ch_dlg, "c.");
    for (i = 0; i < PERMISSIONS; i++) {
	check_perm[i].check = check_new (PY + (PERMISSIONS - i), PX + 2,
					 XTRACT2 (i));
	add_widget (ch_dlg, check_perm[i].check);
    }
}

static int stat_file (char *filename, struct stat *st)
{
    HFILE       fHandle    = 0L;
    ULONG       fInfoLevel = 1;  /* 1st Level Info: Standard attributs */
    FILESTATUS3 fInfoBuf;
    ULONG       fInfoBufSize;
    ULONG      fAction    = 0;
    APIRET      rc;

    fInfoBufSize = sizeof(FILESTATUS3);
    rc = DosOpen((PSZ) filename,
                 &fHandle,
                 &fAction,
                 (ULONG) 0,
                 FILE_NORMAL,
                 OPEN_ACTION_OPEN_IF_EXISTS,
                 (OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE),
                 (PEAOP2) NULL);
    if (rc != 0) {
       return 0;
    } /* endif */

    rc = DosQueryFileInfo(fHandle, fInfoLevel, &fInfoBuf, fInfoBufSize);
    DosClose(fHandle);
    if (rc != 0) {
       return 0;  /* error ! */
    } else {
       st->st_mode = fInfoBuf.attrFile;
    } /* endif */

    if (st->st_mode & FILE_DIRECTORY)
    	return 0;
    return 1;
}

static void chmod_done (void)
{
    if (need_update)
	update_panels (UP_OPTIMIZE, UP_KEEPSEL, UP_KEEPSEL);
    repaint_screen ();
}

char *next_file (void)
{
    while (!cpanel->dir.list[c_file].f.marked)
	c_file++;

    return cpanel->dir.list[c_file].fname;
}

static int os2_chmod (char *filename, ULONG st)
{
    HFILE       fHandle    = 0L;
    ULONG       fInfoLevel = 1;  /* 1st Level Info: Standard attributs */
    FILESTATUS3 fInfoBuf;
    ULONG       fInfoBufSize;
    ULONG       fAction    = 0L;
    APIRET      rc;

    fInfoBufSize = sizeof(FILESTATUS3);
    rc = DosOpen((PSZ) filename,
                 &fHandle,
                 &fAction,
                 (ULONG) 0,
                 FILE_NORMAL,
                 OPEN_ACTION_OPEN_IF_EXISTS,
                 (OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYNONE),
                 0L);
    if (rc != 0) {
       return rc;
    } /* endif */

    rc = DosQueryFileInfo(fHandle, fInfoLevel, &fInfoBuf, fInfoBufSize);
    if (rc!=0) {
       DosClose(fHandle);
       return rc;
    } /* endif */
    fInfoBuf.attrFile = st;
    rc = DosSetFileInfo(fHandle, fInfoLevel, &fInfoBuf, fInfoBufSize);
    rc = DosClose(fHandle);
    return rc;
}

static void do_chmod (struct stat *sf)
{
    HFILE       fHandle    = 0L;
    ULONG       fInfoLevel = 1;  /* 1st Level Info: Standard attributs */
    FILESTATUS3 fInfoBuf;
    ULONG       fInfoBufSize;
    ULONG       fAction = 0;
    EAOP2       fEaop;
    APIRET      rc;

    sf->st_mode &= and_mask;
    sf->st_mode |= or_mask;

    mk_chmod(cpanel->dir.list[c_file].fname, sf->st_mode);

    do_file_mark (cpanel, c_file, 0);
    return;
}

static void mk_chmod (char *filename, ULONG st)
{
    int rc;
    int fileMode = 0;

    if (st & FILE_READONLY) {
       // READONLY set
       fileMode |= S_IREAD;
       os2_chmod(filename, st);
    } else {
       fileMode |= (S_IWRITE | S_IREAD);
       rc = chmod(filename, fileMode);
       // Then set the other flags
       os2_chmod(filename, st);
    } /* endif */
    return;
}


static void apply_mask (struct stat *sf)
{
    char *fname;

    need_update = end_chmod = 1;
    do_chmod (sf);

    do {
	fname = next_file ();
	if (!stat_file (fname, sf))
	    return;
	c_stat = sf->st_mode;

	do_chmod (sf);
    } while (cpanel->marked);
}

void chmod_cmd (void)
{
    char buffer [10];
    char *fname;
    int i;
    struct stat sf_stat;

    do {			/* do while any files remaining */
	init_chmod ();
	if (cpanel->marked)
	    fname = next_file ();	/* next marked file */
	else
	    fname = selection (cpanel)->fname;	/* single file */

	if (!stat_file (fname, &sf_stat))	/* get status of file */
	    break;
	
	c_stat = sf_stat.st_mode;
	mode_change = 0;	/* clear changes flag */

	/* set check buttons */
	for (i = 0; i < PERMISSIONS; i++){
	    check_perm[i].check->state = (c_stat & check_perm[i].mode) ? 1 : 0;
	    check_perm[i].selected = 0;
	}

	tk_new_frame (ch_dlg, "l.");
	/* Set the labels */
	c_fname = name_trunc (fname, 21);
	add_widget (ch_dlg, label_new (FY+2, FX+2, c_fname, NULL));
	c_fown = name_trunc (get_owner (sf_stat.st_uid), 21);
	add_widget (ch_dlg, label_new (FY+6, FX+2, c_fown, NULL));
	c_fgrp = name_trunc (get_group (sf_stat.st_gid), 21);
	add_widget (ch_dlg, label_new (FY+8, FX+2, c_fgrp, NULL));
	sprintf (buffer, "%o", c_stat);
	statl = label_new (FY+4, FX+2, buffer, NULL);
	add_widget (ch_dlg, statl);
	tk_end_frame ();
	
	run_dlg (ch_dlg);	/* retrieve an action */
	
	/* do action */
	switch (ch_dlg->ret_value){
	case B_ENTER:
	    if (mode_change)
		mk_chmod (fname, c_stat);  /*.ado */
	    need_update = 1;
	    break;
	
	case B_CANCEL:
	    end_chmod = 1;
	    break;
	
	case B_ALL:
	case B_MARKED:
	    and_mask = or_mask = 0;
	    and_mask = ~and_mask;

	    for (i = 0; i < PERMISSIONS; i++) {
		if (check_perm[i].selected || ch_dlg->ret_value == B_ALL)
		    if (check_perm[i].check->state & C_BOOL)
			or_mask |= check_perm[i].mode;
		    else
			and_mask &= ~check_perm[i].mode;
	    }

	    apply_mask (&sf_stat);
	    break;
	
	case B_SETMRK:
	    and_mask = or_mask = 0;
	    and_mask = ~and_mask;

	    for (i = 0; i < PERMISSIONS; i++) {
		if (check_perm[i].selected)
		    or_mask |= check_perm[i].mode;
	    }

	    apply_mask (&sf_stat);
	    break;
	case B_CLRMRK:
	    and_mask = or_mask = 0;
	    and_mask = ~and_mask;

	    for (i = 0; i < PERMISSIONS; i++) {
		if (check_perm[i].selected)
		    and_mask &= ~check_perm[i].mode;
	    }

	    apply_mask (&sf_stat);
	    break;
	}

	if (cpanel->marked && ch_dlg->ret_value!=B_CANCEL) {
	    do_file_mark (cpanel, c_file, 0);
	    need_update = 1;
	}
	destroy_dlg (ch_dlg);
    } while (cpanel->marked && !end_chmod);
    chmod_done ();
}

#endif /*__os2__*/

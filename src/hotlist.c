/* Directory hotlist -- for the Midnight Commander
   Copyright (C) 1994, 1995, 1996, 1997 the Free Software Foundation.

   Written by:
    1994 Radek Doulik
    1995 Janne Kukonlehto
    1996 Andrej Borsenkow
    1997 Norbert Warmuth

   Janne did the original Hotlist code, Andrej made the groupable
   hotlist; the move hotlist and revamped the file format and made
   it stronger.
   
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
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>		/* For malloc() */
#include <ctype.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#ifdef SCO_FLAVOR
#	include <sys/timeb.h>	/* alex: for struct timeb, used in time.h */
#endif /* SCO_FLAVOR */
#include <time.h>
#ifndef OS2_NT
#    include <grp.h>
#    include <pwd.h>
#else
#    include <io.h>
#    include <direct.h>
#endif
#include "tty.h"
#include "mad.h"
#include "util.h"		/* Needed for the externs */
#include "win.h"
#include "color.h"
#include "dlg.h"
#include "widget.h"
#include "dialog.h"		/* For do_refresh() */
#include "setup.h"		/* For profile_bname */
#include "profile.h"		/* Load/save directories hotlist */

#include "../vfs/vfs.h"
/* Needed for the extern declarations of integer parameters */
#include "wtools.h"
#include "dir.h"
#include "panel.h"		/* Needed for the externs */
#include "file.h"
#include "main.h"
#include "global.h"
#include "hotlist.h"
#include "key.h"
#include "command.h"

#ifdef HAVE_TK
#    include "tkwidget.h"
#endif

#define UX		5
#define UY		2

#define BX		UX
#define BY		LINES-6

#define BUTTONS		(sizeof(hotlist_but)/sizeof(struct _hotlist_but))
#define LABELS          3
#define B_ADD_CURRENT	B_USER
#define B_REMOVE        (B_USER + 1)
#define B_NEW_GROUP	(B_USER + 2)
#define B_NEW_ENTRY	(B_USER + 3)
#define B_UP_GROUP	(B_USER + 4)
#define B_INSERT	(B_USER + 5)
#define B_APPEND	(B_USER + 6)
#define B_MOVE		(B_USER + 7)

static WListbox *l_hotlist;
static WListbox *l_movelist;

static Dlg_head *hotlist_dlg;
static Dlg_head *movelist_dlg;

static WLabel *pname, *pname_group, *movelist_group;

enum HotListType {
    HL_TYPE_GROUP,
    HL_TYPE_ENTRY,
    HL_TYPE_COMMENT
};

static struct {
    /*
     * these parameters are intended to be user configurable
     */
    int		expanded;	/* expanded view of all groups at startup */

    /*
     * these reflect run time state
     */

    int		loaded;		/* hotlist is loaded */
    int		readonly;	/* hotlist readonly */
    int		file_error;	/* parse error while reading file */
    int		running;	/* we are running dlg (and have to
    				   update listbox */
    int		moving;		/* we are in moving hotlist currently */
    int		modified;	/* hotlist was modified */
    int		type;		/* LIST_HOTLIST || LIST_VFSLIST */
}   hotlist_state;

struct _hotlist_but {
    int ret_cmd, flags, y, x;
    char *text;
    char *tkname;
    int   type;
} hotlist_but[] = {
    { B_MOVE, NORMAL_BUTTON,         1,   42, "&Move",       "move", LIST_HOTLIST},
    { B_REMOVE, NORMAL_BUTTON,       1,   30, "&Remove",     "r",    LIST_HOTLIST},
    { B_APPEND, NORMAL_BUTTON,       1,   15, "&Append",     "e",    LIST_MOVELIST},
    { B_INSERT, NORMAL_BUTTON,       1,    0, "&Insert",     "g",    LIST_MOVELIST},
    { B_NEW_ENTRY, NORMAL_BUTTON,    1,   15, "New &Entry",  "e",    LIST_HOTLIST},
    { B_NEW_GROUP, NORMAL_BUTTON,    1,    0, "New &Group",  "g",    LIST_HOTLIST},
    { B_CANCEL, NORMAL_BUTTON,       0,   53, "&Cancel",     "cc",   LIST_HOTLIST|LIST_VFSLIST|LIST_MOVELIST},
    { B_UP_GROUP, NORMAL_BUTTON,     0,   42, "&Up",         "up",   LIST_HOTLIST|LIST_MOVELIST},
    { B_ADD_CURRENT, NORMAL_BUTTON,  0,   20, "&Add current","ad",   LIST_HOTLIST},
    { B_ENTER, DEFPUSH_BUTTON,       0,    0, "Change &To",  "ct",   LIST_HOTLIST|LIST_VFSLIST|LIST_MOVELIST},
};

/* Directory hotlist */
static struct hotlist{
    enum  HotListType type;
    char *directory;
    char *label;
    struct hotlist *head;
    struct hotlist *up;
    struct hotlist *next;
} *hotlist = NULL;

struct hotlist *current_group;

static void remove_from_hotlist (struct hotlist *entry);
void add_new_group_cmd (void);

static struct hotlist *new_hotlist (void)
{
    struct hotlist *hl;

    hl = xmalloc (sizeof (struct hotlist), "new-hotlist");
    hl->type     = 0;
    hl->directory =
	hl->label = 0;
    hl->head =
	hl->up =
	    hl->next = 0;
    
    return hl;
}

#ifndef HAVE_X
static void hotlist_refresh (Dlg_head *dlg)
{
    dialog_repaint (dlg, COLOR_NORMAL, COLOR_HOT_NORMAL);
    attrset (COLOR_NORMAL);
    draw_box (dlg, UY, UX, UY+(LINES-14), COLS-2*UX-6);
    if (!hotlist_state.moving)
	draw_box (dlg, UY+(LINES-12), UX, 3, COLS-2*UX-6);
}
#endif

/* If current->data is 0, then we are dealing with a VFS pathname */
static INLINE void update_path_name ()
{
    char *text, *p;
    WListbox  *list = hotlist_state.moving ? l_movelist : l_hotlist;
    Dlg_head  *dlg = hotlist_state.moving ? movelist_dlg : hotlist_dlg;

    if (list->current){
	if (list->current->data != 0) {
	    struct hotlist *hlp = (struct hotlist *)list->current->data;
	    
	    if (hlp->type == HL_TYPE_ENTRY)
		text = hlp->directory;
	    else
		text = "Subgroup - press ENTER to see list";
	    
#ifndef HAVE_X
	    p = copy_strings (" ", current_group->label, " ", (char *)0);
	    if (!hotlist_state.moving)
		label_set_text (pname_group, name_trunc (p, COLS-2*UX-8));
	    else
		label_set_text (movelist_group, name_trunc (p, COLS-2*UX-8));
	    free (p);
#endif
	} else {
	    text = list->current->text;
	}
    } else {
	text = "";
    }
    if (!hotlist_state.moving)
	label_set_text (pname, name_trunc (text, COLS-2*UX-10));
    dlg_redraw (dlg);
}

#define CHECK_BUFFER  \
do { \
    int               i; \
\
    if ((i = strlen (current->label) + 3) > buflen) { \
      free (buf); \
      buf = xmalloc (buflen = 1024 * (i/1024 + 1), "fill_listbox"); \
    } \
    buf[0] = '\0'; \
} while (0)

static void fill_listbox (void)
{
    struct hotlist *current = current_group->head;
    static char *buf;
    static int   buflen;

    if (!buf)
	buf = xmalloc (buflen = 1024, "fill_listbox");
    buf[0] = '\0';

    while (current){
	switch (current->type) {
	case HL_TYPE_GROUP:
	    {
		CHECK_BUFFER;
		strcat (strcat (buf, "->"), current->label);
		if (hotlist_state.moving)
		    listbox_add_item (l_movelist, 0, 0, buf, current);
		else
		    listbox_add_item (l_hotlist, 0, 0, buf, current);
	    }
	    break;
	case HL_TYPE_ENTRY:
		if (hotlist_state.moving)
		    listbox_add_item (l_movelist, 0, 0, current->label, current);
		else
		    listbox_add_item (l_hotlist, 0, 0, current->label, current);
	    break;
	}
	current = current->next;
    }
}

#undef CHECK_BUFFER

static void
unlink_entry (struct hotlist *entry)
{
    struct hotlist *current = current_group->head;

    if (current == entry)
	current_group->head = entry->next;
    else
	while (current && current->next != entry)
	    current = current->next;
	if (current)
	    current->next = entry->next;
    entry->next =
	entry->up = 0;
}
    
static void add_new_entry_cmd (void);
static void init_movelist (int, struct hotlist *);

static int hotlist_button_callback (int action, void *data)
{
    switch (action) {
    case B_MOVE:
	{
	    struct hotlist  *saved = current_group;
	    struct hotlist  *item;
	    struct hotlist  *moveto_item = 0;
	    struct hotlist  *moveto_group = 0;
	    int             ret;

	    if (!l_hotlist->current)
		return 0;		/* empty group - nothing to do */
	    item = l_hotlist->current->data;
	    hotlist_state.moving = 1;
	    init_movelist (LIST_MOVELIST, item);
	    run_dlg (movelist_dlg);
	    ret = movelist_dlg->ret_value;
	    hotlist_state.moving = 0;
	    if (l_movelist->current)
		moveto_item = l_movelist->current->data;
	    moveto_group = current_group;
	    destroy_dlg (movelist_dlg);
	    current_group = saved;
	    if (ret == B_CANCEL)
		return 0;
	    if (moveto_item == item)
		return 0;		/* If we insert/append a before/after a
					   it hardly changes anything ;) */
	    unlink_entry (item);
	    listbox_remove_current (l_hotlist, 1);
	    item->up = moveto_group;
	    if (!moveto_group->head)
		moveto_group->head = item;
	    else if (!moveto_item) {	/* we have group with just comments */
		struct hotlist *p = moveto_group->head;

		/* skip comments */
		while (p->next)
		    p = p->next;
		p->next = item;
	    } else if (ret == B_ENTER || ret == B_APPEND)
		if (!moveto_item->next)
		    moveto_item->next = item;
		else {
		    item->next = moveto_item->next;
		    moveto_item->next = item;
		}
	    else if (moveto_group->head == moveto_item) {
		    moveto_group->head = item;
		    item->next = moveto_item;
		} else {
		    struct hotlist *p = moveto_group->head;

		    while (p->next != moveto_item)
			p = p->next;
		    item->next = p->next;
		    p->next = item;
		}
	    listbox_remove_list (l_hotlist);
	    fill_listbox ();
#ifdef HAVE_X
	    x_listbox_select_nth (l_hotlist, 0);
#endif
	    repaint_screen ();
	    hotlist_state.modified = 1;
	    return 0;
	    break;
	}
    case B_REMOVE:
	if (l_hotlist->current)
	    remove_from_hotlist (l_hotlist->current->data);
	return 0;
	break;

    case B_NEW_GROUP:
	add_new_group_cmd ();
	return 0;
	break;

    case B_ADD_CURRENT:
	add2hotlist_cmd ();
	return 0;
	break;

    case B_NEW_ENTRY:
	add_new_entry_cmd ();
	return 0;
	break;

    case B_ENTER:
	{
	WListbox *list = hotlist_state.moving ? l_movelist : l_hotlist;
	if (list->current){
	    if (list->current->data) {
		struct hotlist *hlp = (struct hotlist*) list->current->data;
		if (hlp->type == HL_TYPE_ENTRY)
		    return 1;
		else {
		    listbox_remove_list (list);
		    current_group = hlp;
		    fill_listbox ();
		    return 0;
		}
	    } else
		return 1;
	}
	}
	/* Fall through if list empty - just go up */

    case B_UP_GROUP:
	{
	WListbox *list = hotlist_state.moving ? l_movelist : l_hotlist;
	listbox_remove_list (list);
	current_group = current_group->up;
	fill_listbox ();
#ifdef HAVE_X
	x_listbox_select_nth (list, 0);
#endif
	return 0;
	break;
	}

    default:
	return 1;
	break;

    }
}

static int hotlist_callback (Dlg_head * h, int Par, int Msg)
{
    switch (Msg) {
#ifndef HAVE_X    
    case DLG_DRAW:
	hotlist_refresh (h);
	break;
#endif	

    case DLG_UNHANDLED_KEY:
	switch (Par) {
	case '\n':
	    if (ctrl_pressed())
		goto l1;
	case KEY_ENTER:
	case KEY_RIGHT:
	    if (hotlist_button_callback (B_ENTER, 0)) {
		h->ret_value = B_ENTER;
		dlg_stop (h);
	    };
	    return 1;
	    break;
	case KEY_LEFT:
	    if (hotlist_state.type != LIST_VFSLIST )
    		return !hotlist_button_callback (B_UP_GROUP, 0);
	    else
		return 0;
	    break;
l1:
	case ALT('\n'):
	case ALT('\r'):
	    if (!hotlist_state.moving)
	    {
    		if (l_hotlist->current){
		    if (l_hotlist->current->data) {
			struct hotlist *hlp = (struct hotlist*) l_hotlist->current->data;
			if (hlp->type == HL_TYPE_ENTRY) {
			    char *tmp = copy_strings( "cd ", hlp->directory, NULL);
			    stuff (input_w (cmdline), tmp, 0);
			    free (tmp);
			    dlg_stop (h);
			    h->ret_value = B_CANCEL;
			    return 1;
		        }
		    }
		}
	    }
	    return 1; /* ignore key */
	default:
	    return 0;
	}
	break;

    case DLG_POST_KEY:
	if (hotlist_state.moving)
	    dlg_select_widget (movelist_dlg, l_movelist);
	else
	    dlg_select_widget (hotlist_dlg, l_hotlist);
	/* always stay on hotlist */
	/* fall through */

    case DLG_INIT:
	attrset (MENU_ENTRY_COLOR);
	update_path_name ();
	break;
    }
    return 0;
}

static int l_call (void *l)
{
    WListbox *list = (WListbox *) l;
    Dlg_head *dlg = hotlist_state.moving ? movelist_dlg : hotlist_dlg;

    if (list->current){
	if (list->current->data) {
	    struct hotlist *hlp = (struct hotlist*) list->current->data;
	    if (hlp->type == HL_TYPE_ENTRY) {
		dlg->ret_value = B_ENTER;
		dlg_stop (dlg);
		return listbox_finish;
	    } else {
		hotlist_button_callback (B_ENTER, (void *)0);
		hotlist_callback (dlg, '\n', DLG_POST_KEY);
		return listbox_nothing;
	    }
	} else {
	    dlg->ret_value = B_ENTER;
	    dlg_stop (dlg);
	    return listbox_finish;
	}
    }

    hotlist_button_callback (B_UP_GROUP, (void *)0);
    hotlist_callback (dlg, 'u', DLG_POST_KEY);
    return listbox_nothing;
}

static void add_name_to_list (char *path)
{
    listbox_add_item (l_hotlist, 0, 0, path, 0);
}

static void init_hotlist (int list_type)
{
    int i;

    do_refresh ();

    hotlist_state.expanded = GetPrivateProfileInt ("HotlistConfig",
			    "expanded_view_of_groups", 0, profile_name);

    hotlist_dlg = create_dlg (0, 0, LINES-2, COLS-6, dialog_colors,
			      hotlist_callback, 
			      list_type == LIST_VFSLIST ? "[vfshot]" : "[Hotlist]",
			      list_type == LIST_VFSLIST ? "vfshot" : "hotlist",
			      DLG_CENTER|DLG_GRID);
    x_set_dialog_title (hotlist_dlg,
        list_type == LIST_VFSLIST ? "Active VFS directories" : "Directory hotlist");

#define XTRACT(i) BY+hotlist_but[i].y, BX+hotlist_but[i].x, hotlist_but[i].ret_cmd, hotlist_but[i].flags, hotlist_but[i].text, hotlist_button_callback, 0, hotlist_but[i].tkname

    for (i = 0; i < BUTTONS; i++){
	if (hotlist_but[i].type & list_type)
	    add_widgetl (hotlist_dlg, button_new (XTRACT (i)), (i == BUTTONS - 1) ?
		XV_WLAY_CENTERROW : XV_WLAY_RIGHTOF);
    }
#undef XTRACT

    /* We add the labels. 
     *    pname       will hold entry's pathname;
     *    pname_group will hold name of current group
     */
    pname = label_new (UY-11+LINES, UX+2, "", "the-lab");
    add_widget (hotlist_dlg, pname);
#ifndef HAVE_X
    if (!hotlist_state.moving) {
	add_widget (hotlist_dlg, label_new (UY-12+LINES, UX+1, " Directory path ", NULL));

	/* This one holds the displayed pathname */
	pname_group = label_new (UY, UX+1, " Directory label ", NULL);
	add_widget (hotlist_dlg, pname_group);
    }
#endif
    /* get new listbox */
    l_hotlist = listbox_new (UY + 1, UX + 1, COLS-2*UX-8, LINES-14, listbox_cback, l_call, "listbox");

    /* Fill the hotlist with the active VFS or the hotlist */
    if (list_type == LIST_VFSLIST){
	listbox_add_item (l_hotlist, 0, 0, home_dir, 0);
	vfs_fill_names (add_name_to_list);
    } else
	fill_listbox ();

    add_widgetl (hotlist_dlg, l_hotlist, XV_WLAY_EXTENDWIDTH); 
    /* add listbox to the dialogs */
}

static void init_movelist (int list_type, struct hotlist *item)
{
    int i;
    char *hdr = copy_strings ("Moving ", item->label, 0);

    do_refresh ();

    movelist_dlg = create_dlg (0, 0, LINES-6, COLS-6, dialog_colors,
			      hotlist_callback, "[Hotlist]",
			      "movelist",
			      DLG_CENTER|DLG_GRID);
    x_set_dialog_title (movelist_dlg, hdr);
    free (hdr);

#define XTRACT(i) BY-4+hotlist_but[i].y, BX+hotlist_but[i].x, hotlist_but[i].ret_cmd, hotlist_but[i].flags, hotlist_but[i].text, hotlist_button_callback, 0, hotlist_but[i].tkname

    for (i = 0; i < BUTTONS; i++){
	if (hotlist_but[i].type & list_type)
	    add_widgetl (movelist_dlg, button_new (XTRACT (i)), (i == BUTTONS - 1) ?
		XV_WLAY_CENTERROW : XV_WLAY_RIGHTOF);
    }

#undef	XTRACT

    /* We add the labels.  We are interested in the last one,
     * that one will hold the path name label
     */
#ifndef HAVE_X
    movelist_group = label_new (UY, UX+1, " Directory label ", NULL);
    add_widget (movelist_dlg, movelist_group);
#endif
    /* get new listbox */
    l_movelist = listbox_new (UY + 1, UX + 1, COLS-2*UX-8, LINES-14, listbox_cback, l_call, "listbox");

    fill_listbox ();

    add_widgetl (movelist_dlg, l_movelist, XV_WLAY_EXTENDWIDTH); 
    /* add listbox to the dialogs */
}

static void hotlist_done (void)
{
    destroy_dlg (hotlist_dlg);
    if (0)
	update_panels (UP_OPTIMIZE, UP_KEEPSEL, UP_KEEPSEL);
    repaint_screen ();
}

static char *
find_group_section (struct hotlist *grp)
{
    return copy_strings (grp->directory, ".Group", (char *)0);

}


/* 1.11.96 bor: added pos parameter to control placement of new item.
   see widget.c, listbox_add_item()
                now hotlist is in unsorted mode
 */
static struct hotlist *
add2hotlist (char *label, char *directory, enum HotListType type, int pos)
{
    struct hotlist *current;
    struct hotlist *new;

    if (l_hotlist && l_hotlist->current)
	current = l_hotlist->current->data;

    new = new_hotlist ();

    new->type      = type;
    new->label     = label;
    new->directory = directory;
    new->up	   = current_group;

    if (!current_group->head) { /* first element in group */
	current_group->head = new;
    } else if (pos == 2) { /* should be appended after current*/
	new->next     = current->next;
	current->next = new;
    } else if (pos == 1 &&
	       current == current_group->head) {
			   /* should be inserted before first item */
	new->next = current;
	current_group->head = new;
    } else if (pos == 1) { /* befor current */
	struct hotlist  *p = current_group->head;

	while (p->next != current)
	    p = p->next;

	new->next = current;
	p->next   = new;
    } else {		    /* append at the end */
	struct hotlist  *p = current_group->head;

	while (p->next)
	    p = p->next;

	p->next = new;
    }

    if (hotlist_state.running && type != HL_TYPE_COMMENT) {
	if (type == HL_TYPE_GROUP) {
	    char  *lbl = copy_strings ("->", new->label, (char *)0);

	    listbox_add_item (l_hotlist, pos, 0, lbl, new);
	    free (lbl);
	} else
	    listbox_add_item (l_hotlist, pos, 0, new->label, new);
	listbox_select_entry (l_hotlist, l_hotlist->current);
    }
    return new;

}

static int add_new_entry_input (char *header, char *text1, char *text2, char *help, char **r1, char **r2)
{
    QuickDialog Quick_input;
    QuickWidget quick_widgets [] = {
    { quick_button, 55, 80, 4, 0, "&Cancel", 0, B_CANCEL, 0, 0,
	  XV_WLAY_DONTCARE, "button-cancel" },
    { quick_button, 30, 80, 4, 0, "&Insert", 0, B_INSERT, 0, 0,
	  XV_WLAY_DONTCARE, "button-insert" },
    { quick_button, 10, 80, 4, 0, "&Append", 0, B_APPEND, 0, 0,
	  XV_WLAY_DONTCARE, "button-append" },
    { quick_input,  4, 80, 4, 0, "",58, 0, 0, 0, XV_WLAY_BELOWCLOSE, "input-pth" },
    { quick_label,  3, 80, 3, 0, 0, 0, 0, 0, 0, XV_WLAY_DONTCARE, "label-pth" },
    { quick_input,  4, 80, 3, 0, "", 58, 0, 0, 0, XV_WLAY_BELOWCLOSE, "input-lbl" },
    { quick_label,  3, 80, 2, 0, 0, 0, 0, 0, 0, XV_WLAY_DONTCARE, "label-lbl" },
    { 0 } };
    
    int len;
    int i;
    int lines1, lines2;
    char *my_str1, *my_str2;
    
    len = max (strlen (header), msglen (text1, &lines1));
    len = max (len, msglen (text2, &lines2)) + 4;
    len = max (len, 64);

    Quick_input.xlen  = len;
    Quick_input.xpos  = -1;
    Quick_input.title = header;
    Quick_input.help  = help;
    Quick_input.class = "hotlist_new_entry";
    quick_widgets [6].text = text1;
    quick_widgets [4].text = text2;
    quick_widgets [5].text = *r1;
    quick_widgets [3].text = *r2;

    for (i = 0; i < 7; i++)
	quick_widgets [i].y_divisions = lines1+lines2+7;
    Quick_input.ylen  = lines1 + lines2 + 7;

    quick_widgets [0].relative_y += (lines1 + lines2);
    quick_widgets [1].relative_y += (lines1 + lines2);
    quick_widgets [2].relative_y += (lines1 + lines2);
    quick_widgets [3].relative_y += (lines1);
    quick_widgets [4].relative_y += (lines1);

    quick_widgets [5].str_result = &my_str1;
    quick_widgets [3].str_result = &my_str2;
    
    Quick_input.widgets = quick_widgets;
    if ((i = quick_dialog (&Quick_input)) != B_CANCEL){
	 *r1 = *(quick_widgets [5].str_result);
	 *r2 = *(quick_widgets [3].str_result);
	 return i;
    } else
	 return 0;
}

static void add_new_entry_cmd (void)
{
    char *title = 0, *url = 0;
    int ret;

    /* Take current directory as default value for input fields */
    title = url = cpanel->cwd;

    ret = add_new_entry_input ("New hotlist entry", "Directory label", "Directory path",
	 "[Hotlist]", &title, &url);

    if (!ret || !title || !*title || !url || !*url)
	return;

    if (ret == B_ENTER || ret == B_APPEND)
	add2hotlist (strdup (title), strdup (url), HL_TYPE_ENTRY, 2);
    else
	add2hotlist (strdup (title), strdup (url), HL_TYPE_ENTRY, 1);

    hotlist_state.modified = 1;
}

static int add_new_group_input (char *header, char *label, char **result)
{
    int		ret;
    QuickDialog Quick_input;
    QuickWidget quick_widgets [] = {
    { quick_button, 55, 80, 1, 0, "&Cancel", 0, B_CANCEL, 0, 0,
	  XV_WLAY_DONTCARE, "button-cancel" },
    { quick_button, 30, 80, 1, 0, "&Insert", 0, B_INSERT, 0, 0,
	  XV_WLAY_DONTCARE, "button-insert" },
    { quick_button, 10, 80, 1, 0, "&Append", 0, B_APPEND, 0, 0,
	  XV_WLAY_DONTCARE, "button-append" },
    { quick_input,  4, 80,  0, 0, "", 58, 0, 0, 0, XV_WLAY_BELOWCLOSE, "input" },
    { quick_label,  3, 80, 2, 0, 0, 0, 0, 0, 0, 0, XV_WLAY_DONTCARE, "label" },
    { 0 } };
    
    int len;
    int i;
    int lines;
    char *my_str;
    
    len = max (strlen (header), msglen (label, &lines)) + 4;
    len = max (len, 64);

    Quick_input.xlen  = len;
    Quick_input.xpos  = -1;
    Quick_input.title = header;
    Quick_input.help  = "[Hotlist]";
    Quick_input.class = "hotlist_new_group";
    quick_widgets [4].text = label;

    for (i = 0; i < 5; i++)
	quick_widgets [i].y_divisions = lines+6;
    Quick_input.ylen  = lines + 6;

    for (i = 0; i < 4; i++)
	quick_widgets [i].relative_y += 2 + lines;

    quick_widgets [3].str_result = &my_str;
    quick_widgets [3].text       = "";
    
    Quick_input.widgets = quick_widgets;
    if ((ret = quick_dialog (&Quick_input)) != B_CANCEL){
	*result = *(quick_widgets [3].str_result);
	return ret;
    } else
	return 0;
}

void add_new_group_cmd (void)
{
    char   *label;
    int		ret;

    ret = add_new_group_input (" New hotlist group ", "Name of new group", &label);
    if (!ret || !label || !*label)
	return;

    if (ret == B_ENTER || ret == B_APPEND)
	add2hotlist (label, 0, HL_TYPE_GROUP, 2);
    else
	add2hotlist (label, 0, HL_TYPE_GROUP, 1);

    hotlist_state.modified = 1;
}

void add2hotlist_cmd (void)
{
    char *prompt, *label;

    prompt = xmalloc (strlen (cpanel->cwd) + 20, "add2hotlist_cmd");
    sprintf (prompt, "Label for \"%s\":", name_trunc (cpanel->cwd, COLS-2*UX-23));
    label = input_dialog (" Add to hotlist ", prompt, cpanel->cwd);
    free (prompt);
    if (!label || !*label)
	return;

    add2hotlist (label, strdup (cpanel->cwd), HL_TYPE_ENTRY, 0);
    hotlist_state.modified = 1;
}

static void remove_group (struct hotlist *grp)
{
    struct hotlist *current = grp->head;

    while (current) {
	struct hotlist *next = current->next;

	if (current->type == HL_TYPE_GROUP)
	    remove_group (current);

	if (current->label)
	    free (current->label);
	if (current->directory)
	    free (current->directory);
	free (current);

	current = next;
    }

}
	
static void remove_from_hotlist (struct hotlist *entry)
{
    if (entry->type == HL_TYPE_GROUP) {
	if (entry->head) {
	    char *header;
	    int   result;

	    header = copy_strings (" Remove: ",
				   name_trunc (entry->label, 30),
				   " ",
				   0);
	    result = query_dialog (header, "\n Group not empty.\n Remove it?",
				   D_ERROR, 2,
				   "&No", "&Yes");
	    free (header);

	    if (!result)
		return;
	}

	remove_group (entry);
    }

    unlink_entry (entry);

    if (entry->label)
          free (entry->label);
    if (entry->directory)
        free (entry->directory);
    free (entry);
    /* now remove list entry from screen */
    listbox_remove_current (l_hotlist, 1);
    hotlist_state.modified = 1;
}

char *hotlist_cmd (int vfs_or_hotlist)
{
    char *target = NULL;

    hotlist_state.type = vfs_or_hotlist;
    load_hotlist ();

    init_hotlist (vfs_or_hotlist);

    /* display file info */
    attrset (SELECTED_COLOR);

    hotlist_state.running = 1;
    run_dlg (hotlist_dlg);
    hotlist_state.running = 0;
    save_hotlist ();

    switch (hotlist_dlg->ret_value) {
    case B_CANCEL:
	break;

    case B_ENTER:
	if (l_hotlist->current->data) {
	    struct hotlist *hlp = (struct hotlist*) l_hotlist->current->data;
	    target = strdup (hlp->directory);
	} else
	    target = strdup (l_hotlist->current->text);
	break;
    }

    hotlist_done ();
    return target;
}

void load_group (struct hotlist *grp)
{
    void *profile_keys;
    char *key, *value;
    char *group_section;
    struct hotlist *current = 0;
    
    group_section = find_group_section (grp);

    profile_keys = profile_init_iterator (group_section, profile_name);

    current_group = grp;

    while (profile_keys){
	profile_keys = profile_iterator_next (profile_keys, &key, &value);
	add2hotlist (strdup (value), strdup (key), HL_TYPE_GROUP, 0);
    }
    free (group_section);

    profile_keys = profile_init_iterator (grp->directory, profile_name);

    while (profile_keys){
	profile_keys = profile_iterator_next (profile_keys, &key, &value);
	add2hotlist (strdup (value), strdup (key), HL_TYPE_ENTRY, 0);
    }

    for (current = grp->head; current; current = current->next)
	load_group (current);
}

#define TKN_GROUP	0
#define TKN_ENTRY	1
#define TKN_STRING	2
#define TKN_URL		3
#define TKN_ENDGROUP	4
#define TKN_COMMENT	5
#define TKN_EOL		125
#define TKN_EOF		126
#define TKN_UNKNOWN	127

static char *tkn_buf;
static int  tkn_buf_length;
static int  tkn_length;

static char *hotlist_file_name;
static FILE *hotlist_file;
static time_t hotlist_file_mtime;

static int hot_skip_blanks ()
{
    int c;

    while ((c = getc (hotlist_file)) != EOF && c != '\n' && isspace (c))
	;
    return c;
    
}

static int hot_next_token ()
{
    int	c;

#define CHECK_BUF() \
do { \
    if (tkn_length == tkn_buf_length) \
	tkn_buf = tkn_buf ? (realloc (tkn_buf, tkn_buf_length += 1024)) \
			  : (malloc (tkn_buf_length = 1024)); \
} while (0)

    tkn_length = 0;

again:
    c = hot_skip_blanks ();
    switch (c) {
    case EOF:
	return TKN_EOF;
	break;
    case '\n':
	return TKN_EOL;
	break;
    case '#':
	while ((c = getc (hotlist_file)) != EOF && c != '\n') {
	    if (c == EOF)
		return TKN_EOF;
	    if (c != '\n') {
		CHECK_BUF();
		tkn_buf[tkn_length++] = c == '\n' ? ' ' : c;
	    }
	}
	CHECK_BUF();
	tkn_buf[tkn_length] = '\0';
	return TKN_COMMENT;
	break;
    case '"':
	while ((c = getc (hotlist_file)) != EOF && c != '"') {
	    if (c == '\\')
		if ((c = getc (hotlist_file)) == EOF)
		    return TKN_EOF;
	    CHECK_BUF();
	    tkn_buf[tkn_length++] = c == '\n' ? ' ' : c;
	}
	if (c == EOF)
	    return TKN_EOF;
	CHECK_BUF();
	tkn_buf[tkn_length] = '\0';
	return TKN_STRING;
	break;
    case '\\':
	if ((c = getc (hotlist_file)) == EOF)
	    return TKN_EOF;
	if (c == '\n')
	    goto again;

	/* fall through; it is taken as normal character */

    default:
	do {
	    CHECK_BUF();
	    tkn_buf[tkn_length++] = toupper(c);
	} while ((c = fgetc (hotlist_file)) != EOF && isalnum (c));
	if (c != EOF)
	    ungetc (c, hotlist_file);
	CHECK_BUF();
	tkn_buf[tkn_length] = '\0';
	if (strncmp (tkn_buf, "GROUP", tkn_length) == 0)
	    return TKN_GROUP;
	else if (strncmp (tkn_buf, "ENTRY", tkn_length) == 0)
	    return TKN_ENTRY;
	else if (strncmp (tkn_buf, "ENDGROUP", tkn_length) == 0)
	    return TKN_ENDGROUP;
	else if (strncmp (tkn_buf, "URL", tkn_length) == 0)
	    return TKN_URL;
	else
	    return TKN_UNKNOWN;
	break;
    }
}

#define SKIP_TO_EOL	{ \
int _tkn; \
while ((_tkn = hot_next_token ()) != TKN_EOF && _tkn != TKN_EOL) ; \
}

#define CHECK_TOKEN(_TKN_) \
if ((tkn = hot_next_token ()) != _TKN_) { \
    hotlist_state.readonly = 1; \
    hotlist_state.file_error = 1; \
    while (tkn != TKN_EOL && tkn != TKN_EOF) \
	tkn = hot_next_token (); \
break; \
}

static void
hot_load_group (struct hotlist * grp)
{
    int		tkn;
    struct hotlist *new_grp;
    char	*label, *url;

    current_group = grp;

    while ((tkn = hot_next_token()) != TKN_ENDGROUP)
	switch (tkn) {
	case TKN_GROUP:
	    CHECK_TOKEN(TKN_STRING);
	    new_grp = add2hotlist (strdup (tkn_buf), 0, HL_TYPE_GROUP, 0);
	    SKIP_TO_EOL;
	    hot_load_group (new_grp);
	    current_group = grp;
	    break;
	case TKN_ENTRY:
	    CHECK_TOKEN(TKN_STRING);
	    label = strdup (tkn_buf);
	    CHECK_TOKEN(TKN_URL);
	    CHECK_TOKEN(TKN_STRING);
	    url = strdup (tkn_buf);
	    add2hotlist (label, url, HL_TYPE_ENTRY, 0);
	    SKIP_TO_EOL;
	    break;
	case TKN_COMMENT:
	    label = strdup (tkn_buf);
	    add2hotlist (label, 0, HL_TYPE_COMMENT, 0);
	    break;
	case TKN_EOF:
	    hotlist_state.readonly = 1;
	    hotlist_state.file_error = 1;
	    return;
	    break;
	case TKN_EOL:
	    /* skip empty lines */
	    break;
	default:
	    hotlist_state.readonly = 1;
	    hotlist_state.file_error = 1;
	    SKIP_TO_EOL;
	    break;
	}
    SKIP_TO_EOL;
}

static void
hot_load_file (struct hotlist * grp)
{
    int		tkn;
    struct hotlist *new_grp;
    char	*label, *url;

    current_group = grp;

    while ((tkn = hot_next_token())!= TKN_EOF)
	switch (tkn) {
	case TKN_GROUP:
	    CHECK_TOKEN(TKN_STRING);
	    new_grp = add2hotlist (strdup (tkn_buf), 0, HL_TYPE_GROUP, 0);
	    SKIP_TO_EOL;
	    hot_load_group (new_grp);
	    current_group = grp;
	    break;
	case TKN_ENTRY:
	    CHECK_TOKEN(TKN_STRING);
	    label = strdup (tkn_buf);
	    CHECK_TOKEN(TKN_URL);
	    CHECK_TOKEN(TKN_STRING);
	    url = strdup (tkn_buf);
	    add2hotlist (label, url, HL_TYPE_ENTRY, 0);
	    SKIP_TO_EOL;
	    break;
	case TKN_COMMENT:
	    label = strdup (tkn_buf);
	    add2hotlist (label, 0, HL_TYPE_COMMENT, 0);
	    break;
	case TKN_EOL:
	    /* skip empty lines */
	    break;
	default:
	    hotlist_state.readonly = 1;
	    hotlist_state.file_error = 1;
	    SKIP_TO_EOL;
	    break;
	}
}

void
clean_up_hotlist_groups (char *section)
{
    char	*grp_section;
    void	*profile_keys;
    char	*key, *value;

    grp_section = copy_strings (section, ".Group", (char *)0);
    if (profile_has_section (section, profile_name))
	profile_clean_section (section, profile_name);
    if (profile_has_section (grp_section, profile_name)) {
	profile_keys = profile_init_iterator (grp_section, profile_name);

	while (profile_keys) {
	    profile_keys = profile_iterator_next (profile_keys, &key, &value);
	    clean_up_hotlist_groups (key);
	}
	profile_clean_section (grp_section, profile_name);
    }
    free (grp_section);
}



void load_hotlist (void)
{
    char	*grp_section;
    int		has_old_list = 0;
    int		remove_old_list = 0;
    struct stat stat_buf;

    if (hotlist_state.loaded) {
	stat (hotlist_file_name, &stat_buf);
	if (hotlist_file_mtime < stat_buf.st_mtime) 
	    done_hotlist ();
	else
	    return;
    }

    if (!hotlist_file_name)
	hotlist_file_name = concat_dir_and_file (home_dir, HOTLIST_FILENAME);
    
    hotlist	       = new_hotlist ();
    hotlist->type      = HL_TYPE_GROUP;
    hotlist->label     = strdup (" Top level group ");
    hotlist->up        = hotlist;
    /*
     * compatibility :-(
     */
    hotlist->directory = strdup ("Hotlist");

    grp_section = copy_strings ("Hotlist", ".Group", (char *)0);
    has_old_list = profile_has_section ("Hotlist", profile_name) ||
		   profile_has_section (grp_section, profile_name);
    free (grp_section);

    if ((hotlist_file = fopen (hotlist_file_name, "r")) == 0) {
	int	result;

	message (0, " Hotlist Load ",
		 "Hotlist is now kept in file ~/" HOTLIST_FILENAME,
		 "MC will load hotlist from ~/" PROFILE_NAME " and then",
		 "delete [Hotlist] section there"
	);
	load_group (hotlist);
	hotlist_state.loaded   = 1;
	/*
	 * just to be shure we got copy
	 */
	hotlist_state.modified = 1;
	result = save_hotlist ();
	hotlist_state.modified = 0;
	if (result) {
	    remove_old_list = 1;
	} else {
	    message (D_ERROR, " Hotlist Load ",
		     "MC was unable to write ~/" HOTLIST_FILENAME " file"
		     "your old hotlist entries were not deleted");
	}
    } else {
	hot_load_file (hotlist);
	fclose (hotlist_file);
	hotlist_state.loaded = 1;
	if (has_old_list) {
	    int		result;

	    result = query_dialog (" Hotlist Load ", 
	   "You have ~/" HOTLIST_FILENAME " file and [Hotlist] section in ~/" PROFILE_NAME "\n" /**/
	   "Your ~/" HOTLIST_FILENAME " most probably was created\n" /**/
	   "by an earlier development version of MC\n" /**/
	   "and is more actual than ~/" PROFILE_NAME " entries\n\n" /**/
	   "You can choose between\n\n" /**/
	   "  Remove - remove old hotlist entries from ~/" PROFILE_NAME "\n" /**/
	   "  Keep   - keep your old entries; you will be asked\n" /**/
	   "           the same question next time\n" /**/
	   "  Merge  - add old entries to hotlist as group \"Entries from ~/" PROFILE_NAME "\"\n\n",
				   D_ERROR, 3, "&Remove", "&Keep", "&Merge"
		      );
	    if (result == 0)
		remove_old_list = 1;
	    else if (result == 2) {
		struct hotlist	*grp = hotlist->head;
		struct hotlist	*old;

		hotlist->head = 0;
		load_group (hotlist);

		old            = new_hotlist ();
		old->type      = HL_TYPE_GROUP;
		old->label     = strdup (" Entries from ~/" PROFILE_NAME);
		old->up	       = hotlist;
		old->head      = hotlist->head;
		old->next      = grp;
		hotlist->head  = old;
		hotlist_state.modified = 1;
		if (!save_hotlist ())
		    message (D_ERROR, " Hotlist Load ",
			 "MC was unable to write ~/" HOTLIST_FILENAME " file"
			 "your old hotlist entries were not deleted");
		else
		    remove_old_list = 1;
		hotlist_state.modified = 0;
	    }
	}
    }

    if (remove_old_list) {
	clean_up_hotlist_groups ("Hotlist");
	sync_profiles ();
    }

    stat (hotlist_file_name, &stat_buf);
    hotlist_file_mtime = stat_buf.st_mtime;
    current_group = hotlist;
}

void save_group (struct hotlist *grp)
{
    struct hotlist *current = grp->head;
    char           *group_section;

    group_section = find_group_section (grp);
    
    profile_clean_section (group_section, profile_name);
    for (;current && current->type == HL_TYPE_GROUP; current = current->next){
	WritePrivateProfileString (group_section,
				   current->directory,
				   current->label,
				   profile_name);
    }
    free (group_section);

    for (current = grp->head;
	 current && current->type == HL_TYPE_GROUP;
	 current = current->next)
	 save_group (current);
    
    profile_clean_section (grp->directory, profile_name);
    for (;current; current = current->next){
	WritePrivateProfileString (grp->directory,
				   current->directory,
				   current->label,
				   profile_name);
    }
}

static int  list_level = 0;

void hot_save_group (struct hotlist *grp)
{
    struct hotlist *current = grp->head;
    int		    i;
    char	   *s;

#define INDENT(n) \
do { \
    for (i = 0; i < n; i++) \
	putc (' ', hotlist_file); \
} while (0)

    for (;current; current = current->next)
        switch (current->type) {
        case HL_TYPE_GROUP:
	    INDENT (list_level);
	    fputs ("GROUP \"", hotlist_file);
	    for (s = current->label; *s; s++) {
		if (*s == '"')
		    putc ('\\', hotlist_file);
		else if (*s == '\\')
		    putc ('\\', hotlist_file);
		putc (*s, hotlist_file);
	    }
	    fputs ("\"\n", hotlist_file);
	    list_level += 2;
	    hot_save_group (current);
	    list_level -= 2;
	    INDENT (list_level);
	    fputs ("ENDGROUP\n", hotlist_file);
	    break;
        case HL_TYPE_ENTRY:
	    INDENT(list_level);
	    fputs ("ENTRY \"", hotlist_file);
	    for (s = current->label; *s; s++) {
		if (*s == '"')
		    putc ('\\', hotlist_file);
		else if (*s == '\\')
		    putc ('\\', hotlist_file);
		putc (*s, hotlist_file);
	    }
	    fputs ("\" URL \"", hotlist_file);
	    for (s = current->directory; *s; s++) {
		if (*s == '"')
		    putc ('\\', hotlist_file);
		else if (*s == '\\')
		    putc ('\\', hotlist_file);
		putc (*s, hotlist_file);
	    }
	    fputs ("\"\n", hotlist_file);
	    break;
	case HL_TYPE_COMMENT:
	    fprintf (hotlist_file, "#%s\n", current->label);
	    break;

	}
}

int save_hotlist (void)
{
    int		saved = 0;
    struct      stat stat_buf;
    
    if (!hotlist_state.readonly && hotlist_state.modified && hotlist_file_name) {
	char	*fbak = copy_strings (hotlist_file_name, ".bak", 0);

	rename (hotlist_file_name, fbak);
	if ((hotlist_file = fopen (hotlist_file_name, "w")) != 0) {
	    if (stat (fbak, &stat_buf) == 0)
		chmod (hotlist_file_name, stat_buf.st_mode);
	    else
		chmod (hotlist_file_name, S_IRUSR | S_IWUSR);
	    hot_save_group (hotlist);
	    fflush (hotlist_file);
	    fclose (hotlist_file);
	    stat (hotlist_file_name, &stat_buf);
	    hotlist_file_mtime = stat_buf.st_mtime;
	    saved = 1;
	    hotlist_state.modified = 0;
	} else
	    rename (fbak, hotlist_file_name);
	free (fbak);
    }

    return saved;
}

void done_hotlist (void)
{
    remove_group (hotlist);
    hotlist_state.loaded = 0;
    if (hotlist->label)
        free (hotlist->label);
    if (hotlist->directory)
        free (hotlist->directory);
    free (hotlist);
    if (hotlist_file_name)
        free (hotlist_file_name);
    hotlist_file_name = 0;
    hotlist = current_group = 0;
    l_hotlist = 0;
    current_group = 0;
}



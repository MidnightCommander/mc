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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
 */

#include <config.h>
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "global.h"
#include "tty.h"		/* COLS */
#include "color.h"		/* dialog_colors */
#include "dlg.h"
#include "widget.h"
#include "dialog.h"		/* do_refresh() */
#include "setup.h"		/* For profile_bname */
#include "profile.h"		/* Load/save directories hotlist */
#include "wtools.h"		/* QuickDialog */
#include "panel.h"		/* cpanel */
#include "main.h"		/* repaint_screen */
#include "hotlist.h"
#include "key.h"		/* KEY_M_CTRL */
#include "command.h"		/* cmdline */

#define UX		5
#define UY		2

#define BX		UX
#define BY		(LINES - 6)

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

#ifdef	USE_VFS
#define B_FREE_ALL_VFS	(B_USER + 8)
#define B_REFRESH_VFS	(B_USER + 9)
#endif

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

static struct _hotlist_but {
    int ret_cmd, flags, y, x;
    char *text;
    int   type;
} hotlist_but[] = {
    { B_MOVE, NORMAL_BUTTON,         1,   42, N_("&Move"),       LIST_HOTLIST},
    { B_REMOVE, NORMAL_BUTTON,       1,   30, N_("&Remove"),     LIST_HOTLIST},
    { B_APPEND, NORMAL_BUTTON,       1,   15, N_("&Append"),     LIST_MOVELIST},
    { B_INSERT, NORMAL_BUTTON,       1,    0, N_("&Insert"),     LIST_MOVELIST},
    { B_NEW_ENTRY, NORMAL_BUTTON,    1,   15, N_("New &Entry"),  LIST_HOTLIST},
    { B_NEW_GROUP, NORMAL_BUTTON,    1,    0, N_("New &Group"),  LIST_HOTLIST},
    { B_CANCEL, NORMAL_BUTTON,       0,   53, N_("&Cancel"),     LIST_HOTLIST|LIST_VFSLIST|LIST_MOVELIST},
    { B_UP_GROUP, NORMAL_BUTTON,     0,   42, N_("&Up"),         LIST_HOTLIST|LIST_MOVELIST},
    { B_ADD_CURRENT, NORMAL_BUTTON,  0,   20, N_("&Add current"), LIST_HOTLIST},
#ifdef	USE_VFS
    { B_REFRESH_VFS, NORMAL_BUTTON,  0,   43, N_("&Refresh"),    LIST_VFSLIST},
    { B_FREE_ALL_VFS, NORMAL_BUTTON, 0,   20, N_("Fr&ee VFSs now"), LIST_VFSLIST},
#endif
    { B_ENTER, DEFPUSH_BUTTON,       0,    0, N_("Change &To"),  LIST_HOTLIST|LIST_VFSLIST|LIST_MOVELIST},
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

static struct hotlist *current_group;

static void init_movelist (int, struct hotlist *);
static void add_new_group_cmd (void);
static void add_new_entry_cmd (void);
static void remove_from_hotlist (struct hotlist *entry);
static void load_hotlist (void);

#define new_hotlist() g_new0(struct hotlist, 1)

static void
hotlist_refresh (Dlg_head * dlg)
{
    common_dialog_repaint (dlg);
    attrset (COLOR_NORMAL);
    draw_box (dlg, 2, 5, dlg->lines - (hotlist_state.moving ? 6 : 10),
	      dlg->cols - (UX * 2));
    if (!hotlist_state.moving)
	draw_box (dlg, dlg->lines - 8, 5, 3, dlg->cols - (UX * 2));
}

/* If current->data is 0, then we are dealing with a VFS pathname */
static inline void
update_path_name (void)
{
    char *text, *p;
    WListbox *list = hotlist_state.moving ? l_movelist : l_hotlist;
    Dlg_head *dlg = list->widget.parent;

    if (list->current) {
	if (list->current->data != 0) {
	    struct hotlist *hlp = (struct hotlist *) list->current->data;

	    if (hlp->type == HL_TYPE_ENTRY)
		text = hlp->directory;
	    else
		text = _("Subgroup - press ENTER to see list");
	} else {
	    text = list->current->text;
	}
    } else {
	text = "";
    }
    if (!hotlist_state.moving)
	label_set_text (pname,
			name_trunc (text, dlg->cols - (UX * 2 + 4)));

    p = g_strconcat (" ", current_group->label, " ", NULL);
    if (!hotlist_state.moving)
	label_set_text (pname_group,
			name_trunc (p, dlg->cols - (UX * 2 + 4)));
    else
	label_set_text (movelist_group,
			name_trunc (p, dlg->cols - (UX * 2 + 4)));
    g_free (p);

    dlg_redraw (dlg);
}

#define CHECK_BUFFER  \
do { \
    int               i; \
\
    if ((i = strlen (current->label) + 3) > buflen) { \
      g_free (buf); \
      buf = g_malloc (buflen = 1024 * (i/1024 + 1)); \
    } \
    buf[0] = '\0'; \
} while (0)

static void fill_listbox (void)
{
    struct hotlist *current = current_group->head;
    static char *buf;
    static int   buflen;

    if (!buf)
	buf = g_malloc (buflen = 1024);
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
	default:
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
    
#ifdef USE_VFS
static void add_name_to_list (char *path)
{
    listbox_add_item (l_hotlist, 0, 0, path, 0);
}
#endif /* !USE_VFS */

static int
hotlist_button_callback (int action)
{
    switch (action) {
    case B_MOVE:
	{
	    struct hotlist *saved = current_group;
	    struct hotlist *item;
	    struct hotlist *moveto_item = 0;
	    struct hotlist *moveto_group = 0;
	    int ret;

	    if (!l_hotlist->current)
		return MSG_NOT_HANDLED;	/* empty group - nothing to do */
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
		return MSG_NOT_HANDLED;
	    if (moveto_item == item)
		return MSG_NOT_HANDLED;	/* If we insert/append a before/after a
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
	    } else if (moveto_group->head == moveto_item) {
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
	    repaint_screen ();
	    hotlist_state.modified = 1;
	    return MSG_NOT_HANDLED;
	    break;
	}
    case B_REMOVE:
	if (l_hotlist->current && l_hotlist->current->data)
	    remove_from_hotlist (l_hotlist->current->data);
	return MSG_NOT_HANDLED;
	break;

    case B_NEW_GROUP:
	add_new_group_cmd ();
	return MSG_NOT_HANDLED;
	break;

    case B_ADD_CURRENT:
	add2hotlist_cmd ();
	return MSG_NOT_HANDLED;
	break;

    case B_NEW_ENTRY:
	add_new_entry_cmd ();
	return MSG_NOT_HANDLED;
	break;

    case B_ENTER:
	{
	    WListbox *list = hotlist_state.moving ? l_movelist : l_hotlist;
	    if (list->current) {
		if (list->current->data) {
		    struct hotlist *hlp =
			(struct hotlist *) list->current->data;
		    if (hlp->type == HL_TYPE_ENTRY)
			return MSG_HANDLED;
		    else {
			listbox_remove_list (list);
			current_group = hlp;
			fill_listbox ();
			return MSG_NOT_HANDLED;
		    }
		} else
		    return MSG_HANDLED;
	    }
	}
	/* Fall through if list empty - just go up */

    case B_UP_GROUP:
	{
	    WListbox *list = hotlist_state.moving ? l_movelist : l_hotlist;
	    listbox_remove_list (list);
	    current_group = current_group->up;
	    fill_listbox ();
	    return MSG_NOT_HANDLED;
	    break;
	}

#ifdef	USE_VFS
    case B_FREE_ALL_VFS:
	vfs_expire (1);
	/* fall through */

    case B_REFRESH_VFS:
	listbox_remove_list (l_hotlist);
	listbox_add_item (l_hotlist, 0, 0, home_dir, 0);
	vfs_fill_names (add_name_to_list);
	return MSG_NOT_HANDLED;
#endif				/* USE_VFS */

    default:
	return MSG_HANDLED;
	break;

    }
}

static cb_ret_t
hotlist_callback (Dlg_head *h, dlg_msg_t msg, int parm)
{
    switch (msg) {
    case DLG_DRAW:
	hotlist_refresh (h);
	return MSG_HANDLED;

    case DLG_UNHANDLED_KEY:
	switch (parm) {
	case KEY_M_CTRL | '\n':
	    goto l1;
	case '\n':
	case KEY_ENTER:
	case KEY_RIGHT:
	    if (hotlist_button_callback (B_ENTER)) {
		h->ret_value = B_ENTER;
		dlg_stop (h);
	    };
	    return MSG_HANDLED;
	    break;
	case KEY_LEFT:
	    if (hotlist_state.type != LIST_VFSLIST)
		return !hotlist_button_callback (B_UP_GROUP);
	    else
		return MSG_NOT_HANDLED;
	    break;
	case KEY_DC:
	    if (!hotlist_state.moving) {
		hotlist_button_callback (B_REMOVE);
		return MSG_HANDLED;
	    }
	    break;
	  l1:
	case ALT ('\n'):
	case ALT ('\r'):
	    if (!hotlist_state.moving) {
		if (l_hotlist->current) {
		    if (l_hotlist->current->data) {
			struct hotlist *hlp =
			    (struct hotlist *) l_hotlist->current->data;
			if (hlp->type == HL_TYPE_ENTRY) {
			    char *tmp =
				g_strconcat ("cd ", hlp->directory, NULL);
			    stuff (cmdline, tmp, 0);
			    g_free (tmp);
			    dlg_stop (h);
			    h->ret_value = B_CANCEL;
			    return MSG_HANDLED;
			}
		    }
		}
	    }
	    return MSG_HANDLED;	/* ignore key */
	}
	return MSG_NOT_HANDLED;

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
	return MSG_HANDLED;

    default:
	return default_dlg_callback (h, msg, parm);
    }
}

static int l_call (WListbox *list)
{
    Dlg_head *dlg = list->widget.parent;

    if (list->current){
	if (list->current->data) {
	    struct hotlist *hlp = (struct hotlist*) list->current->data;
	    if (hlp->type == HL_TYPE_ENTRY) {
		dlg->ret_value = B_ENTER;
		dlg_stop (dlg);
		return LISTBOX_DONE;
	    } else {
		hotlist_button_callback (B_ENTER);
		hotlist_callback (dlg, DLG_POST_KEY, '\n');
		return LISTBOX_CONT;
	    }
	} else {
	    dlg->ret_value = B_ENTER;
	    dlg_stop (dlg);
	    return LISTBOX_DONE;
	}
    }

    hotlist_button_callback (B_UP_GROUP);
    hotlist_callback (dlg, DLG_POST_KEY, 'u');
    return LISTBOX_CONT;
}

/*
 * Expands all button names (once) and recalculates button positions.
 * returns number of columns in the dialog box, which is 10 chars longer
 * then buttonbar.
 *
 * If common width of the window (i.e. in xterm) is less than returned
 * width - sorry :)  (anyway this did not handled in previous version too)
 */
static int
init_i18n_stuff(int list_type, int cols)
{
	register int i;
	static char* cancel_but = N_("&Cancel");

#ifdef ENABLE_NLS
	static int hotlist_i18n_flag = 0;

	if (!hotlist_i18n_flag)
	{
		i = sizeof (hotlist_but) / sizeof (hotlist_but [0]);
		while (i--)
			hotlist_but [i].text = _(hotlist_but [i].text);

		cancel_but = _(cancel_but);
		hotlist_i18n_flag = 1;
	}
#endif /* ENABLE_NLS */

	/* Dynamic resizing of buttonbars */
	{
		int len[2], count[2]; /* at most two lines of buttons */
		int cur_x[2], row;

		i = sizeof (hotlist_but) / sizeof (hotlist_but [0]);
		len[0] = len[1] = count[0] = count[1] = 0;

		/* Count len of buttonbars, assuming 2 extra space between buttons */
		while (i--)
		{
			if (! (hotlist_but[i].type & list_type))
				continue;

			row = hotlist_but [i].y;
			++count [row];
			len [row] += strlen (hotlist_but [i].text) + 5;
			if (hotlist_but [i].flags == DEFPUSH_BUTTON)
				len [row] += 2;
		}
		len[0] -= 2;
		len[1] -= 2;

		cols = max(cols, max(len[0], len[1]));

		/* arrange buttons */

		cur_x[0] = cur_x[1] = 0;
		i = sizeof (hotlist_but) / sizeof (hotlist_but [0]);
		while (i--)
		{
			if (! (hotlist_but[i].type & list_type))
				continue;

			row = hotlist_but [i].y;

			if (hotlist_but [i].x != 0) 
			{
				/* not first int the row */
				if (!strcmp (hotlist_but [i].text, cancel_but))
					hotlist_but [i].x = 
						cols - strlen (hotlist_but [i].text) - 13;
				else
					hotlist_but [i].x = cur_x [row];
			}

			cur_x [row] += strlen (hotlist_but [i].text) + 2
				+ (hotlist_but [i].flags == DEFPUSH_BUTTON ? 5 : 3);
		}
	}
	
	return cols;
}

static void
init_hotlist (int list_type)
{
    int i;
    char *title, *help_node;
    int hotlist_cols;

    hotlist_cols = init_i18n_stuff (list_type, COLS - 6);

    do_refresh ();

    hotlist_state.expanded =
	GetPrivateProfileInt ("HotlistConfig", "expanded_view_of_groups",
			      0, profile_name);

    if (list_type == LIST_VFSLIST) {
	title = _("Active VFS directories");
	help_node = "[vfshot]";	/* FIXME - no such node */
    } else {
	title = _("Directory hotlist");
	help_node = "[Hotlist]";
    }

    hotlist_dlg =
	create_dlg (0, 0, LINES - 2, hotlist_cols, dialog_colors,
		    hotlist_callback, help_node, title, DLG_CENTER | DLG_REVERSE);

    for (i = 0; i < BUTTONS; i++) {
	if (hotlist_but[i].type & list_type)
	    add_widget (hotlist_dlg,
			button_new (BY + hotlist_but[i].y,
				    BX + hotlist_but[i].x,
				    hotlist_but[i].ret_cmd,
				    hotlist_but[i].flags,
				    hotlist_but[i].text,
				    hotlist_button_callback));
    }

    /* We add the labels. 
     *    pname       will hold entry's pathname;
     *    pname_group will hold name of current group
     */
    pname = label_new (UY - 11 + LINES, UX + 2, "");
    add_widget (hotlist_dlg, pname);
    if (!hotlist_state.moving) {
	add_widget (hotlist_dlg,
		    label_new (UY - 12 + LINES, UX + 1,
			       _(" Directory path ")));

	/* This one holds the displayed pathname */
	pname_group = label_new (UY, UX + 1, _(" Directory label "));
	add_widget (hotlist_dlg, pname_group);
    }
    /* get new listbox */
    l_hotlist =
	listbox_new (UY + 1, UX + 1, COLS - 2 * UX - 8, LINES - 14,
		     l_call);

    /* Fill the hotlist with the active VFS or the hotlist */
#ifdef USE_VFS
    if (list_type == LIST_VFSLIST) {
	listbox_add_item (l_hotlist, 0, 0, home_dir, 0);
	vfs_fill_names (add_name_to_list);
    } else
#endif				/* !USE_VFS */
	fill_listbox ();

    add_widget (hotlist_dlg, l_hotlist);
    /* add listbox to the dialogs */
}

static void
init_movelist (int list_type, struct hotlist *item)
{
    int i;
    char *hdr = g_strdup_printf (_("Moving %s"), item->label);
    int movelist_cols = init_i18n_stuff (list_type, COLS - 6);

    do_refresh ();

    movelist_dlg =
	create_dlg (0, 0, LINES - 6, movelist_cols, dialog_colors,
		    hotlist_callback, "[Hotlist]", hdr, DLG_CENTER | DLG_REVERSE);
    g_free (hdr);

    for (i = 0; i < BUTTONS; i++) {
	if (hotlist_but[i].type & list_type)
	    add_widget (movelist_dlg,
			button_new (BY - 4 + hotlist_but[i].y,
				    BX + hotlist_but[i].x,
				    hotlist_but[i].ret_cmd,
				    hotlist_but[i].flags,
				    hotlist_but[i].text,
				    hotlist_button_callback));
    }

    /* We add the labels.  We are interested in the last one,
     * that one will hold the path name label
     */
    movelist_group = label_new (UY, UX + 1, _(" Directory label "));
    add_widget (movelist_dlg, movelist_group);
    /* get new listbox */
    l_movelist =
	listbox_new (UY + 1, UX + 1, movelist_dlg->cols - 2 * UX - 2,
		     movelist_dlg->lines - 8, l_call);

    fill_listbox ();

    add_widget (movelist_dlg, l_movelist);
    /* add listbox to the dialogs */
}

/*
 * Destroy the list dialog.
 * Don't confuse with done_hotlist() for the list in memory.
 */
static void hotlist_done (void)
{
    destroy_dlg (hotlist_dlg);
    l_hotlist = NULL;
    if (0)
	update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    repaint_screen ();
}

static char *
find_group_section (struct hotlist *grp)
{
    return  g_strconcat (grp->directory, ".Group", NULL);

}


/* 1.11.96 bor: added pos parameter to control placement of new item.
   see widget.c, listbox_add_item()
                now hotlist is in unsorted mode
 */
static struct hotlist *
add2hotlist (char *label, char *directory, enum HotListType type, int pos)
{
    struct hotlist *new;
    struct hotlist *current = NULL;

    /*
     * Hotlist is neither loaded nor loading.
     * Must be called by "Ctrl-x a" before using hotlist.
     */
    if (!current_group)
	load_hotlist ();

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
	    char  *lbl = g_strconcat ("->", new->label, NULL);

	    listbox_add_item (l_hotlist, pos, 0, lbl, new);
	    g_free (lbl);
	} else
	    listbox_add_item (l_hotlist, pos, 0, new->label, new);
	listbox_select_entry (l_hotlist, l_hotlist->current);
    }
    return new;

}

#ifdef ENABLE_NLS
/*
 * Support routine for add_new_entry_input()/add_new_group_input()
 * Change positions of buttons (first three widgets).
 *
 * This is just a quick hack. Accurate procedure must take care of
 * internationalized label lengths and total buttonbar length...assume
 * 64 is longer anyway.
 */
static void add_widgets_i18n(QuickWidget* qw, int len)
{
	int i, l[3], space, cur_x;

	for (i = 0; i < 3; i++)
	{
		qw [i].text = _(qw [i].text);
		l[i] = strlen (qw [i].text) + 3;
	}
	space = (len - 4 - l[0] - l[1] - l[2]) / 4;

	for (cur_x = 2 + space, i = 3; i--; cur_x += l[i] + space)
	{
		qw [i].relative_x = cur_x;
		qw [i].x_divisions = len;
	}
}
#endif /* ENABLE_NLS */

static int add_new_entry_input (char *header, char *text1, char *text2, char *help, char **r1, char **r2)
{
#define RELATIVE_Y_BUTTONS	4
#define RELATIVE_Y_LABEL_PTH	3
#define RELATIVE_Y_INPUT_PTH	4

    QuickDialog Quick_input;
    static QuickWidget quick_widgets [] = {
    { quick_button, 55, 80, RELATIVE_Y_BUTTONS, 0, N_("&Cancel"), 0, B_CANCEL, 
	  0, 0, "button-cancel" },
    { quick_button, 30, 80, RELATIVE_Y_BUTTONS, 0, N_("&Insert"), 0, B_INSERT, 
	  0, 0, "button-insert" },
    { quick_button, 10, 80, RELATIVE_Y_BUTTONS, 0, N_("&Append"), 0, B_APPEND, 
	  0, 0, "button-append" },
    { quick_input,  4, 80, RELATIVE_Y_INPUT_PTH, 0, "",58, 0, 
	  0, 0, "input-pth" },
    { quick_label, RELATIVE_Y_LABEL_PTH, 80, 3, 0, 0, 0, 0, 
	  0, 0, "label-pth" },
    { quick_input,  4, 80, 3, 0, "", 58, 0, 
	  0, 0, "input-lbl" },
    { quick_label,  3, 80, 2, 0, 0, 0, 0, 
	  0, 0, "label-lbl" },
    { 0 } };
    
    int len;
    int i;
    int lines1, lines2;
    char *my_str1, *my_str2;
    
#ifdef ENABLE_NLS
	static int i18n_flag = 0;
#endif /* ENABLE_NLS */

    len = max (strlen (header), msglen (text1, &lines1));
    len = max (len, msglen (text2, &lines2)) + 4;
    len = max (len, 64);

#ifdef ENABLE_NLS
	if (!i18n_flag)
	{
		add_widgets_i18n(quick_widgets, len);
		i18n_flag = 1;
	}
#endif /* ENABLE_NLS */

    Quick_input.xlen  = len;
    Quick_input.xpos  = -1;
    Quick_input.title = header;
    Quick_input.help  = help;
    Quick_input.i18n  = 0;
    quick_widgets [6].text = text1;
    quick_widgets [4].text = text2;
    quick_widgets [5].text = *r1;
    quick_widgets [3].text = *r2;

    for (i = 0; i < 7; i++)
	quick_widgets [i].y_divisions = lines1+lines2+7;
    Quick_input.ylen  = lines1 + lines2 + 7;

    quick_widgets [0].relative_y = RELATIVE_Y_BUTTONS + (lines1 + lines2);
    quick_widgets [1].relative_y = RELATIVE_Y_BUTTONS + (lines1 + lines2);
    quick_widgets [2].relative_y = RELATIVE_Y_BUTTONS + (lines1 + lines2);
    quick_widgets [3].relative_y = RELATIVE_Y_INPUT_PTH + (lines1);
    quick_widgets [4].relative_y = RELATIVE_Y_LABEL_PTH + (lines1);

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

    ret = add_new_entry_input (_("New hotlist entry"), _("Directory label"), _("Directory path"),
	 "[Hotlist]", &title, &url);

    if (!ret || !title || !*title || !url || !*url)
	return;

    if (ret == B_ENTER || ret == B_APPEND)
	add2hotlist (g_strdup (title),g_strdup (url), HL_TYPE_ENTRY, 2);
    else
	add2hotlist (g_strdup (title),g_strdup (url), HL_TYPE_ENTRY, 1);

    hotlist_state.modified = 1;
}

static int add_new_group_input (char *header, char *label, char **result)
{
    int		ret;
    QuickDialog Quick_input;
    static QuickWidget quick_widgets [] = {
    { quick_button, 55, 80, 1, 0, N_("&Cancel"), 0, B_CANCEL, 0, 0,
	  "button-cancel" },
    { quick_button, 30, 80, 1, 0, N_("&Insert"), 0, B_INSERT, 0, 0,
	  "button-insert" },
    { quick_button, 10, 80, 1, 0, N_("&Append"), 0, B_APPEND, 0, 0,
	  "button-append" },
    { quick_input,  4, 80,  0, 0, "", 58, 0, 0, 0, "input" },
    { quick_label,  3, 80,  2, 0,  0,  0, 0, 0, 0, "label" },
    { 0 } };
    int relative_y[] = {1, 1, 1, 0, 2}; /* the relative_x component from the
                                           quick_widgets variable above */
    int len;
    int i;
    int lines;
    char *my_str;

#ifdef ENABLE_NLS
	static int i18n_flag = 0;
#endif /* ENABLE_NLS */
    
    len = max (strlen (header), msglen (label, &lines)) + 4;
    len = max (len, 64);

#ifdef ENABLE_NLS
	if (!i18n_flag)
	{
		add_widgets_i18n(quick_widgets, len);
		i18n_flag = 1;
	}
#endif /* ENABLE_NLS */

    Quick_input.xlen  = len;
    Quick_input.xpos  = -1;
    Quick_input.title = header;
    Quick_input.help  = "[Hotlist]";
    Quick_input.i18n  = 0;
    quick_widgets [4].text = label;

    for (i = 0; i < 5; i++)
	quick_widgets [i].y_divisions = lines+6;
    Quick_input.ylen  = lines + 6;

    for (i = 0; i < 4; i++)
	quick_widgets [i].relative_y = relative_y[i] + 2 + lines;

    quick_widgets [3].str_result = &my_str;
    quick_widgets [3].text       = "";
    
    Quick_input.widgets = quick_widgets;
    if ((ret = quick_dialog (&Quick_input)) != B_CANCEL){
	*result = *(quick_widgets [3].str_result);
	return ret;
    } else
	return 0;
}

static void add_new_group_cmd (void)
{
    char   *label;
    int		ret;

    ret = add_new_group_input (_(" New hotlist group "), _("Name of new group"), &label);
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
    char *cp = _("Label for \"%s\":");
    int l = strlen (cp);

    prompt = g_strdup_printf (cp, name_trunc (cpanel->cwd, COLS-2*UX-(l+8)));
    label = input_dialog (_(" Add to hotlist "), prompt, cpanel->cwd);
    g_free (prompt);
    if (!label || !*label)
	return;

    add2hotlist (label,g_strdup (cpanel->cwd), HL_TYPE_ENTRY, 0);
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
	    g_free (current->label);
	if (current->directory)
	    g_free (current->directory);
	 g_free (current);

	current = next;
    }

}
	
static void remove_from_hotlist (struct hotlist *entry)
{
    if (entry->type == HL_TYPE_GROUP) {
	if (entry->head) {
	    char *header;
	    int   result;

	    header = g_strconcat (_(" Remove: "),
				   name_trunc (entry->label, 30),
				   " ",
				   NULL);
	    result = query_dialog (header, _("\n Group not empty.\n Remove it?"),
				   D_ERROR, 2,
				   _("&No"), _("&Yes"));
	    g_free (header);

	    if (!result)
		return;
	}

	remove_group (entry);
    }

    unlink_entry (entry);

    if (entry->label)
          g_free (entry->label);
    if (entry->directory)
        g_free (entry->directory);
    g_free (entry);
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
	    target = g_strdup (hlp->directory);
	} else
	    target = g_strdup (l_hotlist->current->text);
	break;
    }

    hotlist_done ();
    return target;
}

static void
load_group (struct hotlist *grp)
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
	add2hotlist (g_strdup (value), g_strdup (key), HL_TYPE_GROUP, 0);
    }
    g_free (group_section);

    profile_keys = profile_init_iterator (grp->directory, profile_name);

    while (profile_keys){
	profile_keys = profile_iterator_next (profile_keys, &key, &value);
	add2hotlist (g_strdup (value),g_strdup (key), HL_TYPE_ENTRY, 0);
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

static int hot_skip_blanks (void)
{
    int c;

    while ((c = getc (hotlist_file)) != EOF && c != '\n' && isspace (c))
	;
    return c;
    
}

static int hot_next_token (void)
{
    int	c;

#define CHECK_BUF() \
do { \
    if (tkn_length == tkn_buf_length) \
	tkn_buf = tkn_buf ? ( g_realloc (tkn_buf, tkn_buf_length += 1024)) \
			  : ( g_malloc (tkn_buf_length = 1024)); \
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
	    new_grp = add2hotlist (g_strdup (tkn_buf), 0, HL_TYPE_GROUP, 0);
	    SKIP_TO_EOL;
	    hot_load_group (new_grp);
	    current_group = grp;
	    break;
	case TKN_ENTRY:
	    CHECK_TOKEN(TKN_STRING);
	    label = g_strdup (tkn_buf);
	    CHECK_TOKEN(TKN_URL);
	    CHECK_TOKEN(TKN_STRING);
	    url = g_strdup (tkn_buf);
	    add2hotlist (label, url, HL_TYPE_ENTRY, 0);
	    SKIP_TO_EOL;
	    break;
	case TKN_COMMENT:
	    label = g_strdup (tkn_buf);
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
	    new_grp = add2hotlist (g_strdup (tkn_buf), 0, HL_TYPE_GROUP, 0);
	    SKIP_TO_EOL;
	    hot_load_group (new_grp);
	    current_group = grp;
	    break;
	case TKN_ENTRY:
	    CHECK_TOKEN(TKN_STRING);
	    label = g_strdup (tkn_buf);
	    CHECK_TOKEN(TKN_URL);
	    CHECK_TOKEN(TKN_STRING);
	    url = g_strdup (tkn_buf);
	    add2hotlist (label, url, HL_TYPE_ENTRY, 0);
	    SKIP_TO_EOL;
	    break;
	case TKN_COMMENT:
	    label = g_strdup (tkn_buf);
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

static void
clean_up_hotlist_groups (char *section)
{
    char	*grp_section;
    void	*profile_keys;
    char	*key, *value;

    grp_section = g_strconcat (section, ".Group", NULL);
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
    g_free (grp_section);
}



static void
load_hotlist (void)
{
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
    hotlist->label     = g_strdup (_(" Top level group "));
    hotlist->up        = hotlist;
    /*
     * compatibility :-(
     */
    hotlist->directory = g_strdup ("Hotlist");

    if ((hotlist_file = fopen (hotlist_file_name, "r")) == 0) {
	int	result;

	load_group (hotlist);
	hotlist_state.loaded   = 1;
	/*
	 * just to be sure we got copy
	 */
	hotlist_state.modified = 1;
	result = save_hotlist ();
	hotlist_state.modified = 0;
	if (result) {
	    remove_old_list = 1;
	} else {
	    char *msg;

	    msg = g_strconcat (_("MC was unable to write ~/"), HOTLIST_FILENAME,
				_(" file, your old hotlist entries were not deleted"), NULL);

	    message (D_ERROR, _(" Hotlist Load "), msg);
	    g_free (msg);
	}
    } else {
	hot_load_file (hotlist);
	fclose (hotlist_file);
	hotlist_state.loaded = 1;
    }

    if (remove_old_list) {
	clean_up_hotlist_groups ("Hotlist");
	sync_profiles ();
    }

    stat (hotlist_file_name, &stat_buf);
    hotlist_file_mtime = stat_buf.st_mtime;
    current_group = hotlist;
}

static void
save_group (struct hotlist *grp)
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
    g_free (group_section);

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

static void
hot_save_group (struct hotlist *grp)
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
	char	*fbak = g_strconcat (hotlist_file_name, ".bak", NULL);

	rename (hotlist_file_name, fbak);
	if ((hotlist_file = fopen (hotlist_file_name, "w")) != 0) {
	    if (stat (fbak, &stat_buf) == 0)
		chmod (hotlist_file_name, stat_buf.st_mode);
	    else
		chmod (hotlist_file_name, S_IRUSR | S_IWUSR);
	    hot_save_group (hotlist);
	    fclose (hotlist_file);
	    stat (hotlist_file_name, &stat_buf);
	    hotlist_file_mtime = stat_buf.st_mtime;
	    saved = 1;
	    hotlist_state.modified = 0;
	} else
	    rename (fbak, hotlist_file_name);
	g_free (fbak);
    }

    return saved;
}

/*
 * Unload list from memory.
 * Don't confuse with hotlist_done() for GUI.
 */
void done_hotlist (void)
{
    if (hotlist){
	remove_group (hotlist);
	if (hotlist->label)
	   g_free (hotlist->label);
        if (hotlist->directory)
            g_free (hotlist->directory);
        g_free (hotlist);
	hotlist = 0;
    }
    
    hotlist_state.loaded = 0;

    if (hotlist_file_name){
        g_free (hotlist_file_name);
	hotlist_file_name = 0;
    }
    l_hotlist = 0;
    current_group = 0;
    if (tkn_buf){
        g_free (tkn_buf);
	tkn_buf_length = 0;
	tkn_length = 0;
	tkn_buf = NULL;
    }
}


#ifndef __PANEL_H
#define __PANEL_H

#include "fs.h"
#include "dir.h"     /* file_entry */
#include "dlg.h"
#include "util.h"
#include "widget.h"	/* for history loading and saving */

#define LIST_TYPES	5

enum list_types {
    list_full,			/* Name, size, perm/date */
    list_brief,			/* Name */
    list_long,			/* Like ls -l */
    list_user,			/* User defined */
    list_icons			/* iconic display */
};

enum view_modes {
    view_listing,		/* Directory listing */
    view_info,			/* Information panel */
    view_tree,			/* Tree view */
    view_quick,			/* Quick view */
    view_nothing		/* Undefined */
};

enum panel_display_enum {
    frame_full,			/* full screen frame */
    frame_half			/* half screen frame */
};

#define is_view_special(x) (((x) == view_info) || ((x) == view_quick))

#define J_LEFT 		1
#define J_RIGHT		2
#define J_CENTER	3

#define IS_FIT(x)	((x) & 0x0004)
#define MAKE_FIT(x)	((x) | 0x0004)
#define HIDE_FIT(x)	((x) & 0x0003)

#define J_LEFT_FIT	5
#define J_RIGHT_FIT	6
#define J_CENTER_FIT	7

#define NORMAL		0
#define SELECTED	1
#define MARKED		2
#define MARKED_SELECTED	3
#define STATUS		5

/*
 * This describes a format item.  The parse_display_format routine parses
 * the user specified format and creates a linked list of format_e structures.
 *
 * parse_display_format computes the actual field allocations if
 * the COMPUTE_FORMAT_ALLOCATIONs define is set.  MC frontends that are
 * just interested in the parsed display format should not set this define.
 */
typedef struct format_e {
    struct format_e *next;
    int    requested_field_len;
    int    field_len;
    int    just_mode;
    int    expand;
    char   *(*string_fn)(file_entry *, int len);
    char   *title;
    char   *id;

    /* first format_e has the number of items */
    int    items;
    int    use_in_gui;
} format_e;

typedef struct {
    Widget   widget;
    dir_list dir;		/* Directory contents */

    int      list_type;		/* listing type (was view_type) */
    int      active;		/* If panel is currently selected */
    char     cwd [MC_MAXPATHLEN];/* Current Working Directory */
    char     lwd [MC_MAXPATHLEN];/* Last Working Directory */
    Hist     *dir_history;	/* directory history */
    char     *hist_name;	/* directory history name for history file */
    int      count;		/* Number of files in dir structure */
    int      marked;		/* Count of marked files */
    int      dirs_marked;	/* Count of marked directories */
    double   total;		/* Bytes in marked files */
    int      top_file;		/* The file showed on the top of the panel */
    int      selected;		/* Index to the selected file */
    int      reverse;		/* Show listing in reverse? */
    int      case_sensitive;    /* Listing is case sensitive? */
    int      split;		/* Split panel to allow two columns */
    int      is_panelized;	/* Flag: special filelisting, can't reload */
    int      frame_size;	/* half or full frame */
    int      icons_per_row;     /* Icon view; how many icons displayed per row */
    sortfn   *sort_type;	/* Sort type */
    char     *filter;		/* File name filter */

    int      dirty;		/* Should we redisplay the panel? */

    int	     user_mini_status;  	/* Is user_status_format used */
    char     *user_format;      	/* User format */
    char     *user_status_format[LIST_TYPES];/* User format for status line */

    format_e *format;		/* Display format */
    format_e *status_format;    /* Mini status format */

    int      format_modified;	/* If the format was changed this is set */

    char     *panel_name;	/* The panel name */
    struct   stat dir_stat;	/* Stat of current dir: used by execute () */

    char     *gc;
    void     *font;
    int	     item_height;
    int	     total_width;
    int	     ascent;
    int	     descent;

    int      searching;
    char     search_buffer [256];

    void     *port_ui;		/* UI stuff specific to each GUI port */
} WPanel;

WPanel *panel_new (const char *panel_name);
void panel_set_size (WPanel *panel, int x1, int y1, int x2, int y2);
void paint_paint (WPanel *panel);
void panel_refresh (WPanel *panel);
void Xtry_to_select (WPanel *panel, char *name);
void panel_clean_dir (WPanel *panel);

extern int torben_fj_mode;
extern int permission_mode;
extern int filetype_mode;
extern int show_mini_info;
extern int panel_scroll_pages;

#define selection(p) (&(p->dir.list [p->selected]))

extern int fast_reload;
extern int extra_info;

/*#define ITEMS(p) ((p)->view_type == view_brief ? (p)->lines *2 : (p)->lines)
*/
/* The return value of panel_reload */
#define CHANGED 1

#define PANEL_ISVIEW(p) (p->view_type == view_brief || \
			 p->view_type == view_full  || \
			 p->view_type == view_long  || \
			 p->view_type == view_user  || \
			 p->view_type == view_tree)

#define RP_ONLY_PAINT 0
#define RP_SETPOS 1

void paint_panel          (WPanel *panel);
void display_mini_info    (WPanel *panel);
void panel_reload         (WPanel *panel);

void panel_set_sort_order (WPanel *panel, sortfn *sort_order);
void panel_re_sort        (WPanel *panel);

void x_panel_set_size        (int index);
void x_create_panel          (Dlg_head *h, widget_data parent, WPanel *panel);
void x_fill_panel            (WPanel *panel);
void x_adjust_top_file       (WPanel *panel);
void x_filter_changed        (WPanel *panel);
void x_add_sort_label        (WPanel *panel, int index, char *text, char *tag, void *sr);
void x_sort_label_start      (WPanel *panel);
void x_reset_sort_labels     (WPanel *panel);
void x_panel_destroy         (WPanel *panel);
void change_view             (WPanel *panel, int view_type);
void x_panel_update_marks    (WPanel *panel);

extern void paint_info_panel (WPanel *);
extern void paint_quick_view_panel (WPanel *);
void info_frame (WPanel *panel);
extern WPanel *the_info_panel;
void panel_update_contents (WPanel *panel);
void panel_update_cols (Widget *widget, int frame_size);
int set_panel_formats (WPanel *p);

WPanel *get_current_panel (void);
WPanel *get_other_panel (void);

#define other_panel get_other_panel()

extern WPanel *left_panel;
extern WPanel *right_panel;
extern WPanel *current_panel;

void try_to_select (WPanel *panel, char *name);

#define DEFAULT_USER_FORMAT "half type,name,|,size,|,perm"

/* This were in main: */
void unmark_files (WPanel *panel);
void select_item (WPanel *panel);

extern Hook *select_file_hook;

void recalculate_panel_summary (WPanel *panel);
void file_mark (WPanel *panel, int index, int val);
void do_file_mark (WPanel *panel, int index, int val);

void    x_panel_select_item (WPanel *panel, int index, int val);
void    x_select_item (WPanel *panel);
void    x_unselect_item (WPanel *panel);
sortfn *get_sort_fn (char *name);
void    update_one_panel_widget (WPanel *panel, int force_update, char *current_file);
void    panel_update_marks (WPanel *panel);

void directory_history_next (WPanel * panel);
void directory_history_prev (WPanel * panel);
void directory_history_list (WPanel * panel);

#endif	/* __PANEL_H */

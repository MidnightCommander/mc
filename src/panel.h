
/** \file panel.h
 *  \brief Header: defines WPanel structure
 */

#ifndef MC_PANEL_H
#define MC_PANEL_H

#include "dir.h"		/* dir_list */
#include "dialog.h"		/* Widget */
#include "fs.h"			/* MC_MAXPATHLEN */
#include "strutil.h"

#define selection(p) (&(p->dir.list[p->selected]))
#define DEFAULT_USER_FORMAT "half type name | size | perm"

#define LIST_TYPES	4

enum list_types {
    list_full,			/* Name, size, perm/date */
    list_brief,			/* Name */
    list_long,			/* Like ls -l */
    list_user			/* User defined */
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

struct format_e;

typedef struct panel_format_struct {
    const char *id;
    int  min_size;
    int  expands;
    align_crt_t default_just;
    const char *title;
    const char *title_hotkey;
    int  use_in_gui;
    gboolean use_in_user_format;
    const char *(*string_fn)(file_entry *, int);
    sortfn *sort_routine; /* used by mouse_sort_col() */
} panel_field_t;

extern panel_field_t panel_fields [];

typedef struct WPanel {
    Widget   widget;
    dir_list dir;		/* Directory contents */

    int      list_type;		/* listing type (was view_type) */
    int      active;		/* If panel is currently selected */
    char     cwd [MC_MAXPATHLEN];/* Current Working Directory */
    char     lwd [MC_MAXPATHLEN];/* Last Working Directory */
    GList    *dir_history;	/* directory history */
    char     *hist_name;	/* directory history name for history file */
    int      count;		/* Number of files in dir structure */
    int      marked;		/* Count of marked files */
    int      dirs_marked;	/* Count of marked directories */
    double   total;		/* Bytes in marked files */
    int      top_file;		/* The file showed on the top of the panel */
    int      selected;		/* Index to the selected file */
    int      reverse;		/* Show listing in reverse? */
    int      case_sensitive;    /* Listing is case sensitive? */
    int      exec_first;	/* Show executable top in list? */
    int      split;		/* Split panel to allow two columns */
    int      is_panelized;	/* Flag: special filelisting, can't reload */
    int      frame_size;	/* half or full frame */
    const panel_field_t *current_sort_field;
    char     *filter;		/* File name filter */

    int      dirty;		/* Should we redisplay the panel? */

    int	     user_mini_status;	/* Is user_status_format used */
    char     *user_format;      /* User format */
    char     *user_status_format[LIST_TYPES];/* User format for status line */

    struct   format_e *format;		/* Display format */
    struct   format_e *status_format;    /* Mini status format */

    int      format_modified;	/* If the format was changed this is set */

    char     *panel_name;	/* The panel name */
    struct   stat dir_stat;	/* Stat of current dir: used by execute () */

    int      searching;
    char     search_buffer [256];
    char     search_char [MB_LEN_MAX]; /*buffer for multibytes characters*/
    int      search_chpoint;           /*point after last characters in search_char*/
} WPanel;

WPanel *panel_new (const char *panel_name);
WPanel *panel_new_with_dir (const char *panel_name, const char *dr);
void panel_clean_dir (WPanel *panel);

extern int torben_fj_mode;
extern int permission_mode;
extern int filetype_mode;
extern int show_mini_info;
extern int panel_scroll_pages;
extern int fast_reload;

void panel_reload         (WPanel *panel);
void panel_set_sort_order (WPanel *panel, const panel_field_t *sort_order);
void panel_re_sort        (WPanel *panel);
void set_panel_encoding (WPanel *);

#define UP_OPTIMIZE		0
#define UP_RELOAD		1
#define UP_ONLY_CURRENT		2

#define UP_KEEPSEL ((char *) -1)

void update_dirty_panels (void);
void update_panels (int force_update, const char *current_file);
void panel_update_cols (Widget *widget, int frame_size);
int set_panel_formats (WPanel *p);

#define other_panel get_other_panel()

extern WPanel *left_panel;
extern WPanel *right_panel;
extern WPanel *current_panel;

void try_to_select (WPanel *panel, const char *name);

void unmark_files (WPanel *panel);
void select_item (WPanel *panel);

extern Hook *select_file_hook;

void recalculate_panel_summary (WPanel *panel);
void file_mark (WPanel *panel, int index, int val);
void do_file_mark (WPanel *panel, int index, int val);

void directory_history_next (WPanel *panel);
void directory_history_prev (WPanel *panel);
void directory_history_list (WPanel *panel);

gsize panel_get_num_of_sortable_fields(void);
const char **panel_get_sortable_fields(gsize *);
const panel_field_t *panel_get_field_by_id(const char *);
const panel_field_t *panel_get_field_by_title(const char *);
const panel_field_t *panel_get_field_by_title_hotkey(const char *);
gsize panel_get_num_of_user_possible_fields(void);
const char **panel_get_user_possible_fields(gsize *);

#endif

/** \file panel.h
 *  \brief Header: defines WPanel structure
 */

#ifndef MC__PANEL_H
#define MC__PANEL_H

#include <inttypes.h>           /* uintmax_t */

#include "lib/global.h"         /* gboolean */
#include "lib/fs.h"             /* MC_MAXPATHLEN */
#include "lib/strutil.h"
#include "lib/widget.h"         /* Widget */
#include "lib/filehighlight.h"

#include "dir.h"                /* dir_list */

/*** typedefs(not structures) and defined constants **********************************************/

#define PANEL(x) ((WPanel *)(x))
#define selection(p) (&(p->dir.list[p->selected]))
#define DEFAULT_USER_FORMAT "half type name | size | perm"

#define LIST_TYPES 4

#define UP_KEEPSEL ((char *) -1)

/*** enums ***************************************************************************************/

typedef enum
{
    list_full,                  /* Name, size, perm/date */
    list_brief,                 /* Name */
    list_long,                  /* Like ls -l */
    list_user                   /* User defined */
} list_type_t;

typedef enum
{
    frame_full,                 /* full screen frame */
    frame_half                  /* half screen frame */
} panel_display_t;

typedef enum
{
    UP_OPTIMIZE = 0,
    UP_RELOAD = 1,
    UP_ONLY_CURRENT = 2
} panel_update_flags_t;

/* selection flags */
typedef enum
{
    SELECT_FILES_ONLY = 1 << 0,
    SELECT_MATCH_CASE = 1 << 1,
    SELECT_SHELL_PATTERNS = 1 << 2
} panel_select_flags_t;

/* run mode and params */

enum cd_enum
{
    cd_parse_command,
    cd_exact
};

/*** structures declarations (and typedefs of structures)*****************************************/

struct format_e;

typedef struct panel_field_struct
{
    const char *id;
    int min_size;
    gboolean expands;
    align_crt_t default_just;
    const char *hotkey;
    const char *title_hotkey;
    gboolean is_user_choice;
    gboolean use_in_user_format;
    const char *(*string_fn) (file_entry_t *, int);
    GCompareFunc sort_routine;  /* used by mouse_sort_col() */
} panel_field_t;

typedef struct
{
    dir_list list;
    vfs_path_t *root_vpath;
} panelized_panel_t;

typedef struct
{
    Widget widget;
    dir_list dir;               /* Directory contents */

    list_type_t list_type;      /* listing type */
    int active;                 /* If panel is currently selected */
    vfs_path_t *cwd_vpath;      /* Current Working Directory */
    vfs_path_t *lwd_vpath;      /* Last Working Directory */
    GList *dir_history;         /* directory history */
    GList *dir_history_current; /* pointer to the current history item */
    char *hist_name;            /* directory history name for history file */
    int marked;                 /* Count of marked files */
    int dirs_marked;            /* Count of marked directories */
    uintmax_t total;            /* Bytes in marked files */
    int top_file;               /* The file showed on the top of the panel */
    int selected;               /* Index to the selected file */
    int list_cols;              /* Number of file list columns */
    int brief_cols;             /* Number of columns in case of list_brief format */
    gboolean is_panelized;      /* Flag: special filelisting, can't reload */
    panel_display_t frame_size; /* half or full frame */
    char *filter;               /* File name filter */

    /* sort */
    dir_sort_options_t sort_info;
    const panel_field_t *sort_field;

    int dirty;                  /* Should we redisplay the panel? */

    gboolean user_mini_status;  /* Is user_status_format used */
    char *user_format;          /* User format */
    char *user_status_format[LIST_TYPES];       /* User format for status line */

    struct format_e *format;    /* Display format */
    struct format_e *status_format;     /* Mini status format */

    int format_modified;        /* If the format was changed this is set */

    char *panel_name;           /* The panel name */
    struct stat dir_stat;       /* Stat of current dir: used by execute () */

#ifdef HAVE_CHARSET
    int codepage;               /* panel codepage */
#endif

    gboolean searching;
    char search_buffer[MC_MAXFILENAMELEN];
    char prev_search_buffer[MC_MAXFILENAMELEN];
    char search_char[MB_LEN_MAX];       /*buffer for multibytes characters */
    int search_chpoint;         /*point after last characters in search_char */
    int content_shift;          /* Number of characters of filename need to skip from left side. */
    int max_shift;              /* Max shift for visible part of current panel */
} WPanel;

/*** global variables defined in .c file *********************************************************/

extern panelized_panel_t panelized_panel;

extern hook_t *select_file_hook;

extern mc_fhl_t *mc_filehighlight;

/*** declarations of public functions ************************************************************/

WPanel *panel_new_with_dir (const char *panel_name, const vfs_path_t * vpath);
void panel_clean_dir (WPanel * panel);

void panel_reload (WPanel * panel);
void panel_set_sort_order (WPanel * panel, const panel_field_t * sort_order);
void panel_re_sort (WPanel * panel);

#ifdef HAVE_CHARSET
void panel_change_encoding (WPanel * panel);
vfs_path_t *remove_encoding_from_path (const vfs_path_t * vpath);
#endif

void update_panels (panel_update_flags_t flags, const char *current_file);
int set_panel_formats (WPanel * p);

void try_to_select (WPanel * panel, const char *name);

void unmark_files (WPanel * panel);
void select_item (WPanel * panel);

void recalculate_panel_summary (WPanel * panel);
void file_mark (WPanel * panel, int idx, int val);
void do_file_mark (WPanel * panel, int idx, int val);

gboolean do_panel_cd (WPanel * panel, const vfs_path_t * new_dir_vpath, enum cd_enum cd_type);

gsize panel_get_num_of_sortable_fields (void);
char **panel_get_sortable_fields (gsize *);
const panel_field_t *panel_get_field_by_id (const char *);
const panel_field_t *panel_get_field_by_title (const char *);
const panel_field_t *panel_get_field_by_title_hotkey (const char *);
gsize panel_get_num_of_user_possible_fields (void);
char **panel_get_user_possible_fields (gsize *);
void panel_set_cwd (WPanel * panel, const vfs_path_t * vpath);
void panel_set_lwd (WPanel * panel, const vfs_path_t * vpath);

void panel_init (void);
void panel_deinit (void);
gboolean do_cd (const vfs_path_t * new_dir_vpath, enum cd_enum cd_type);

/* --------------------------------------------------------------------------------------------- */
/*** inline functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Panel creation.
 *
 * @param panel_name the name of the panel for setup retieving
 *
 * @return new instance of WPanel
 */

static inline WPanel *
panel_new (const char *panel_name)
{
    return panel_new_with_dir (panel_name, NULL);
}

/* --------------------------------------------------------------------------------------------- */

#endif /* MC__PANEL_H */

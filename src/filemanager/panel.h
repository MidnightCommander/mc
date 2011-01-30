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

#include "src/main.h"           /* cd_enum */

#include "dir.h"                /* dir_list */

/*** typedefs(not structures) and defined constants **********************************************/

#define selection(p) (&(p->dir.list[p->selected]))
#define DEFAULT_USER_FORMAT "half type name | size | perm"

#define LIST_TYPES 4

#define UP_KEEPSEL ((char *) -1)

/*** enums ***************************************************************************************/

enum list_types
{
    list_full,                  /* Name, size, perm/date */
    list_brief,                 /* Name */
    list_long,                  /* Like ls -l */
    list_user                   /* User defined */
};

typedef enum
{
    view_listing = 0,           /* Directory listing */
    view_info = 1,              /* Information panel */
    view_tree = 2,              /* Tree view */
    view_quick = 3,             /* Quick view */
    view_nothing = 4,           /* Undefined */
} panel_view_mode_t;

typedef enum
{
    frame_full,                 /* full screen frame */
    frame_half                  /* half screen frame */
} panel_display_t;

typedef enum
{
    UP_OPTIMIZE     = 0,
    UP_RELOAD       = 1,
    UP_ONLY_CURRENT = 2
} panel_update_flags_t;

/*** structures declarations (and typedefs of structures)*****************************************/

struct format_e;

typedef struct panel_field_struct
{
    const char *id;
    int min_size;
    int expands;
    align_crt_t default_just;
    const char *hotkey;
    const char *title_hotkey;
    gboolean is_user_choice;
    gboolean use_in_user_format;
    const char *(*string_fn) (file_entry *, int);
    sortfn *sort_routine;       /* used by mouse_sort_col() */
} panel_field_t;

typedef struct panel_sort_info_struct
{
    gboolean reverse;                /* Show listing in reverse? */
    gboolean case_sensitive;         /* Listing is case sensitive? */
    gboolean exec_first;             /* Show executable top in list? */
    const panel_field_t *sort_field;
} panel_sort_info_t;

typedef struct WPanel
{
    Widget widget;
    dir_list dir;               /* Directory contents */

    int list_type;              /* listing type (was view_type) */
    int active;                 /* If panel is currently selected */
    char cwd[MC_MAXPATHLEN];    /* Current Working Directory */
    char lwd[MC_MAXPATHLEN];    /* Last Working Directory */
    GList *dir_history;         /* directory history */
    char *hist_name;            /* directory history name for history file */
    int count;                  /* Number of files in dir structure */
    int marked;                 /* Count of marked files */
    int dirs_marked;            /* Count of marked directories */
    uintmax_t total;            /* Bytes in marked files */
    int top_file;               /* The file showed on the top of the panel */
    int selected;               /* Index to the selected file */
    int split;                  /* Split panel to allow two columns */
    int is_panelized;           /* Flag: special filelisting, can't reload */
    panel_display_t frame_size; /* half or full frame */
    char *filter;               /* File name filter */
    panel_sort_info_t sort_info;        /* Sort descriptor */

    int dirty;                  /* Should we redisplay the panel? */

    int user_mini_status;       /* Is user_status_format used */
    char *user_format;          /* User format */
    char *user_status_format[LIST_TYPES];       /* User format for status line */

    struct format_e *format;    /* Display format */
    struct format_e *status_format;     /* Mini status format */

    int format_modified;        /* If the format was changed this is set */

    char *panel_name;           /* The panel name */
    struct stat dir_stat;       /* Stat of current dir: used by execute () */

    int codepage;               /* panel codepage */

    gboolean searching;
    char search_buffer[MC_MAXFILENAMELEN];
    char prev_search_buffer[MC_MAXFILENAMELEN];
    char search_char[MB_LEN_MAX];       /*buffer for multibytes characters */
    int search_chpoint;         /*point after last characters in search_char */
} WPanel;

/*** global variables defined in .c file *********************************************************/

extern panel_field_t panel_fields[];

extern hook_t *select_file_hook;

/*** declarations of public functions ************************************************************/

WPanel *panel_new (const char *panel_name);
WPanel *panel_new_with_dir (const char *panel_name, const char *dr);
void panel_clean_dir (WPanel * panel);

void panel_reload (WPanel * panel);
void panel_set_sort_order (WPanel * panel, const panel_field_t * sort_order);
void panel_re_sort (WPanel * panel);
void panel_change_encoding (WPanel * panel);

void update_panels (panel_update_flags_t flags, const char *current_file);
int set_panel_formats (WPanel * p);

void try_to_select (WPanel * panel, const char *name);

void unmark_files (WPanel * panel);
void select_item (WPanel * panel);

void recalculate_panel_summary (WPanel * panel);
void file_mark (WPanel * panel, int idx, int val);
void do_file_mark (WPanel * panel, int idx, int val);

gboolean do_panel_cd (struct WPanel *panel, const char *new_dir, enum cd_enum cd_type);

void directory_history_add (struct WPanel *panel, const char *dir);

char *remove_encoding_from_path (const char *path);

gsize panel_get_num_of_sortable_fields (void);
const char **panel_get_sortable_fields (gsize *);
const panel_field_t *panel_get_field_by_id (const char *);
const panel_field_t *panel_get_field_by_title (const char *);
const panel_field_t *panel_get_field_by_title_hotkey (const char *);
gsize panel_get_num_of_user_possible_fields (void);
const char **panel_get_user_possible_fields (gsize *);

void panel_init (void);
void panel_deinit (void);

/*** inline functions ****************************************************************************/
#endif /* MC__PANEL_H */

#ifndef MC__DIFFVIEW_INTERNAL_H
#define MC__DIFFVIEW_INTERNAL_H

#include "lib/global.h"
#include "lib/event.h"
#include "lib/mcconfig.h"
#include "lib/search.h"
#include "lib/tty/color.h"
#include "lib/widget.h"

#include "execute.h"

/*** typedefs(not structures) and defined constants **********************************************/

typedef int (*DFUNC) (void *ctx, int ch, int line, off_t off, size_t sz, const char *str);
typedef int PAIR[2];

#define error_dialog(h, s) query_dialog(h, s, D_ERROR, 1, _("&Dismiss"))

#define ADD_CH '+'
#define DEL_CH '-'
#define CHG_CH '*'
#define EQU_CH ' '

/*** enums ***************************************************************************************/

typedef enum
{
    DATA_SRC_MEM = 0,
    DATA_SRC_TMP = 1,
    DATA_SRC_ORG = 2
} DSRC;

typedef enum
{
    DIFF_LEFT = 0,
    DIFF_RIGHT = 1,
    DIFF_COUNT = 2,
    DIFF_CURRENT = 3,
    DIFF_OTHER = 4
} diff_place_t;

typedef enum
{
    DIFF_NONE = 0,
    DIFF_ADD = 1,
    DIFF_DEL = 2,
    DIFF_CHG = 3
} DiffState;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    int off;
    int len;
} BRACKET[DIFF_COUNT];

typedef struct
{
    int ch;
    int line;
    union
    {
        off_t off;
        size_t len;
    } u;
    void *p;
} DIFFLN;

typedef struct
{
    FBUF *f;
    GArray *a;
    DSRC dsrc;
} PRINTER_CTX;

typedef struct WDiff
{
    Widget widget;

    const char *args;           /* Args passed to diff */
    const char *file[DIFF_COUNT];       /* filenames */
    char *label[DIFF_COUNT];
    FBUF *f[DIFF_COUNT];
    const char *backup_sufix;
    gboolean merged[DIFF_COUNT];
    GArray *a[DIFF_COUNT];
    GPtrArray *hdiff;
    int ndiff;                  /* number of hunks */
    DSRC dsrc;                  /* data source: memory or temporary file */

    gboolean view_quit;         /* Quit flag */

    int height;
    int half1;
    int half2;
    int width1;
    int width2;
    int bias;
    int new_frame;
    int skip_rows;
    int skip_cols;
    int display_symbols;
    int display_numbers;
    int show_cr;
    int tab_size;
    diff_place_t ord;
    int full;

#ifdef HAVE_CHARSET
    gboolean utf8;
    /* converter for translation of text */
    GIConv converter;
#endif                          /* HAVE_CHARSET */

    struct
    {
        int quality;
        gboolean strip_trailing_cr;
        gboolean ignore_tab_expansion;
        gboolean ignore_space_change;
        gboolean ignore_all_space;
        gboolean ignore_case;
    } opt;

    /* Search variables */
    struct
    {
        mc_search_t *handle;
        gchar *last_string;

        ssize_t last_found_line;
        ssize_t last_accessed_num_line;
    } search;
} WDiff;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void mc_diffviewer_init_events (GError ** error);
void mc_diffviewer_init_keymaps (GError ** error);

/* search.c */
gboolean mc_diffviewer_cmd_search (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_diffviewer_cmd_continue_search (event_info_t * event_info, gpointer data,
                                            GError ** error);


int mc_diffviewer_calc_nwidth (const GArray ** const a);
void mc_diffviewer_compute_split (WDiff * dview, int i);
int mc_diffviewer_get_line_numbers (const GArray * a, size_t pos, int *linenum, int *lineofs);
void mc_diffviewer_reread (WDiff * dview);
void mc_diffviewer_update (WDiff * dview);

void mc_diffviewer_destroy_hdiff (WDiff * dview);
void mc_diffviewer_deinit (WDiff * dview);

int mc_diffviewer_redo_diff (WDiff * dview);

gboolean mc_diffviewer_cmd_merge (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_diffviewer_cmd_run (event_info_t * event_info, gpointer data, GError ** error);

/*** inline functions ****************************************************************************/

#endif /* MC__DIFFVIEW_INTERNAL_H */

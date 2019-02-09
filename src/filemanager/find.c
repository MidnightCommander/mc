/*
   Find file command for the Midnight Commander

   Copyright (C) 1995-2019
   Free Software Foundation, Inc.

   Written  by:
   Miguel de Icaza, 1995
   Slava Zanko <slavazanko@gmail.com>, 2013
   Andrew Borodin <aborodin@vmail.ru>, 2013

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file find.c
 *  \brief Source: Find file command
 */

#include <config.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/key.h"
#include "lib/skin.h"
#include "lib/search.h"
#include "lib/mcconfig.h"
#include "lib/vfs/vfs.h"
#include "lib/strutil.h"
#include "lib/widget.h"
#include "lib/util.h"           /* canonicalize_pathname() */

#include "src/setup.h"          /* verbose */
#include "src/history.h"        /* MC_HISTORY_SHARED_SEARCH */

#include "dir.h"
#include "cmd.h"                /* view_file_at_line() */
#include "midnight.h"           /* current_panel */
#include "boxes.h"
#include "panelize.h"

#include "find.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define MAX_REFRESH_INTERVAL (G_USEC_PER_SEC / 20)      /* 50 ms */
#define MIN_REFRESH_FILE_SIZE (256 * 1024)      /* 256 KB */

/*** file scope type declarations ****************************************************************/

/* A couple of extra messages we need */
enum
{
    B_STOP = B_USER + 1,
    B_AGAIN,
    B_PANELIZE,
    B_TREE,
    B_VIEW
};

typedef enum
{
    FIND_CONT = 0,
    FIND_SUSPEND,
    FIND_ABORT
} FindProgressStatus;

/* find file options */
typedef struct
{
    /* file name options */
    gboolean file_case_sens;
    gboolean file_pattern;
    gboolean find_recurs;
    gboolean skip_hidden;
    gboolean file_all_charsets;

    /* file content options */
    gboolean content_case_sens;
    gboolean content_regexp;
    gboolean content_first_hit;
    gboolean content_whole_words;
    gboolean content_all_charsets;

    /* whether use ignore dirs or not */
    gboolean ignore_dirs_enable;
    /* list of directories to be ignored, separated by ':' */
    char *ignore_dirs;
} find_file_options_t;

typedef struct
{
    char *dir;
    gsize start;
    gsize end;
} find_match_location_t;

/*** file scope variables ************************************************************************/

/* button callbacks */
static int start_stop (WButton * button, int action);
static int find_do_view_file (WButton * button, int action);
static int find_do_edit_file (WButton * button, int action);

/* Parsed ignore dirs */
static char **find_ignore_dirs = NULL;

/* static variables to remember find parameters */
static WInput *in_start;        /* Start path */
static WInput *in_name;         /* Filename */
static WInput *in_with;         /* Text */
static WInput *in_ignore;
static WLabel *content_label;   /* 'Content:' label */
static WCheck *file_case_sens_cbox;     /* "case sensitive" checkbox */
static WCheck *file_pattern_cbox;       /* File name is glob or regexp */
static WCheck *recursively_cbox;
static WCheck *skip_hidden_cbox;
static WCheck *content_case_sens_cbox;  /* "case sensitive" checkbox */
static WCheck *content_regexp_cbox;     /* "find regular expression" checkbox */
static WCheck *content_first_hit_cbox;  /* "First hit" checkbox" */
static WCheck *content_whole_words_cbox;        /* "whole words" checkbox */
#ifdef HAVE_CHARSET
static WCheck *file_all_charsets_cbox;
static WCheck *content_all_charsets_cbox;
#endif
static WCheck *ignore_dirs_cbox;

static gboolean running = FALSE;        /* nice flag */
static char *find_pattern = NULL;       /* Pattern to search */
static char *content_pattern = NULL;    /* pattern to search inside files; if
                                           content_regexp_flag is true, it contains the
                                           regex pattern, else the search string. */
static gboolean content_is_empty = TRUE;        /* remember content field state; initially is empty */
static unsigned long matches;   /* Number of matches */
static gboolean is_start = FALSE;       /* Status of the start/stop toggle button */
static char *old_dir = NULL;

static struct timeval last_refresh;

/* Where did we stop */
static gboolean resuming;
static int last_line;
static int last_pos;
static off_t last_off;
static int last_i;

static size_t ignore_count = 0;

static WDialog *find_dlg;       /* The dialog */
static WLabel *status_label;    /* Finished, Searching etc. */
static WLabel *found_num_label; /* Number of found items */

/* This keeps track of the directory stack */
static GQueue dir_queue = G_QUEUE_INIT;

/* *INDENT-OFF* */
static struct
{
    int ret_cmd;
    button_flags_t flags;
    const char *text;
    int len;                    /* length including space and brackets */
    int x;
    Widget *button;
    bcback_fn callback;
} fbuts[] =
{
    { B_ENTER, DEFPUSH_BUTTON, N_("&Chdir"), 0, 0, NULL, NULL },
    { B_AGAIN, NORMAL_BUTTON, N_("&Again"), 0, 0, NULL, NULL },
    { B_STOP, NORMAL_BUTTON, N_("S&uspend"), 0, 0, NULL, start_stop },
    { B_STOP, NORMAL_BUTTON, N_("Con&tinue"), 0, 0, NULL, NULL },
    { B_CANCEL, NORMAL_BUTTON, N_("&Quit"), 0, 0, NULL, NULL },

    { B_PANELIZE, NORMAL_BUTTON, N_("Pane&lize"), 0, 0, NULL, NULL },
    { B_VIEW, NORMAL_BUTTON, N_("&View - F3"), 0, 0, NULL, find_do_view_file },
    { B_VIEW, NORMAL_BUTTON, N_("&Edit - F4"), 0, 0, NULL, find_do_edit_file }
};
/* *INDENT-ON* */

static const size_t fbuts_num = G_N_ELEMENTS (fbuts);
static const size_t quit_button = 4;    /* index of "Quit" button */

static WListbox *find_list;     /* Listbox with the file list */

static find_file_options_t options = {
    TRUE, TRUE, TRUE, FALSE, FALSE,
    TRUE, FALSE, FALSE, FALSE, FALSE,
    FALSE, NULL
};

static char *in_start_dir = INPUT_LAST_TEXT;

static mc_search_t *search_file_handle = NULL;
static mc_search_t *search_content_handle = NULL;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* don't use max macro to avoid double str_term_width1() call in widget length caclulation */
#undef max

static int
max (int a, int b)
{
    return (a > b ? a : b);
}

/* --------------------------------------------------------------------------------------------- */

static void
parse_ignore_dirs (const char *ignore_dirs)
{
    size_t r = 0, w = 0;        /* read and write iterators */

    if (!options.ignore_dirs_enable || ignore_dirs == NULL || ignore_dirs[0] == '\0')
        return;

    find_ignore_dirs = g_strsplit (ignore_dirs, ":", -1);

    /* Values like '/foo::/bar: produce holes in list.
     * Find and remove them */
    for (; find_ignore_dirs[r] != NULL; r++)
    {
        if (find_ignore_dirs[r][0] == '\0')
        {
            /* empty entry -- skip it */
            MC_PTR_FREE (find_ignore_dirs[r]);
            continue;
        }

        if (r != w)
        {
            /* copy entry to the previous free array cell */
            find_ignore_dirs[w] = find_ignore_dirs[r];
            find_ignore_dirs[r] = NULL;
        }

        canonicalize_pathname (find_ignore_dirs[w]);
        if (find_ignore_dirs[w][0] != '\0')
            w++;
        else
            MC_PTR_FREE (find_ignore_dirs[w]);
    }

    if (find_ignore_dirs[0] == NULL)
    {
        g_strfreev (find_ignore_dirs);
        find_ignore_dirs = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
find_load_options (void)
{
    static gboolean loaded = FALSE;

    if (loaded)
        return;

    loaded = TRUE;

    options.file_case_sens =
        mc_config_get_bool (mc_global.main_config, "FindFile", "file_case_sens", TRUE);
    options.file_pattern =
        mc_config_get_bool (mc_global.main_config, "FindFile", "file_shell_pattern", TRUE);
    options.find_recurs =
        mc_config_get_bool (mc_global.main_config, "FindFile", "file_find_recurs", TRUE);
    options.skip_hidden =
        mc_config_get_bool (mc_global.main_config, "FindFile", "file_skip_hidden", FALSE);
    options.file_all_charsets =
        mc_config_get_bool (mc_global.main_config, "FindFile", "file_all_charsets", FALSE);
    options.content_case_sens =
        mc_config_get_bool (mc_global.main_config, "FindFile", "content_case_sens", TRUE);
    options.content_regexp =
        mc_config_get_bool (mc_global.main_config, "FindFile", "content_regexp", FALSE);
    options.content_first_hit =
        mc_config_get_bool (mc_global.main_config, "FindFile", "content_first_hit", FALSE);
    options.content_whole_words =
        mc_config_get_bool (mc_global.main_config, "FindFile", "content_whole_words", FALSE);
    options.content_all_charsets =
        mc_config_get_bool (mc_global.main_config, "FindFile", "content_all_charsets", FALSE);
    options.ignore_dirs_enable =
        mc_config_get_bool (mc_global.main_config, "FindFile", "ignore_dirs_enable", TRUE);
    options.ignore_dirs =
        mc_config_get_string (mc_global.main_config, "FindFile", "ignore_dirs", "");

    if (options.ignore_dirs[0] == '\0')
        MC_PTR_FREE (options.ignore_dirs);
}

/* --------------------------------------------------------------------------------------------- */

static void
find_save_options (void)
{
    mc_config_set_bool (mc_global.main_config, "FindFile", "file_case_sens",
                        options.file_case_sens);
    mc_config_set_bool (mc_global.main_config, "FindFile", "file_shell_pattern",
                        options.file_pattern);
    mc_config_set_bool (mc_global.main_config, "FindFile", "file_find_recurs", options.find_recurs);
    mc_config_set_bool (mc_global.main_config, "FindFile", "file_skip_hidden", options.skip_hidden);
    mc_config_set_bool (mc_global.main_config, "FindFile", "file_all_charsets",
                        options.file_all_charsets);
    mc_config_set_bool (mc_global.main_config, "FindFile", "content_case_sens",
                        options.content_case_sens);
    mc_config_set_bool (mc_global.main_config, "FindFile", "content_regexp",
                        options.content_regexp);
    mc_config_set_bool (mc_global.main_config, "FindFile", "content_first_hit",
                        options.content_first_hit);
    mc_config_set_bool (mc_global.main_config, "FindFile", "content_whole_words",
                        options.content_whole_words);
    mc_config_set_bool (mc_global.main_config, "FindFile", "content_all_charsets",
                        options.content_all_charsets);
    mc_config_set_bool (mc_global.main_config, "FindFile", "ignore_dirs_enable",
                        options.ignore_dirs_enable);
    mc_config_set_string (mc_global.main_config, "FindFile", "ignore_dirs", options.ignore_dirs);
}

/* --------------------------------------------------------------------------------------------- */

static inline char *
add_to_list (const char *text, void *data)
{
    return listbox_add_item (find_list, LISTBOX_APPEND_AT_END, 0, text, data, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static inline void
stop_idle (void *data)
{
    widget_idle (WIDGET (data), FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static inline void
status_update (const char *text)
{
    label_set_text (status_label, text);
}

/* --------------------------------------------------------------------------------------------- */

static void
found_num_update (void)
{
    char buffer[BUF_TINY];

    g_snprintf (buffer, sizeof (buffer), _("Found: %lu"), matches);
    label_set_text (found_num_label, buffer);
}

/* --------------------------------------------------------------------------------------------- */

static void
get_list_info (char **file, char **dir, gsize * start, gsize * end)
{
    find_match_location_t *location;

    listbox_get_current (find_list, file, (void **) &location);
    if (location != NULL)
    {
        if (dir != NULL)
            *dir = location->dir;
        if (start != NULL)
            *start = location->start;
        if (end != NULL)
            *end = location->end;
    }
    else
    {
        if (dir != NULL)
            *dir = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */
/** check regular expression */

static gboolean
find_check_regexp (const char *r)
{
    mc_search_t *search;
    gboolean regexp_ok = FALSE;

    search = mc_search_new (r, NULL);

    if (search != NULL)
    {
        search->search_type = MC_SEARCH_T_REGEX;
        regexp_ok = mc_search_prepare (search);
        mc_search_free (search);
    }

    return regexp_ok;
}

/* --------------------------------------------------------------------------------------------- */

static void
find_toggle_enable_ignore_dirs (void)
{
    widget_disable (WIDGET (in_ignore), !ignore_dirs_cbox->state);
}

/* --------------------------------------------------------------------------------------------- */

static void
find_toggle_enable_params (void)
{
    gboolean disable = input_is_empty (in_name);

    widget_disable (WIDGET (file_pattern_cbox), disable);
    widget_disable (WIDGET (file_case_sens_cbox), disable);
#ifdef HAVE_CHARSET
    widget_disable (WIDGET (file_all_charsets_cbox), disable);
#endif
}

/* --------------------------------------------------------------------------------------------- */

static void
find_toggle_enable_content (void)
{
    widget_disable (WIDGET (content_regexp_cbox), content_is_empty);
    widget_disable (WIDGET (content_case_sens_cbox), content_is_empty);
#ifdef HAVE_CHARSET
    widget_disable (WIDGET (content_all_charsets_cbox), content_is_empty);
#endif
    widget_disable (WIDGET (content_whole_words_cbox), content_is_empty);
    widget_disable (WIDGET (content_first_hit_cbox), content_is_empty);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for the parameter dialog.
 * Validate regex, prevent closing the dialog if it's invalid.
 */

static cb_ret_t
find_parm_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    /* FIXME: HACK: use first draw of dialog to resolve widget state dependencies.
     * Use this time moment to check input field content. We can't do that in MSG_INIT
     * because history is not loaded yet.
     * Probably, we want new MSG_ACTIVATE message as complement to MSG_VALIDATE one. Or
     * we could name it MSG_POST_INIT.
     *
     * In one or two other places we use MSG_IDLE instead of MSG_DRAW for a similar
     * purpose. We should remember to fix those places too when we introduce the new
     * message.
     */
    static gboolean first_draw = TRUE;

    WDialog *h = DIALOG (w);

    switch (msg)
    {
    case MSG_INIT:
        first_draw = TRUE;
        return MSG_HANDLED;

    case MSG_NOTIFY:
        if (sender == WIDGET (ignore_dirs_cbox))
        {
            find_toggle_enable_ignore_dirs ();
            return MSG_HANDLED;
        }

        return MSG_NOT_HANDLED;

    case MSG_VALIDATE:
        if (h->ret_value != B_ENTER)
            return MSG_HANDLED;

        /* check filename regexp */
        if (!file_pattern_cbox->state && !input_is_empty (in_name)
            && !find_check_regexp (in_name->buffer))
        {
            /* Don't stop the dialog */
            widget_set_state (WIDGET (h), WST_ACTIVE, TRUE);
            message (D_ERROR, MSG_ERROR, _("Malformed regular expression"));
            widget_select (WIDGET (in_name));
            return MSG_HANDLED;
        }

        /* check content regexp */
        if (content_regexp_cbox->state && !content_is_empty && !find_check_regexp (in_with->buffer))
        {
            /* Don't stop the dialog */
            widget_set_state (WIDGET (h), WST_ACTIVE, TRUE);
            message (D_ERROR, MSG_ERROR, _("Malformed regular expression"));
            widget_select (WIDGET (in_with));
            return MSG_HANDLED;
        }

        return MSG_HANDLED;

    case MSG_POST_KEY:
        if (h->current->data == in_name)
            find_toggle_enable_params ();
        else if (h->current->data == in_with)
        {
            content_is_empty = input_is_empty (in_with);
            find_toggle_enable_content ();
        }
        return MSG_HANDLED;

    case MSG_DRAW:
        if (first_draw)
        {
            find_toggle_enable_ignore_dirs ();
            find_toggle_enable_params ();
            find_toggle_enable_content ();
        }

        first_draw = FALSE;
        MC_FALLTHROUGH;         /* to call MSG_DRAW default handler */

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * find_parameters: gets information from the user
 *
 * If the return value is TRUE, then the following holds:
 *
 * start_dir, ignore_dirs, pattern and content contain the information provided by the user.
 * They are newly allocated strings and must be freed when uneeded.
 *
 * start_dir_len is -1 when user entered an absolute path, otherwise it is a length
 * of start_dir (which is absolute). It is used to get a relative pats of find results.
 */

static gboolean
find_parameters (char **start_dir, ssize_t * start_dir_len,
                 char **ignore_dirs, char **pattern, char **content)
{
    /* Size of the find parameters window */
#ifdef HAVE_CHARSET
    const int lines = 18;
#else
    const int lines = 17;
#endif
    int cols = 68;

    gboolean return_value;

    /* file name */
    const char *file_name_label = N_("File name:");
    const char *file_recurs_label = N_("&Find recursively");
    const char *file_pattern_label = N_("&Using shell patterns");
#ifdef HAVE_CHARSET
    const char *file_all_charsets_label = N_("&All charsets");
#endif
    const char *file_case_label = N_("Cas&e sensitive");
    const char *file_skip_hidden_label = N_("S&kip hidden");

    /* file content */
    const char *content_content_label = N_("Content:");
    const char *content_use_label = N_("Sea&rch for content");
    const char *content_regexp_label = N_("Re&gular expression");
    const char *content_case_label = N_("Case sens&itive");
#ifdef HAVE_CHARSET
    const char *content_all_charsets_label = N_("A&ll charsets");
#endif
    const char *content_whole_words_label = N_("&Whole words");
    const char *content_first_hit_label = N_("Fir&st hit");

    const char *buts[] = { N_("&Tree"), N_("&OK"), N_("&Cancel") };

    /* button lengths */
    int b0, b1, b2, b12;
    int y1, y2, x1, x2;
    /* column width */
    int cw;

#ifdef ENABLE_NLS
    {
        size_t i;

        file_name_label = _(file_name_label);
        file_recurs_label = _(file_recurs_label);
        file_pattern_label = _(file_pattern_label);
#ifdef HAVE_CHARSET
        file_all_charsets_label = _(file_all_charsets_label);
#endif
        file_case_label = _(file_case_label);
        file_skip_hidden_label = _(file_skip_hidden_label);

        /* file content */
        content_content_label = _(content_content_label);
        content_use_label = _(content_use_label);
        content_regexp_label = _(content_regexp_label);
        content_case_label = _(content_case_label);
#ifdef HAVE_CHARSET
        content_all_charsets_label = _(content_all_charsets_label);
#endif
        content_whole_words_label = _(content_whole_words_label);
        content_first_hit_label = _(content_first_hit_label);

        for (i = 0; i < G_N_ELEMENTS (buts); i++)
            buts[i] = _(buts[i]);
    }
#endif /* ENABLE_NLS */

    /* caclulate dialog width */

    /* widget widths */
    cw = str_term_width1 (file_name_label);
    cw = max (cw, str_term_width1 (file_recurs_label) + 4);
    cw = max (cw, str_term_width1 (file_pattern_label) + 4);
#ifdef HAVE_CHARSET
    cw = max (cw, str_term_width1 (file_all_charsets_label) + 4);
#endif
    cw = max (cw, str_term_width1 (file_case_label) + 4);
    cw = max (cw, str_term_width1 (file_skip_hidden_label) + 4);

    cw = max (cw, str_term_width1 (content_content_label) + 4);
    cw = max (cw, str_term_width1 (content_use_label) + 4);
    cw = max (cw, str_term_width1 (content_regexp_label) + 4);
    cw = max (cw, str_term_width1 (content_case_label) + 4);
#ifdef HAVE_CHARSET
    cw = max (cw, str_term_width1 (content_all_charsets_label) + 4);
#endif
    cw = max (cw, str_term_width1 (content_whole_words_label) + 4);
    cw = max (cw, str_term_width1 (content_first_hit_label) + 4);

    /* button width */
    b0 = str_term_width1 (buts[0]) + 3;
    b1 = str_term_width1 (buts[1]) + 5; /* default button */
    b2 = str_term_width1 (buts[2]) + 3;
    b12 = b1 + b2 + 1;

    cols = max (cols, max (b12, cw * 2 + 1) + 6);

    find_load_options ();

    if (in_start_dir == NULL)
        in_start_dir = g_strdup (".");

    find_dlg =
        dlg_create (TRUE, 0, 0, lines, cols, WPOS_CENTER, FALSE, dialog_colors, find_parm_callback,
                    NULL, "[Find File]", _("Find File"));

    x1 = 3;
    x2 = cols / 2 + 1;
    cw = (cols - 7) / 2;
    y1 = 2;

    add_widget (find_dlg, label_new (y1++, x1, _("Start at:")));
    in_start =
        input_new (y1, x1, input_colors, cols - b0 - 7, in_start_dir, "start",
                   INPUT_COMPLETE_CD | INPUT_COMPLETE_FILENAMES);
    add_widget (find_dlg, in_start);

    add_widget (find_dlg, button_new (y1++, cols - b0 - 3, B_TREE, NORMAL_BUTTON, buts[0], NULL));

    ignore_dirs_cbox =
        check_new (y1++, x1, options.ignore_dirs_enable, _("Ena&ble ignore directories:"));
    add_widget (find_dlg, ignore_dirs_cbox);

    in_ignore =
        input_new (y1++, x1, input_colors, cols - 6,
                   options.ignore_dirs != NULL ? options.ignore_dirs : "", "ignoredirs",
                   INPUT_COMPLETE_CD | INPUT_COMPLETE_FILENAMES);
    add_widget (find_dlg, in_ignore);

    add_widget (find_dlg, hline_new (y1++, -1, -1));

    y2 = y1;

    /* Start 1st column */
    add_widget (find_dlg, label_new (y1++, x1, file_name_label));
    in_name =
        input_new (y1++, x1, input_colors, cw, INPUT_LAST_TEXT, "name",
                   INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_CD);
    add_widget (find_dlg, in_name);

    /* Start 2nd column */
    content_label = label_new (y2++, x2, content_content_label);
    add_widget (find_dlg, content_label);
    in_with =
        input_new (y2++, x2, input_colors, cw, content_is_empty ? "" : INPUT_LAST_TEXT,
                   MC_HISTORY_SHARED_SEARCH, INPUT_COMPLETE_NONE);
    in_with->label = content_label;
    add_widget (find_dlg, in_with);

    /* Continue 1st column */
    recursively_cbox = check_new (y1++, x1, options.find_recurs, file_recurs_label);
    add_widget (find_dlg, recursively_cbox);

    file_pattern_cbox = check_new (y1++, x1, options.file_pattern, file_pattern_label);
    add_widget (find_dlg, file_pattern_cbox);

    file_case_sens_cbox = check_new (y1++, x1, options.file_case_sens, file_case_label);
    add_widget (find_dlg, file_case_sens_cbox);

#ifdef HAVE_CHARSET
    file_all_charsets_cbox =
        check_new (y1++, x1, options.file_all_charsets, file_all_charsets_label);
    add_widget (find_dlg, file_all_charsets_cbox);
#endif

    skip_hidden_cbox = check_new (y1++, x1, options.skip_hidden, file_skip_hidden_label);
    add_widget (find_dlg, skip_hidden_cbox);

    /* Continue 2nd column */
    content_whole_words_cbox =
        check_new (y2++, x2, options.content_whole_words, content_whole_words_label);
    add_widget (find_dlg, content_whole_words_cbox);

    content_regexp_cbox = check_new (y2++, x2, options.content_regexp, content_regexp_label);
    add_widget (find_dlg, content_regexp_cbox);

    content_case_sens_cbox = check_new (y2++, x2, options.content_case_sens, content_case_label);
    add_widget (find_dlg, content_case_sens_cbox);

#ifdef HAVE_CHARSET
    content_all_charsets_cbox =
        check_new (y2++, x2, options.content_all_charsets, content_all_charsets_label);
    add_widget (find_dlg, content_all_charsets_cbox);
#endif

    content_first_hit_cbox =
        check_new (y2++, x2, options.content_first_hit, content_first_hit_label);
    add_widget (find_dlg, content_first_hit_cbox);

    /* buttons */
    y1 = max (y1, y2);
    x1 = (cols - b12) / 2;
    add_widget (find_dlg, hline_new (y1++, -1, -1));
    add_widget (find_dlg, button_new (y1, x1, B_ENTER, DEFPUSH_BUTTON, buts[1], NULL));
    add_widget (find_dlg, button_new (y1, x1 + b1 + 1, B_CANCEL, NORMAL_BUTTON, buts[2], NULL));

  find_par_start:
    widget_select (WIDGET (in_name));

    switch (dlg_run (find_dlg))
    {
    case B_CANCEL:
        return_value = FALSE;
        break;

    case B_TREE:
        {
            char *temp_dir;

            temp_dir = in_start->buffer;
            if (*temp_dir == '\0' || DIR_IS_DOT (temp_dir))
                temp_dir = g_strdup (vfs_path_as_str (current_panel->cwd_vpath));
            else
                temp_dir = g_strdup (temp_dir);

            if (in_start_dir != INPUT_LAST_TEXT)
                g_free (in_start_dir);
            in_start_dir = tree_box (temp_dir);
            if (in_start_dir == NULL)
                in_start_dir = temp_dir;
            else
                g_free (temp_dir);

            input_assign_text (in_start, in_start_dir);

            /* Warning: Dreadful goto */
            goto find_par_start;
        }

    default:
        {
            char *s;

#ifdef HAVE_CHARSET
            options.file_all_charsets = file_all_charsets_cbox->state;
            options.content_all_charsets = content_all_charsets_cbox->state;
#endif
            options.content_case_sens = content_case_sens_cbox->state;
            options.content_regexp = content_regexp_cbox->state;
            options.content_first_hit = content_first_hit_cbox->state;
            options.content_whole_words = content_whole_words_cbox->state;
            options.find_recurs = recursively_cbox->state;
            options.file_pattern = file_pattern_cbox->state;
            options.file_case_sens = file_case_sens_cbox->state;
            options.skip_hidden = skip_hidden_cbox->state;
            options.ignore_dirs_enable = ignore_dirs_cbox->state;
            g_free (options.ignore_dirs);
            options.ignore_dirs = g_strdup (in_ignore->buffer);

            *content = !input_is_empty (in_with) ? g_strdup (in_with->buffer) : NULL;
            if (!input_is_empty (in_name))
                *pattern = g_strdup (in_name->buffer);
            else
                *pattern = g_strdup (options.file_pattern ? "*" : ".*");
            *start_dir = !input_is_empty (in_start) ? in_start->buffer : (char *) ".";
            if (in_start_dir != INPUT_LAST_TEXT)
                g_free (in_start_dir);
            in_start_dir = g_strdup (*start_dir);

            s = tilde_expand (*start_dir);
            canonicalize_pathname (s);

            if (DIR_IS_DOT (s))
            {
                *start_dir = g_strdup (vfs_path_as_str (current_panel->cwd_vpath));
                /* FIXME: is current_panel->cwd_vpath canonicalized? */
                /* relative paths will be used in panelization */
                *start_dir_len = (ssize_t) strlen (*start_dir);
                g_free (s);
            }
            else if (g_path_is_absolute (s))
            {
                *start_dir = s;
                *start_dir_len = -1;
            }
            else
            {
                /* relative paths will be used in panelization */
                *start_dir =
                    mc_build_filename (vfs_path_as_str (current_panel->cwd_vpath), s,
                                       (char *) NULL);
                *start_dir_len = (ssize_t) strlen (vfs_path_as_str (current_panel->cwd_vpath));
                g_free (s);
            }

            if (!options.ignore_dirs_enable || input_is_empty (in_ignore)
                || DIR_IS_DOT (in_ignore->buffer))
                *ignore_dirs = NULL;
            else
                *ignore_dirs = g_strdup (in_ignore->buffer);

            find_save_options ();

            return_value = TRUE;
        }
    }

    dlg_destroy (find_dlg);

    return return_value;
}

/* --------------------------------------------------------------------------------------------- */

static inline void
push_directory (vfs_path_t * dir)
{
    g_queue_push_head (&dir_queue, (void *) dir);
}

/* --------------------------------------------------------------------------------------------- */

static inline vfs_path_t *
pop_directory (void)
{
    return (vfs_path_t *) g_queue_pop_head (&dir_queue);
}

/* --------------------------------------------------------------------------------------------- */
/** Remove all the items from the stack */

static void
clear_stack (void)
{
    g_queue_clear_full (&dir_queue, (GDestroyNotify) vfs_path_free);
}

/* --------------------------------------------------------------------------------------------- */

static void
insert_file (const char *dir, const char *file, gsize start, gsize end)
{
    char *tmp_name = NULL;
    static char *dirname = NULL;
    find_match_location_t *location;

    while (IS_PATH_SEP (dir[0]) && IS_PATH_SEP (dir[1]))
        dir++;

    if (old_dir)
    {
        if (strcmp (old_dir, dir))
        {
            g_free (old_dir);
            old_dir = g_strdup (dir);
            dirname = add_to_list (dir, NULL);
        }
    }
    else
    {
        old_dir = g_strdup (dir);
        dirname = add_to_list (dir, NULL);
    }

    tmp_name = g_strdup_printf ("    %s", file);
    location = g_malloc (sizeof (*location));
    location->dir = dirname;
    location->start = start;
    location->end = end;
    add_to_list (tmp_name, location);
    g_free (tmp_name);
}

/* --------------------------------------------------------------------------------------------- */

static void
find_add_match (const char *dir, const char *file, gsize start, gsize end)
{
    insert_file (dir, file, start, end);

    /* Don't scroll */
    if (matches == 0)
        listbox_select_first (find_list);
    widget_redraw (WIDGET (find_list));

    matches++;
    found_num_update ();
}

/* --------------------------------------------------------------------------------------------- */

static FindProgressStatus
check_find_events (WDialog * h)
{
    Gpm_Event event;
    int c;

    event.x = -1;
    c = tty_get_event (&event, h->mouse_status == MOU_REPEAT, FALSE);
    if (c != EV_NONE)
    {
        dlg_process_event (h, c, &event);
        if (h->ret_value == B_ENTER
            || h->ret_value == B_CANCEL || h->ret_value == B_AGAIN || h->ret_value == B_PANELIZE)
        {
            /* dialog terminated */
            return FIND_ABORT;
        }
        if (!widget_get_state (WIDGET (h), WST_IDLE))
        {
            /* searching suspended */
            return FIND_SUSPEND;
        }
    }

    return FIND_CONT;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * search_content:
 *
 * Search the content_pattern string in the DIRECTORY/FILE.
 * It will add the found entries to the find listbox.
 *
 * returns FALSE if do_search should look for another file
 *         TRUE if do_search should exit and proceed to the event handler
 */

static gboolean
search_content (WDialog * h, const char *directory, const char *filename)
{
    struct stat s;
    char buffer[BUF_4K];        /* raw input buffer */
    int file_fd;
    gboolean ret_val = FALSE;
    vfs_path_t *vpath;
    struct timeval tv;
    time_t seconds;
    suseconds_t useconds;
    gboolean status_updated = FALSE;

    vpath = vfs_path_build_filename (directory, filename, (char *) NULL);

    if (mc_stat (vpath, &s) != 0 || !S_ISREG (s.st_mode))
    {
        vfs_path_free (vpath);
        return FALSE;
    }

    file_fd = mc_open (vpath, O_RDONLY);
    vfs_path_free (vpath);

    if (file_fd == -1)
        return FALSE;

    /* get time elapsed from last refresh */
    if (gettimeofday (&tv, NULL) == -1)
    {
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        last_refresh = tv;
    }
    seconds = tv.tv_sec - last_refresh.tv_sec;
    useconds = tv.tv_usec - last_refresh.tv_usec;
    if (useconds < 0)
    {
        seconds--;
        useconds += G_USEC_PER_SEC;
    }

    if (s.st_size >= MIN_REFRESH_FILE_SIZE || seconds > 0 || useconds > MAX_REFRESH_INTERVAL)
    {
        g_snprintf (buffer, sizeof (buffer), _("Grepping in %s"), filename);
        status_update (str_trunc (buffer, WIDGET (h)->cols - 8));
        mc_refresh ();
        last_refresh = tv;
        status_updated = TRUE;
    }

    tty_enable_interrupt_key ();
    tty_got_interrupt ();

    {
        int line = 1;
        int pos = 0;
        int n_read = 0;
        off_t off = 0;          /* file_fd's offset corresponding to strbuf[0] */
        gboolean found = FALSE;
        gsize found_len;
        gsize found_start;
        char result[BUF_MEDIUM];
        char *strbuf = NULL;    /* buffer for fetched string */
        int strbuf_size = 0;
        int i = -1;             /* compensate for a newline we'll add when we first enter the loop */

        if (resuming)
        {
            /* We've been previously suspended, start from the previous position */
            resuming = FALSE;
            line = last_line;
            pos = last_pos;
            off = last_off;
            i = last_i;
        }

        while (!ret_val)
        {
            char ch = '\0';
            off += i + 1;       /* the previous line, plus a newline character */
            i = 0;

            /* read to buffer and get line from there */
            while (TRUE)
            {
                if (pos >= n_read)
                {
                    pos = 0;
                    n_read = mc_read (file_fd, buffer, sizeof (buffer));
                    if (n_read <= 0)
                        break;
                }

                ch = buffer[pos++];
                if (ch == '\0')
                {
                    /* skip possible leading zero(s) */
                    if (i == 0)
                    {
                        off++;
                        continue;
                    }
                    break;
                }

                if (i >= strbuf_size - 1)
                {
                    strbuf_size += 128;
                    strbuf = g_realloc (strbuf, strbuf_size);
                }

                /* Strip newline */
                if (ch == '\n')
                    break;

                strbuf[i++] = ch;
            }

            if (i == 0)
            {
                if (ch == '\0')
                    break;

                /* if (ch == '\n'): do not search in empty strings */
                goto skip_search;
            }

            strbuf[i] = '\0';

            if (!found          /* Search in binary line once */
                && mc_search_run (search_content_handle, (const void *) strbuf, 0, i, &found_len))
            {
                if (!status_updated)
                {
                    /* if we add results for a file, we have to ensure that
                       name of this file is shown in status bar */
                    g_snprintf (result, sizeof (result), _("Grepping in %s"), filename);
                    status_update (str_trunc (result, WIDGET (h)->cols - 8));
                    mc_refresh ();
                    last_refresh = tv;
                    status_updated = TRUE;
                }

                g_snprintf (result, sizeof (result), "%d:%s", line, filename);
                found_start = off + search_content_handle->normal_offset + 1;   /* off by one: ticket 3280 */
                find_add_match (directory, result, found_start, found_start + found_len);
                found = TRUE;
            }

            if (found && options.content_first_hit)
                break;

            if (ch == '\n')
            {
              skip_search:
                found = FALSE;
                line++;
            }

            if ((line & 0xff) == 0)
            {
                FindProgressStatus res;
                res = check_find_events (h);
                switch (res)
                {
                case FIND_ABORT:
                    stop_idle (h);
                    ret_val = TRUE;
                    break;
                case FIND_SUSPEND:
                    resuming = TRUE;
                    last_line = line;
                    last_pos = pos;
                    last_off = off;
                    last_i = i;
                    ret_val = TRUE;
                    break;
                default:
                    break;
                }
            }
        }

        g_free (strbuf);
    }

    tty_disable_interrupt_key ();
    mc_close (file_fd);
    return ret_val;
}

/* --------------------------------------------------------------------------------------------- */

/**
  If dir is absolute, this means we're within dir and searching file here.
  If dir is relative, this means we're going to add dir to the directory stack.
**/
static gboolean
find_ignore_dir_search (const char *dir)
{
    if (find_ignore_dirs != NULL)
    {
        const size_t dlen = strlen (dir);
        const unsigned char dabs = g_path_is_absolute (dir) ? 1 : 0;

        char **ignore_dir;

        for (ignore_dir = find_ignore_dirs; *ignore_dir != NULL; ignore_dir++)
        {
            const size_t ilen = strlen (*ignore_dir);
            const unsigned char iabs = g_path_is_absolute (*ignore_dir) ? 2 : 0;

            /* ignore dir is too long -- skip it */
            if (dlen < ilen)
                continue;

            /* handle absolute and relative paths */
            switch (iabs | dabs)
            {
            case 0:            /* both paths are relative */
            case 3:            /* both paths are abolute */
                /* if ignore dir is not a path  of dir -- skip it */
                if (strncmp (dir, *ignore_dir, ilen) == 0)
                {
                    /* be sure that ignore dir is not a part of dir like:
                       ignore dir is "h", dir is "home" */
                    if (dir[ilen] == '\0' || IS_PATH_SEP (dir[ilen]))
                        return TRUE;
                }
                break;
            case 1:            /* dir is absolute, ignore_dir is relative */
                {
                    char *d;

                    d = strstr (dir, *ignore_dir);
                    if (d != NULL && IS_PATH_SEP (d[-1])
                        && (d[ilen] == '\0' || IS_PATH_SEP (d[ilen])))
                        return TRUE;
                }
                break;
            case 2:            /* dir is relative, ignore_dir is absolute */
                /* FIXME: skip this case */
                break;
            default:           /* this cannot occurs */
                return FALSE;
            }
        }
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
find_rotate_dash (const WDialog * h, gboolean show)
{
    static size_t pos = 0;
    static const char rotating_dash[4] = "|/-\\";
    const Widget *w = CONST_WIDGET (h);

    if (!verbose)
        return;

    tty_setcolor (h->color[DLG_COLOR_NORMAL]);
    widget_move (h, w->lines - 7, w->cols - 4);
    tty_print_char (show ? rotating_dash[pos] : ' ');
    pos = (pos + 1) % sizeof (rotating_dash);
    mc_refresh ();
}

/* --------------------------------------------------------------------------------------------- */

static int
do_search (WDialog * h)
{
    static struct dirent *dp = NULL;
    static DIR *dirp = NULL;
    static char *directory = NULL;
    struct stat tmp_stat;
    gsize bytes_found;
    unsigned short count;

    if (h == NULL)
    {                           /* someone forces me to close dirp */
        if (dirp != NULL)
        {
            mc_closedir (dirp);
            dirp = NULL;
        }
        MC_PTR_FREE (directory);
        dp = NULL;
        return 1;
    }

    for (count = 0; count < 32; count++)
    {
        while (dp == NULL)
        {
            if (dirp != NULL)
            {
                mc_closedir (dirp);
                dirp = NULL;
            }

            while (dirp == NULL)
            {
                vfs_path_t *tmp_vpath = NULL;

                tty_setcolor (REVERSE_COLOR);

                while (TRUE)
                {
                    tmp_vpath = pop_directory ();
                    if (tmp_vpath == NULL)
                    {
                        running = FALSE;
                        if (ignore_count == 0)
                            status_update (_("Finished"));
                        else
                        {
                            char msg[BUF_SMALL];

                            g_snprintf (msg, sizeof (msg),
                                        ngettext ("Finished (ignored %zu directory)",
                                                  "Finished (ignored %zu directories)",
                                                  ignore_count), ignore_count);
                            status_update (msg);
                        }
                        find_rotate_dash (h, FALSE);
                        stop_idle (h);
                        return 0;
                    }

                    /* handle absolute ignore dirs here */
                    {
                        gboolean ok;

                        ok = find_ignore_dir_search (vfs_path_as_str (tmp_vpath));
                        if (!ok)
                            break;
                    }

                    vfs_path_free (tmp_vpath);
                    ignore_count++;
                }

                g_free (directory);
                directory = g_strdup (vfs_path_as_str (tmp_vpath));

                if (verbose)
                {
                    char buffer[BUF_MEDIUM];

                    g_snprintf (buffer, sizeof (buffer), _("Searching %s"), directory);
                    status_update (str_trunc (directory, WIDGET (h)->cols - 8));
                }

                dirp = mc_opendir (tmp_vpath);
                vfs_path_free (tmp_vpath);
            }                   /* while (!dirp) */

            /* skip invalid filenames */
            while ((dp = mc_readdir (dirp)) != NULL && !str_is_valid_string (dp->d_name))
                ;
        }                       /* while (!dp) */

        if (DIR_IS_DOT (dp->d_name) || DIR_IS_DOTDOT (dp->d_name))
        {
            /* skip invalid filenames */
            while ((dp = mc_readdir (dirp)) != NULL && !str_is_valid_string (dp->d_name))
                ;

            return 1;
        }

        if (!(options.skip_hidden && (dp->d_name[0] == '.')))
        {
            gboolean search_ok;

            if (options.find_recurs && (directory != NULL))
            {                   /* Can directory be NULL ? */
                /* handle relative ignore dirs here */
                if (options.ignore_dirs_enable && find_ignore_dir_search (dp->d_name))
                    ignore_count++;
                else
                {
                    vfs_path_t *tmp_vpath;

                    tmp_vpath = vfs_path_build_filename (directory, dp->d_name, (char *) NULL);

                    if (mc_lstat (tmp_vpath, &tmp_stat) == 0 && S_ISDIR (tmp_stat.st_mode))
                        push_directory (tmp_vpath);
                    else
                        vfs_path_free (tmp_vpath);
                }
            }

            search_ok = mc_search_run (search_file_handle, dp->d_name,
                                       0, strlen (dp->d_name), &bytes_found);

            if (search_ok)
            {
                if (content_pattern == NULL)
                    find_add_match (directory, dp->d_name, 0, 0);
                else if (search_content (h, directory, dp->d_name))
                    return 1;
            }
        }

        /* skip invalid filenames */
        while ((dp = mc_readdir (dirp)) != NULL && !str_is_valid_string (dp->d_name))
            ;
    }                           /* for */

    find_rotate_dash (h, TRUE);

    return 1;
}

/* --------------------------------------------------------------------------------------------- */

static void
init_find_vars (void)
{
    MC_PTR_FREE (old_dir);
    matches = 0;
    ignore_count = 0;

    /* Remove all the items from the stack */
    clear_stack ();

    g_strfreev (find_ignore_dirs);
    find_ignore_dirs = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
find_do_view_edit (gboolean unparsed_view, gboolean edit, char *dir, char *file, off_t search_start,
                   off_t search_end)
{
    const char *filename = NULL;
    int line;
    vfs_path_t *fullname_vpath;

    if (content_pattern != NULL)
    {
        filename = strchr (file + 4, ':') + 1;
        line = atoi (file + 4);
    }
    else
    {
        filename = file + 4;
        line = 0;
    }

    fullname_vpath = vfs_path_build_filename (dir, filename, (char *) NULL);
    if (edit)
        edit_file_at_line (fullname_vpath, use_internal_edit, line);
    else
        view_file_at_line (fullname_vpath, unparsed_view, use_internal_view, line, search_start,
                           search_end);
    vfs_path_free (fullname_vpath);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
view_edit_currently_selected_file (gboolean unparsed_view, gboolean edit)
{
    char *text = NULL;
    find_match_location_t *location;

    listbox_get_current (find_list, &text, (void **) &location);

    if ((text == NULL) || (location == NULL) || (location->dir == NULL))
        return MSG_NOT_HANDLED;

    find_do_view_edit (unparsed_view, edit, location->dir, text, location->start, location->end);
    return MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static void
find_calc_button_locations (const WDialog * h, gboolean all_buttons)
{
    const int cols = CONST_WIDGET (h)->cols;

    int l1, l2;

    l1 = fbuts[0].len + fbuts[1].len + fbuts[is_start ? 3 : 2].len + fbuts[4].len + 3;
    l2 = fbuts[5].len + fbuts[6].len + fbuts[7].len + 2;

    fbuts[0].x = (cols - l1) / 2;
    fbuts[1].x = fbuts[0].x + fbuts[0].len + 1;
    fbuts[2].x = fbuts[1].x + fbuts[1].len + 1;
    fbuts[3].x = fbuts[2].x;
    fbuts[4].x = fbuts[2].x + fbuts[is_start ? 3 : 2].len + 1;

    if (all_buttons)
    {
        fbuts[5].x = (cols - l2) / 2;
        fbuts[6].x = fbuts[5].x + fbuts[5].len + 1;
        fbuts[7].x = fbuts[6].x + fbuts[6].len + 1;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
find_adjust_header (WDialog * h)
{
    char title[BUF_MEDIUM];
    int title_len;

    if (content_pattern != NULL)
        g_snprintf (title, sizeof (title), _("Find File: \"%s\". Content: \"%s\""), find_pattern,
                    content_pattern);
    else
        g_snprintf (title, sizeof (title), _("Find File: \"%s\""), find_pattern);

    title_len = str_term_width1 (title);
    if (title_len > WIDGET (h)->cols - 6)
    {
        /* title is too wide, truncate it */
        title_len = WIDGET (h)->cols - 6;
        title_len = str_column_to_pos (title, title_len);
        title_len -= 3;         /* reserve space for three dots */
        title_len = str_offset_to_pos (title, title_len);
        /* mark that title is truncated */
        memmove (title + title_len, "...", 4);
    }

    dlg_set_title (h, title);
}

/* --------------------------------------------------------------------------------------------- */

static void
find_relocate_buttons (const WDialog * h, gboolean all_buttons)
{
    size_t i;

    find_calc_button_locations (h, all_buttons);

    for (i = 0; i < fbuts_num; i++)
        fbuts[i].button->x = CONST_WIDGET (h)->x + fbuts[i].x;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
find_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WDialog *h = DIALOG (w);

    switch (msg)
    {
    case MSG_INIT:
        find_adjust_header (h);
        return MSG_HANDLED;

    case MSG_KEY:
        if (parm == KEY_F (3) || parm == KEY_F (13))
        {
            gboolean unparsed_view = (parm == KEY_F (13));

            return view_edit_currently_selected_file (unparsed_view, FALSE);
        }
        if (parm == KEY_F (4))
            return view_edit_currently_selected_file (FALSE, TRUE);
        return MSG_NOT_HANDLED;

    case MSG_RESIZE:
        dlg_set_size (h, LINES - 4, COLS - 16);
        find_adjust_header (h);
        find_relocate_buttons (h, TRUE);
        return MSG_HANDLED;

    case MSG_IDLE:
        do_search (h);
        return MSG_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Handles the Stop/Start button in the find window */

static int
start_stop (WButton * button, int action)
{
    Widget *w = WIDGET (button);

    (void) action;

    running = is_start;
    widget_idle (WIDGET (find_dlg), running);
    is_start = !is_start;

    status_update (is_start ? _("Stopped") : _("Searching"));
    button_set_text (button, fbuts[is_start ? 3 : 2].text);

    find_relocate_buttons (w->owner, FALSE);
    dlg_redraw (w->owner);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/** Handle view command, when invoked as a button */

static int
find_do_view_file (WButton * button, int action)
{
    (void) button;
    (void) action;

    view_edit_currently_selected_file (FALSE, FALSE);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/** Handle edit command, when invoked as a button */

static int
find_do_edit_file (WButton * button, int action)
{
    (void) button;
    (void) action;

    view_edit_currently_selected_file (FALSE, TRUE);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
setup_gui (void)
{
    size_t i;
    int lines, cols;
    int y;

    static gboolean i18n_flag = FALSE;

    if (!i18n_flag)
    {
        for (i = 0; i < fbuts_num; i++)
        {
#ifdef ENABLE_NLS
            fbuts[i].text = _(fbuts[i].text);
#endif /* ENABLE_NLS */
            fbuts[i].len = str_term_width1 (fbuts[i].text) + 3;
            if (fbuts[i].flags == DEFPUSH_BUTTON)
                fbuts[i].len += 2;
        }

        i18n_flag = TRUE;
    }

    lines = LINES - 4;
    cols = COLS - 16;

    find_dlg =
        dlg_create (TRUE, 0, 0, lines, cols, WPOS_CENTER, FALSE, dialog_colors, find_callback, NULL,
                    "[Find File]", NULL);

    find_calc_button_locations (find_dlg, TRUE);

    y = 2;
    find_list = listbox_new (y, 2, lines - 10, cols - 4, FALSE, NULL);
    add_widget_autopos (find_dlg, find_list, WPOS_KEEP_ALL, NULL);
    y += WIDGET (find_list)->lines;

    add_widget_autopos (find_dlg, hline_new (y++, -1, -1), WPOS_KEEP_BOTTOM, NULL);

    found_num_label = label_new (y++, 4, "");
    add_widget_autopos (find_dlg, found_num_label, WPOS_KEEP_BOTTOM, NULL);

    status_label = label_new (y++, 4, _("Searching"));
    add_widget_autopos (find_dlg, status_label, WPOS_KEEP_BOTTOM, NULL);

    add_widget_autopos (find_dlg, hline_new (y++, -1, -1), WPOS_KEEP_BOTTOM, NULL);

    for (i = 0; i < fbuts_num; i++)
    {
        if (i == 3)
            fbuts[3].button = fbuts[2].button;
        else
        {
            fbuts[i].button =
                WIDGET (button_new
                        (y, fbuts[i].x, fbuts[i].ret_cmd, fbuts[i].flags, fbuts[i].text,
                         fbuts[i].callback));
            add_widget_autopos (find_dlg, fbuts[i].button, WPOS_KEEP_BOTTOM, NULL);
        }

        if (i == quit_button)
            y++;
    }

    widget_select (WIDGET (find_list));
}

/* --------------------------------------------------------------------------------------------- */

static int
run_process (void)
{
    int ret;

    search_content_handle = mc_search_new (content_pattern, NULL);
    if (search_content_handle)
    {
        search_content_handle->search_type =
            options.content_regexp ? MC_SEARCH_T_REGEX : MC_SEARCH_T_NORMAL;
        search_content_handle->is_case_sensitive = options.content_case_sens;
        search_content_handle->whole_words = options.content_whole_words;
#ifdef HAVE_CHARSET
        search_content_handle->is_all_charsets = options.content_all_charsets;
#endif
    }
    search_file_handle = mc_search_new (find_pattern, NULL);
    search_file_handle->search_type = options.file_pattern ? MC_SEARCH_T_GLOB : MC_SEARCH_T_REGEX;
    search_file_handle->is_case_sensitive = options.file_case_sens;
#ifdef HAVE_CHARSET
    search_file_handle->is_all_charsets = options.file_all_charsets;
#endif
    search_file_handle->is_entire_line = options.file_pattern;

    resuming = FALSE;

    widget_idle (WIDGET (find_dlg), TRUE);
    ret = dlg_run (find_dlg);

    mc_search_free (search_file_handle);
    search_file_handle = NULL;
    mc_search_free (search_content_handle);
    search_content_handle = NULL;

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static void
kill_gui (void)
{
    widget_idle (WIDGET (find_dlg), FALSE);
    dlg_destroy (find_dlg);
}

/* --------------------------------------------------------------------------------------------- */

static int
do_find (const char *start_dir, ssize_t start_dir_len, const char *ignore_dirs,
         char **dirname, char **filename)
{
    int return_value = 0;
    char *dir_tmp = NULL, *file_tmp = NULL;

    setup_gui ();

    init_find_vars ();
    parse_ignore_dirs (ignore_dirs);
    push_directory (vfs_path_from_str (start_dir));

    return_value = run_process ();

    /* Clear variables */
    init_find_vars ();

    get_list_info (&file_tmp, &dir_tmp, NULL, NULL);

    if (dir_tmp)
        *dirname = g_strdup (dir_tmp);
    if (file_tmp)
        *filename = g_strdup (file_tmp);

    if (return_value == B_PANELIZE && *filename)
    {
        int link_to_dir, stale_link;
        int i;
        struct stat st;
        GList *entry;
        dir_list *list = &current_panel->dir;
        char *name = NULL;

        panel_clean_dir (current_panel);
        dir_list_init (list);

        for (i = 0, entry = listbox_get_first_link (find_list); entry != NULL;
             i++, entry = g_list_next (entry))
        {
            const char *lc_filename = NULL;
            WLEntry *le = LENTRY (entry->data);
            find_match_location_t *location = le->data;
            char *p;

            if ((le->text == NULL) || (location == NULL) || (location->dir == NULL))
                continue;

            if (!content_is_empty)
                lc_filename = strchr (le->text + 4, ':') + 1;
            else
                lc_filename = le->text + 4;

            name = mc_build_filename (location->dir, lc_filename, (char *) NULL);
            /* skip initial start dir */
            if (start_dir_len < 0)
                p = name;
            else
            {
                p = name + (size_t) start_dir_len;
                if (IS_PATH_SEP (*p))
                    p++;
            }

            if (!handle_path (p, &st, &link_to_dir, &stale_link))
            {
                g_free (name);
                continue;
            }
            /* Need to grow the *list? */
            if (list->len == list->size && !dir_list_grow (list, DIR_LIST_RESIZE_STEP))
            {
                g_free (name);
                break;
            }

            /* don't add files more than once to the panel */
            if (!content_is_empty && list->len != 0
                && strcmp (list->list[list->len - 1].fname, p) == 0)
            {
                g_free (name);
                continue;
            }

            list->list[list->len].fnamelen = strlen (p);
            list->list[list->len].fname = g_strndup (p, list->list[list->len].fnamelen);
            list->list[list->len].f.marked = 0;
            list->list[list->len].f.link_to_dir = link_to_dir;
            list->list[list->len].f.stale_link = stale_link;
            list->list[list->len].f.dir_size_computed = 0;
            list->list[list->len].st = st;
            list->list[list->len].sort_key = NULL;
            list->list[list->len].second_sort_key = NULL;
            list->len++;
            g_free (name);
            if ((list->len & 15) == 0)
                rotate_dash (TRUE);
        }

        current_panel->is_panelized = TRUE;
        panelize_absolutize_if_needed (current_panel);
        panelize_save_panel (current_panel);
    }

    kill_gui ();
    do_search (NULL);           /* force do_search to release resources */
    MC_PTR_FREE (old_dir);
    rotate_dash (FALSE);

    return return_value;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
find_file (void)
{
    char *start_dir = NULL, *ignore_dirs = NULL;
    ssize_t start_dir_len;

    find_pattern = NULL;
    content_pattern = NULL;

    while (find_parameters (&start_dir, &start_dir_len,
                            &ignore_dirs, &find_pattern, &content_pattern))
    {
        char *filename = NULL, *dirname = NULL;
        int v = B_CANCEL;

        content_is_empty = content_pattern == NULL;

        if (find_pattern[0] != '\0')
        {
            last_refresh.tv_sec = 0;
            last_refresh.tv_usec = 0;

            is_start = FALSE;

            if (!content_is_empty && !str_is_valid_string (content_pattern))
                MC_PTR_FREE (content_pattern);

            v = do_find (start_dir, start_dir_len, ignore_dirs, &dirname, &filename);
        }

        g_free (start_dir);
        g_free (ignore_dirs);
        MC_PTR_FREE (find_pattern);

        if (v == B_ENTER)
        {
            if (dirname != NULL)
            {
                vfs_path_t *dirname_vpath;

                dirname_vpath = vfs_path_from_str (dirname);
                do_cd (dirname_vpath, cd_exact);
                vfs_path_free (dirname_vpath);
                if (filename != NULL)
                    try_to_select (current_panel,
                                   filename + (content_pattern != NULL
                                               ? strchr (filename + 4, ':') - filename + 1 : 4));
            }
            else if (filename != NULL)
            {
                vfs_path_t *filename_vpath;

                filename_vpath = vfs_path_from_str (filename);
                do_cd (filename_vpath, cd_exact);
                vfs_path_free (filename_vpath);
            }
        }

        MC_PTR_FREE (content_pattern);
        g_free (dirname);
        g_free (filename);

        if (v == B_ENTER || v == B_CANCEL)
            break;

        if (v == B_PANELIZE)
        {
            panel_re_sort (current_panel);
            try_to_select (current_panel, NULL);
            break;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

#ifndef MC_SKIN_H
#define MC_SKIN_H

#include "lib/global.h"

#include "lib/mcconfig.h"

#include "lib/tty/color.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

enum
{
    /* Basic colors */
    CORE_DEFAULT_COLOR = TTY_COLOR_MAP_OFFSET,
    CORE_NORMAL_COLOR,
    CORE_MARKED_COLOR,
    CORE_SELECTED_COLOR,
    CORE_MARKED_SELECTED_COLOR,
    CORE_DISABLED_COLOR,
    CORE_REVERSE_COLOR,
    CORE_COMMAND_MARK_COLOR,
    CORE_HEADER_COLOR,
    CORE_SHADOW_COLOR,
    CORE_FRAME_COLOR,

    /* Dialog colors */
    DIALOG_NORMAL_COLOR,
    DIALOG_FOCUS_COLOR,
    DIALOG_HOT_NORMAL_COLOR,
    DIALOG_HOT_FOCUS_COLOR,
    DIALOG_SELECTED_NORMAL_COLOR,
    DIALOG_SELECTED_FOCUS_COLOR,
    DIALOG_TITLE_COLOR,
    DIALOG_FRAME_COLOR,

    /* Error dialog colors */
    ERROR_NORMAL_COLOR,
    ERROR_FOCUS_COLOR,
    ERROR_HOT_NORMAL_COLOR,
    ERROR_HOT_FOCUS_COLOR,
    ERROR_TITLE_COLOR,
    ERROR_FRAME_COLOR,

    /* File highlight default color, the rest are constructed dynamically */
    FILEHIGHLIGHT_DEFAULT_COLOR,

    /* Menu colors */
    MENU_ENTRY_COLOR,
    MENU_SELECTED_COLOR,
    MENU_HOT_COLOR,
    MENU_HOTSEL_COLOR,
    MENU_INACTIVE_COLOR,
    MENU_FRAME_COLOR,

    /* Popup menu colors */
    PMENU_ENTRY_COLOR,
    PMENU_SELECTED_COLOR,
    PMENU_HOT_COLOR,     // unused: not implemented yet
    PMENU_HOTSEL_COLOR,  // unused: not implemented yet
    PMENU_TITLE_COLOR,
    PMENU_FRAME_COLOR,

    BUTTONBAR_HOTKEY_COLOR,
    BUTTONBAR_BUTTON_COLOR,

    STATUSBAR_COLOR,

    /*
     * This should be selectable independently. Default has to be black background
     * foreground does not matter at all.
     */
    CORE_GAUGE_COLOR,
    CORE_INPUT_COLOR,
    CORE_INPUT_UNCHANGED_COLOR,
    CORE_INPUT_MARK_COLOR,
    CORE_INPUT_HISTORY_COLOR,
    CORE_COMMAND_HISTORY_COLOR,

    HELP_NORMAL_COLOR,
    HELP_ITALIC_COLOR,
    HELP_BOLD_COLOR,
    HELP_LINK_COLOR,
    HELP_SLINK_COLOR,
    HELP_TITLE_COLOR,
    HELP_FRAME_COLOR,

    VIEWER_NORMAL_COLOR,
    VIEWER_BOLD_COLOR,
    VIEWER_UNDERLINED_COLOR,
    VIEWER_BOLD_UNDERLINED_COLOR,
    VIEWER_SELECTED_COLOR,
    VIEWER_FRAME_COLOR,

    /*
     * editor colors - only 4 for normal, search->found, select, and whitespace
     * respectively
     * Last is defined to view color.
     */
    EDITOR_NORMAL_COLOR,
    EDITOR_NONPRINTABLE_COLOR,
    EDITOR_BOLD_COLOR,
    EDITOR_MARKED_COLOR,
    EDITOR_WHITESPACE_COLOR,
    EDITOR_RIGHT_MARGIN_COLOR,
    EDITOR_BACKGROUND_COLOR,
    EDITOR_FRAME_COLOR,
    EDITOR_FRAME_ACTIVE_COLOR,
    EDITOR_FRAME_DRAG_COLOR,
    /* color of left 8 char status per line */
    EDITOR_LINE_STATE_COLOR,
    EDITOR_BOOKMARK_COLOR,
    EDITOR_BOOKMARK_FOUND_COLOR,

    /* Diff colors */
    DIFFVIEWER_ADDED_COLOR,
    DIFFVIEWER_CHANGEDLINE_COLOR,
    DIFFVIEWER_CHANGEDNEW_COLOR,
    DIFFVIEWER_CHANGED_COLOR,
    DIFFVIEWER_REMOVED_COLOR,
    DIFFVIEWER_ERROR_COLOR,

    COLOR_MAP_NEXT
};

#define COLOR_MAP_SIZE (COLOR_MAP_NEXT - TTY_COLOR_MAP_OFFSET)

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct mc_skin_struct
{
    gchar *name;
    gchar *description;
    mc_config_t *config;
    GHashTable *colors;
    gboolean have_256_colors;
    gboolean have_true_colors;
} mc_skin_t;

/*** global variables defined in .c file *********************************************************/

extern mc_skin_t mc_skin__default;

/*** declarations of public functions ************************************************************/

gboolean mc_skin_init (const gchar *skin_override, GError **error);
void mc_skin_deinit (void);

int mc_skin_color_get (const gchar *group, const gchar *name);

void mc_skin_lines_parse_ini_file (mc_skin_t *mc_skin);

gchar *mc_skin_get (const gchar *group, const gchar *key, const gchar *default_value);

GPtrArray *mc_skin_list (void);

#endif

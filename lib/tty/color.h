/** \file color.h
 *  \brief Header: color setup
 *
 * PLEASE FORGOT ABOUT tty/color.h!
 * Use skin engine for getting needed color pairs.
 *
 * edit/syntax.c may use this file directly, I'm agree. :)
 *
 */

#ifndef MC__COLOR_H
#define MC__COLOR_H

#include "lib/global.h"  // glib.h

#ifdef HAVE_SLANG
#include "color-slang.h"
#else
#include "tty-ncurses.h"
#endif

/*** typedefs(not structures) and defined constants **********************************************/

typedef struct
{
    char *fg;
    char *bg;
    char *attrs;
    size_t pair_index;
} tty_color_pair_t;

/*** enums ***************************************************************************************/

/*
 * Color values below this number refer directly to the ncurses/slang color pair id.
 *
 * Color values beginning with this number represent a role, for which the ncurses/slang color pair
 * id is looked up runtime from tty_color_role_to_pair[] which is initialized when loading the skin.
 *
 * This way the numbers representing skinnable colors remain the same across skin changes.
 *
 * (Note: Another approach could be to allocate a new ncurses/slang color pair id for every role,
 * even if they use the same colors in the skin. The problem with this is that for 8-color terminal
 * descriptors (like TERM=linux and TERM=xterm) ncurses only allows 64 color pairs, so we can't
 * afford to allocate new ids for duplicates.)
 *
 * In src/editor/editdraw.c these values are shifted by 16 bits to the left and stored in an int.
 * Keep this number safely below 65536 to avoid overflow.
 */
#define COLOR_MAP_OFFSET 4096

enum
{
    /* Basic colors */
    CORE_NORMAL_COLOR = COLOR_MAP_OFFSET,
    CORE_MARKED_COLOR,
    CORE_SELECTED_COLOR,
    CORE_MARKED_SELECTED_COLOR,
    CORE_DISABLED_COLOR,
    CORE_REVERSE_COLOR,
    CORE_HINTBAR_COLOR,
    CORE_SHELLPROMPT_COLOR,
    CORE_COMMANDLINE_COLOR,
    CORE_COMMANDLINE_MARK_COLOR,
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

#define COLOR_MAP_SIZE (COLOR_MAP_NEXT - COLOR_MAP_OFFSET)

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

extern int tty_color_role_to_pair[];

/*** declarations of public functions ************************************************************/

void tty_init_colors (gboolean disable, gboolean force);
void tty_colors_done (void);

gboolean tty_use_colors (void);
int tty_try_alloc_color_pair (const tty_color_pair_t *color, gboolean is_temp);

void tty_color_free_temp (void);
void tty_color_free_all (void);

void tty_setcolor (int color);
void tty_set_normal_attrs (void);

extern gboolean tty_use_256colors (GError **error);
extern gboolean tty_use_truecolors (GError **error);

/*** inline functions ****************************************************************************/

#endif

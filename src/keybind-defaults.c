/*
   Default values for keybinding engine

   Copyright (C) 2009-2020
   Free Software Foundation, Inc.

   Written by:
   Vitja Makarov, 2005
   Ilia Maslakov <il.smind@gmail.com>, 2009, 2010
   Andrew Borodin <aborodin@vmail.ru>, 2010, 2011

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

#include <config.h>

#include "lib/global.h"
#include "lib/widget.h"         /* dialog_map, input_map, listbox_map, menu_map, radio_map */

#include "keybind-defaults.h"

/*** global variables ****************************************************************************/

GArray *filemanager_keymap = NULL;
GArray *filemanager_x_keymap = NULL;
GArray *panel_keymap = NULL;
GArray *dialog_keymap = NULL;
GArray *menu_keymap = NULL;
GArray *input_keymap = NULL;
GArray *listbox_keymap = NULL;
GArray *radio_keymap = NULL;
GArray *tree_keymap = NULL;
GArray *help_keymap = NULL;
#ifdef ENABLE_EXT2FS_ATTR
GArray *chattr_keymap = NULL;
#endif
#ifdef USE_INTERNAL_EDIT
GArray *editor_keymap = NULL;
GArray *editor_x_keymap = NULL;
#endif
GArray *viewer_keymap = NULL;
GArray *viewer_hex_keymap = NULL;
#ifdef USE_DIFF_VIEW
GArray *diff_keymap = NULL;
#endif

const global_keymap_t *filemanager_map = NULL;
const global_keymap_t *filemanager_x_map = NULL;
const global_keymap_t *panel_map = NULL;
const global_keymap_t *tree_map = NULL;
const global_keymap_t *help_map = NULL;
#ifdef ENABLE_EXT2FS_ATTR
const global_keymap_t *chattr_map = NULL;
#endif
#ifdef USE_INTERNAL_EDIT
const global_keymap_t *editor_map = NULL;
const global_keymap_t *editor_x_map = NULL;
#endif
const global_keymap_t *viewer_map = NULL;
const global_keymap_t *viewer_hex_map = NULL;
#ifdef USE_DIFF_VIEW
const global_keymap_t *diff_map = NULL;
#endif

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/* default keymaps in ini (key=value) format */
typedef struct global_keymap_ini_t
{
    const char *key;
    const char *value;
} global_keymap_ini_t;

/*** file scope variables ************************************************************************/

/* midnight */
static const global_keymap_ini_t default_filemanager_keymap[] = {
    {"ChangePanel", "tab; ctrl-i"},
    {"Help", "f1"},
    {"UserMenu", "f2"},
    {"View", "f3"},
    {"Edit", "f4"},
    {"Copy", "f5"},
    {"Move", "f6"},
    {"MakeDir", "f7"},
    {"Delete", "f8"},
    {"Menu", "f9"},
    {"Quit", "f10"},
    {"MenuLastSelected", "f19"},
    {"QuitQuiet", "f20"},
    {"History", "alt-h"},
    {"EditorViewerHistory", "alt-shift-e"},
    {"DirSize", "ctrl-space"},
    /* Copy useful information to the command line */
    {"PutCurrentPath", "alt-a"},
    {"PutOtherPath", "alt-shift-a"},
    {"PutCurrentSelected", "alt-enter; ctrl-enter"},
    {"PutCurrentFullSelected", "ctrl-shift-enter"},
    {"CdQuick", "alt-c"},
    /* To access the directory hotlist */
    {"HotList", "ctrl-backslash"},
    /* Suspend */
    {"Suspend", "ctrl-z"},
    /* The filtered view command */
    {"ViewFiltered", "alt-exclamation"},
    /* Find file */
    {"Find", "alt-question"},
    /* Panel refresh */
    {"Reread", "ctrl-r"},
    /* Switch listing between long, user defined and full formats */
    /* Swap panels */
    {"Swap", "ctrl-u"},
    /* Resize panels */
    {"SplitEqual", "alt-equal"},
    {"SplitMore", "alt-shift-right"},
    {"SplitLess", "alt-shift-left"},
    /* View output */
    {"Shell", "ctrl-o"},
    {"ShowHidden", "alt-dot"},
    {"SplitVertHoriz", "alt-comma"},
    {"ExtendedKeyMap", "ctrl-x"},
    /* Select/unselect group */
    {"Select", "kpplus"},
    {"Unselect", "kpminus"},
    {"SelectInvert", "kpasterisk"},
    /* List of screens */
    {"ScreenList", "alt-prime"},
    {NULL, NULL}
};

static const global_keymap_ini_t default_filemanager_x_keymap[] = {
    {"CompareDirs", "d"},
#ifdef USE_DIFF_VIEW
    {"CompareFiles", "ctrl-d"},
#endif /* USE_DIFF_VIEW */
#ifdef ENABLE_VFS
    {"VfsList", "a"},
#endif /* ENABLE_VFS */
    {"PutCurrentPath", "p"},
    {"PutOtherPath", "ctrl-p"},
    {"PutCurrentTagged", "t"},
    {"PutOtherTagged", "ctrl-t"},
    {"ChangeMode", "c"},
    {"ChangeOwn", "o"},
#ifdef ENABLE_EXT2FS_ATTR
    {"ChangeAttributes", "e"},
#endif /* ENABLE_EXT2FS_ATTR */
    {"PutCurrentLink", "r"},
    {"PutOtherLink", "ctrl-r"},
    {"Link", "l"},
    {"LinkSymbolic", "s"},
    {"LinkSymbolicRelative", "v"},
    {"LinkSymbolicEdit", "ctrl-s"},
    {"PanelInfo", "i"},
    {"PanelQuickView", "q"},
    {"HotListAdd", "h"},
#ifdef ENABLE_BACKGROUND
    {"Jobs", "j"},
#endif /* ENABLE_BACKGROUND */
    {"ExternalPanelize", "!"},
    {NULL, NULL}
};

/* panel */
static const global_keymap_ini_t default_panel_keymap[] = {
    {"CycleListingFormat", "alt-t"},
    {"PanelOtherCd", "alt-o"},
    {"PanelOtherCdLink", "alt-l"},
    {"CopySingle", "f15"},
    {"DeleteSingle", "f18"},
    {"Enter", "enter"},
    {"EditNew", "f14"},
    {"MoveSingle", "f16"},
    {"SelectInvert", "alt-asterisk"},
    {"Select", "alt-plus"},
    {"Unselect", "alt-minus"},
    {"ViewRaw", "f13"},
    {"CdChild", "ctrl-pgdn"},
    {"CdParent", "ctrl-pgup"},
    {"History", "alt-shift-h"},
    {"HistoryNext", "alt-u"},
    {"HistoryPrev", "alt-y"},
    {"BottomOnScreen", "alt-j"},
    {"MiddleOnScreen", "alt-r"},
    {"TopOnScreen", "alt-g"},
    {"Mark", "insert; ctrl-t"},
    {"MarkDown", "shift-down"},
    {"MarkUp", "shift-up"},
    {"Up", "up; ctrl-p"},
    {"Down", "down; ctrl-n"},
    {"Left", "left"},
    {"Right", "right"},
    {"Top", "alt-lt; home; a1"},
    {"Bottom", "alt-gt; end; c1"},
    {"PageDown", "pgdn; ctrl-v"},
    {"PageUp", "pgup; alt-v"},
#ifdef HAVE_CHARSET
    {"SelectCodepage", "alt-e"},
#endif
    {"Search", "ctrl-s; alt-s"},
    {"PanelOtherSync", "alt-i"},
    {NULL, NULL}
};

/* dialog */
static const global_keymap_ini_t default_dialog_keymap[] = {
    {"Ok", "enter"},
    {"Cancel", "f10; esc; ctrl-g"},
    {"Up", "up; left"},
    {"Down", "down; right"},
#if 0
    {"Left", "up; left"},
    {"Right", "down; right"},
#endif
    {"Help", "f1"},
    {"Suspend", "ctrl-z"},
    {"Refresh", "ctrl-l"},
    {"ScreenList", "alt-prime"},
    {"ScreenNext", "alt-rbrace"},
    {"ScreenPrev", "alt-lbrace"},
    {NULL, NULL}
};

/* menubar */
static const global_keymap_ini_t default_menu_keymap[] = {
    {"Help", "f1"},
    {"Left", "left; ctrl-b"},
    {"Right", "right; ctrl-f"},
    {"Up", "up; ctrl-p"},
    {"Down", "down; ctrl-n"},
    {"Home", "home; alt-lt; ctrl-a"},
    {"End", "end; alt-gt; ctrl-e"},
    {"Enter", "enter"},
    {"Quit", "f10; ctrl-g; esc"},
    {NULL, NULL}
};

/* input line */
static const global_keymap_ini_t default_input_keymap[] = {
    /* Motion */
    {"Home", "ctrl-a; alt-lt; home; a1"},
    {"End", "ctrl-e; alt-gt; end; c1"},
    {"Left", "left; alt-left; ctrl-b"},
    {"Right", "right; alt-right; ctrl-f"},
    {"WordLeft", "ctrl-left; alt-b"},
    {"WordRight", "ctrl-right; alt-f"},
    /* Mark */
    {"MarkLeft", "shift-left"},
    {"MarkRight", "shift-right"},
    {"MarkToWordBegin", "ctrl-shift-left"},
    {"MarkToWordEnd", "ctrl-shift-right"},
    {"MarkToHome", "shift-home"},
    {"MarkToEnd", "shift-end"},
    /* Editing */
    {"Backspace", "backspace; ctrl-h"},
    {"Delete", "delete; ctrl-d"},
    {"DeleteToWordEnd", "alt-d"},
    {"DeleteToWordBegin", "alt-backspace"},
    /* Region manipulation */
    {"Remove", "ctrl-w"},
    {"Store", "alt-w"},
    {"Yank", "ctrl-y"},
    {"DeleteToEnd", "ctrl-k"},
    /* History */
    {"History", "alt-h"},
    {"HistoryPrev", "alt-p; ctrl-down"},
    {"HistoryNext", "alt-n; ctrl-up"},
    /* Completion */
    {"Complete", "alt-tab"},
    {NULL, NULL}
};

/* listbox */
static const global_keymap_ini_t default_listbox_keymap[] = {
    {"Up", "up; ctrl-p"},
    {"Down", "down; ctrl-n"},
    {"Top", "home; alt-lt; a1"},
    {"Bottom", "end; alt-gt; c1"},
    {"PageUp", "pgup; alt-v"},
    {"PageDown", "pgdn; ctrl-v"},
    {"Delete", "delete; d"},
    {"Clear", "shift-delete; shift-d"},
    {"View", "f3"},
    {"Edit", "f4"},
    {"Enter", "enter"},
    {NULL, NULL}
};

/* radio */
static const global_keymap_ini_t default_radio_keymap[] = {
    {"Up", "up; ctrl-p"},
    {"Down", "down; ctrl-n"},
    {"Top", "home; alt-lt; a1"},
    {"Bottom", "end; alt-gt; c1"},
    {"Select", "space"},
    {NULL, NULL}
};

/* tree */
static const global_keymap_ini_t default_tree_keymap[] = {
    {"Help", "f1"},
    {"Rescan", "f2; ctrl-r"},
    {"Forget", "f3"},
    {"ToggleNavigation", "f4"},
    {"Copy", "f5"},
    {"Move", "f6"},
#if 0
    {"MakeDir", "f7"},
#endif
    {"Delete", "f8; delete"},
    {"Up", "up; ctrl-p"},
    {"Down", "down; ctrl-n"},
    {"Left", "left"},
    {"Right", "right"},
    {"Top", "home; alt-lt; a1"},
    {"Bottom", "end; alt-gt; c1"},
    {"PageUp", "pgup; alt-v"},
    {"PageDown", "pgdn; ctrl-v"},
    {"Enter", "enter"},
    {"Search", "ctrl-s; alt-s"},
    {NULL, NULL}
};

/* help */
static const global_keymap_ini_t default_help_keymap[] = {
    {"Help", "f1"},
    {"Index", "f2; c"},
    {"Back", "f3; left; l"},
    {"Quit", "f10; esc"},
    {"Up", "up; ctrl-p"},
    {"Down", "down; ctrl-n"},
    {"PageDown", "f; space; pgdn; ctrl-v"},
    {"PageUp", "b; pgup; alt-v; backspace"},
    {"HalfPageDown", "d"},
    {"HalfPageUp", "u"},
    {"Top", "home; ctrl-home; ctrl-pgup; a1; alt-lt; g"},
    {"Bottom", "end; ctrl-end; ctrl-pgdn; c1; alt-gt; shift-g"},
    {"Enter", "right; enter"},
    {"LinkNext", "tab"},
    {"LinkPrev", "alt-tab"},
    {"NodeNext", "n"},
    {"NodePrev", "p"},
    {NULL, NULL}
};

#ifdef ENABLE_EXT2FS_ATTR
/* chattr dialog */
static const global_keymap_ini_t default_chattr_keymap[] = {
    {"Up", "up; left; ctrl-p"},
    {"Down", "down; right; ctrl-n"},
    {"Top", "home; alt-lt; a1"},
    {"Bottom", "end; alt-gt; c1"},
    {"PageUp", "pgup; alt-v"},
    {"PageDown", "pgdn; ctrl-v"},
    {"Mark", "t; shift-t"},
    {"MarkAndDown", "insert"},
    {NULL, NULL}
};
#endif /* ENABLE_EXT2FS_ATTR */

#ifdef USE_INTERNAL_EDIT
static const global_keymap_ini_t default_editor_keymap[] = {
    {"Enter", "enter"},
    {"Return", "shift-enter; ctrl-enter; ctrl-shift-enter"},    /* useful for pasting multiline text */
    {"Tab", "tab; shift-tab; ctrl-tab; ctrl-shift-tab"},        /* ditto */
    {"BackSpace", "backspace; ctrl-h"},
    {"Delete", "delete; ctrl-d"},
    {"Left", "left"},
    {"Right", "right"},
    {"Up", "up"},
    {"Down", "down"},
    {"Home", "home"},
    {"End", "end"},
    {"PageUp", "pgup"},
    {"PageDown", "pgdn"},
    {"WordLeft", "ctrl-left; ctrl-z"},
    {"WordRight", "ctrl-right; ctrl-x"},
    {"InsertOverwrite", "insert"},
    {"Help", "f1"},
    {"Save", "f2"},
    {"Mark", "f3"},
    {"Replace", "f4"},
    {"Copy", "f5"},
    {"Move", "f6"},
    {"Search", "f7"},
    {"Remove", "f8; ctrl-delete"},
    {"Menu", "f9"},
    {"Quit", "f10; esc"},
    {"UserMenu", "f11"},
    {"SaveAs", "f12; ctrl-f2"},
    {"MarkColumn", "f13"},
    {"ReplaceContinue", "f14; ctrl-f4"},
    {"InsertFile", "f15"},
    {"SearchContinue", "f17; ctrl-f7"},
    {"EditNew", "ctrl-n"},
    {"DeleteToWordBegin", "alt-backspace"},
    {"DeleteToWordEnd", "alt-d"},
    {"DeleteLine", "ctrl-y"},
    {"DeleteToEnd", "ctrl-k"},
    {"Undo", "ctrl-u; ctrl-backspace"},
    {"Redo", "alt-r"},
#ifdef HAVE_CHARSET
    {"SelectCodepage", "alt-e"},
#endif
    {"Goto", "alt-l; alt-shift-l"},
    {"Refresh", "ctrl-l"},
    {"Shell", "ctrl-o"},
    {"Top", "ctrl-home; ctrl-pgup; alt-lt"},
    {"Bottom", "ctrl-end; ctrl-pgdn; alt-gt"},
    {"TopOnScreen", "ctrl-pgup"},
    {"BottomOnScreen", "ctrl-pgdn"},
    {"ScrollUp", "ctrl-up"},
    {"ScrollDown", "ctrl-down"},
    {"Store", "ctrl-insert"},
    {"Paste", "shift-insert"},
    {"Cut", "shift-delete"},
    {"BlockSave", "ctrl-f"},
    {"MarkLeft", "shift-left"},
    {"MarkRight", "shift-right"},
    {"MarkUp", "shift-up"},
    {"MarkDown", "shift-down"},
    {"MarkPageUp", "shift-pgup"},
    {"MarkPageDown", "shift-pgdn"},
    {"MarkToWordBegin", "ctrl-shift-left"},
    {"MarkToWordEnd", "ctrl-shift-right"},
    {"MarkToHome", "shift-home"},
    {"MarkToEnd", "shift-end"},
    {"MarkToFileBegin", "ctrl-shift-home"},
    {"MarkToFileEnd", "ctrl-shift-end"},
    {"MarkToPageBegin", "ctrl-shift-pgup"},
    {"MarkToPageEnd", "ctrl-shift-pgdn"},
    {"MarkScrollUp", "ctrl-shift-up"},
    {"MarkScrollDown", "ctrl-shift-down"},
    {"MarkColumnLeft", "alt-left"},
    {"MarkColumnRight", "alt-right"},
    {"MarkColumnUp", "alt-up"},
    {"MarkColumnDown", "alt-down"},
    {"MarkColumnPageUp", "alt-pgup"},
    {"MarkColumnPageDown", "alt-pgdn"},
    {"InsertLiteral", "ctrl-q"},
    {"Complete", "alt-tab"},
    {"MatchBracket", "alt-b"},
    {"ParagraphFormat", "alt-p"},
    {"Bookmark", "alt-k"},
    {"BookmarkFlush", "alt-o"},
    {"BookmarkNext", "alt-j"},
    {"BookmarkPrev", "alt-i"},
    {"MacroStartStopRecord", "ctrl-r"},
    {"MacroExecute", "ctrl-a"},
    {"ShowNumbers", "alt-n"},
    {"ShowTabTws", "alt-underline"},
    {"SyntaxOnOff", "ctrl-s"},
    {"Find", "alt-enter"},
    {"FilePrev", "alt-minus"},
    {"FileNext", "alt-plus"},
    {"Sort", "alt-t"},
    {"Mail", "alt-m"},
    {"ExternalCommand", "alt-u"},
#ifdef HAVE_ASPELL
    {"SpellCheckCurrentWord", "ctrl-p"},
#endif
    {"ExtendedKeyMap", "ctrl-x"},
    {NULL, NULL}
};

/* emacs keyboard layout emulation */
static const global_keymap_ini_t default_editor_x_keymap[] = {
    {NULL, NULL}
};
#endif /* USE_INTERNAL_EDIT */

/* viewer */
static const global_keymap_ini_t default_viewer_keymap[] = {
    {"Help", "f1"},
    {"WrapMode", "f2"},
    {"Quit", "f3; f10; q; esc"},
    {"HexMode", "f4"},
    {"Goto", "f5"},
    {"Search", "f7"},
    {"SearchContinue", "f17; n"},
    {"MagicMode", "f8"},
    {"NroffMode", "f9"},
    {"Home", "ctrl-a"},
    {"End", "ctrl-e"},
    {"Left", "h; left"},
    {"Right", "l; right"},
    {"LeftQuick", "ctrl-left"},
    {"RightQuick", "ctrl-right"},
    {"Up", "k; y; insert; up; ctrl-p"},
    {"Down", "j; e; delete; down; enter; ctrl-n"},
    {"PageDown", "f; space; pgdn; ctrl-v"},
    {"PageUp", "b; pgup; alt-v; backspace"},
    {"HalfPageDown", "d"},
    {"HalfPageUp", "u"},
    {"Top", "home; ctrl-home; ctrl-pgup; a1; alt-lt; g"},
    {"Bottom", "end; ctrl-end; ctrl-pgdn; c1; alt-gt; shift-g"},
    {"BookmarkGoto", "m"},
    {"Bookmark", "r"},
    {"FileNext", "ctrl-f"},
    {"FilePrev", "ctrl-b"},
#ifdef HAVE_CHARSET
    {"SelectCodepage", "alt-e"},
#endif
    {"Shell", "ctrl-o"},
    {"Ruler", "alt-r"},
    {"SearchForward", "slash"},
    {"SearchBackward", "question"},
    {"SearchForwardContinue", "ctrl-s"},
    {"SearchBackwardContinue", "ctrl-r"},
    {"SearchOppositeContinue", "shift-n"},
    {"History", "alt-shift-e"},
    {NULL, NULL}
};

/* hex viewer */
static const global_keymap_ini_t default_viewer_hex_keymap[] = {
    {"Help", "f1"},
    {"HexEditMode", "f2"},
    {"Quit", "f3; f10; q; esc"},
    {"HexMode", "f4"},
    {"Goto", "f5"},
    {"Save", "f6"},
    {"Search", "f7"},
    {"SearchContinue", "f17; n"},
    {"MagicMode", "f8"},
    {"NroffMode", "f9"},
    {"ToggleNavigation", "tab"},
    {"Home", "ctrl-a; home"},
    {"End", "ctrl-e; end"},
    {"Left", "b; left"},
    {"Right", "f; right"},
    {"Up", "k; y; up"},
    {"Down", "j; delete; down"},
    {"PageDown", "pgdn; ctrl-v"},
    {"PageUp", "pgup; alt-v"},
    {"Top", "ctrl-home; ctrl-pgup; a1; alt-lt; g"},
    {"Bottom", "ctrl-end; ctrl-pgdn; c1; alt-gt; shift-g"},
#ifdef HAVE_CHARSET
    {"SelectCodepage", "alt-e"},
#endif
    {"Shell", "ctrl-o"},
    {"SearchForward", "slash"},
    {"SearchBackward", "question"},
    {"SearchForwardContinue", "ctrl-s"},
    {"SearchBackwardContinue", "ctrl-r"},
    {"SearchOppositeContinue", "shift-n"},
    {"History", "alt-shift-e"},
    {NULL, NULL}
};

#ifdef  USE_DIFF_VIEW
/* diff viewer */
static const global_keymap_ini_t default_diff_keymap[] = {
    {"ShowSymbols", "alt-s; s"},
    {"ShowNumbers", "alt-n; l"},
    {"SplitFull", "f"},
    {"SplitEqual", "equal"},
    {"SplitMore", "gt"},
    {"SplitLess", "lt"},
    {"Tab2", "2"},
    {"Tab3", "3"},
    {"Tab4", "4"},
    {"Tab8", "8"},
    {"Swap", "ctrl-u"},
    {"Redo", "ctrl-r"},
    {"HunkNext", "n; enter; space"},
    {"HunkPrev", "p; backspace"},
    {"Goto", "g; shift-g"},
    {"Save", "f2"},
    {"Edit", "f4"},
    {"EditOther", "f14"},
    {"Merge", "f5"},
    {"MergeOther", "f15"},
    {"Search", "f7"},
    {"SearchContinue", "f17"},
    {"Options", "f9"},
    {"Top", "ctrl-home"},
    {"Bottom", "ctrl-end"},
    {"Down", "down"},
    {"Up", "up"},
    {"LeftQuick", "ctrl-left"},
    {"RightQuick", "ctrl-right"},
    {"Left", "left"},
    {"Right", "right"},
    {"PageDown", "pgdn"},
    {"PageUp", "pgup"},
    {"Home", "home"},
    {"End", "end"},
    {"Help", "f1"},
    {"Quit", "f10; q; shift-q; esc"},
#ifdef HAVE_CHARSET
    {"SelectCodepage", "alt-e"},
#endif
    {"Shell", "ctrl-o"},
    {NULL, NULL}
};
#endif

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
create_default_keymap_section (mc_config_t * keymap, const char *section,
                               const global_keymap_ini_t * k)
{
    size_t i;

    for (i = 0; k[i].key != NULL; i++)
        mc_config_set_string_raw (keymap, section, k[i].key, k[i].value);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

mc_config_t *
create_default_keymap (void)
{
    mc_config_t *keymap;

    keymap = mc_config_init (NULL, TRUE);

    create_default_keymap_section (keymap, KEYMAP_SECTION_FILEMANAGER, default_filemanager_keymap);
    create_default_keymap_section (keymap, KEYMAP_SECTION_FILEMANAGER_EXT,
                                   default_filemanager_x_keymap);
    create_default_keymap_section (keymap, KEYMAP_SECTION_PANEL, default_panel_keymap);
    create_default_keymap_section (keymap, KEYMAP_SECTION_DIALOG, default_dialog_keymap);
    create_default_keymap_section (keymap, KEYMAP_SECTION_MENU, default_menu_keymap);
    create_default_keymap_section (keymap, KEYMAP_SECTION_INPUT, default_input_keymap);
    create_default_keymap_section (keymap, KEYMAP_SECTION_LISTBOX, default_listbox_keymap);
    create_default_keymap_section (keymap, KEYMAP_SECTION_RADIO, default_radio_keymap);
    create_default_keymap_section (keymap, KEYMAP_SECTION_TREE, default_tree_keymap);
    create_default_keymap_section (keymap, KEYMAP_SECTION_HELP, default_help_keymap);
#ifdef ENABLE_EXT2FS_ATTR
    create_default_keymap_section (keymap, KEYMAP_SECTION_HELP, default_chattr_keymap);
#endif
#ifdef USE_INTERNAL_EDIT
    create_default_keymap_section (keymap, KEYMAP_SECTION_EDITOR, default_editor_keymap);
    create_default_keymap_section (keymap, KEYMAP_SECTION_EDITOR_EXT, default_editor_x_keymap);
#endif
    create_default_keymap_section (keymap, KEYMAP_SECTION_VIEWER, default_viewer_keymap);
    create_default_keymap_section (keymap, KEYMAP_SECTION_VIEWER_HEX, default_viewer_hex_keymap);
#ifdef  USE_DIFF_VIEW
    create_default_keymap_section (keymap, KEYMAP_SECTION_DIFFVIEWER, default_diff_keymap);
#endif

    return keymap;
}

/*
   Copyright (C) 2009 The Free Software Foundation, Inc.

   Written by:    2005        Vitja Makarov
   2009        Ilia Maslakov

   This file is part of the Midnight Commander.

   The Midnight Commander is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.
 */

#include <config.h>

#include "lib/global.h"
#include "lib/tty/key.h"

#include "keybind-defaults.h"

/*** global variables ****************************************************************************/

#ifdef USE_INTERNAL_EDIT
GArray *editor_keymap = NULL;
GArray *editor_x_keymap = NULL;

const global_keymap_t *editor_map;
const global_keymap_t *editor_x_map;
#endif

GArray *main_keymap = NULL;
GArray *main_x_keymap = NULL;
GArray *viewer_keymap = NULL;
GArray *viewer_hex_keymap = NULL;
GArray *panel_keymap = NULL;
GArray *input_keymap = NULL;
GArray *listbox_keymap = NULL;
GArray *tree_keymap = NULL;
GArray *help_keymap = NULL;
GArray *dialog_keymap = NULL;

#ifdef USE_DIFF_VIEW
GArray *diff_keymap = NULL;

const global_keymap_t *diff_map;
#endif

const global_keymap_t *main_map;
const global_keymap_t *main_x_map;
const global_keymap_t *panel_map;
const global_keymap_t *input_map;
const global_keymap_t *listbox_map;
const global_keymap_t *tree_map;
const global_keymap_t *help_map;
const global_keymap_t *dialog_map;

/*** global variables ****************************************************************************/

/* viewer/actions_cmd.c */
const global_keymap_t default_viewer_keymap[] = {
    {KEY_F (1), CK_ViewHelp, "F1"},
    {KEY_F (2), CK_ViewToggleWrapMode, "F2"},
    {KEY_F (3), CK_ViewQuit, "F3"},
    {KEY_F (4), CK_ViewToggleHexMode, "F4"},
    {KEY_F (5), CK_ViewGoto, "F5"},
    {KEY_F (7), CK_ViewSearch, "F7"},
    {KEY_F (8), CK_ViewToggleMagicMode, "F8"},
    {KEY_F (9), CK_ViewToggleNroffMode, "F9"},
    {KEY_F (10), CK_ViewQuit, "F10"},

    {'?', CK_ViewSearch, "?"},
    {'/', CK_ViewSearch, "/"},
    {XCTRL ('r'), CK_ViewContinueSearch, "C-r"},
    {XCTRL ('s'), CK_ViewContinueSearch, "C-s"},
    {KEY_F (17), CK_ViewContinueSearch, "S-F7"},
    {'n', CK_ViewContinueSearch, "n"},
    {ALT ('r'), CK_ViewToggleRuler, "M-r"},

    {XCTRL ('a'), CK_ViewMoveToBol, "C-a"},
    {XCTRL ('e'), CK_ViewMoveToEol, "C-e"},

    {'h', CK_ViewMoveLeft, "h"},
    {KEY_LEFT, CK_ViewMoveLeft, "Left"},

    {'l', CK_ViewMoveRight, "l"},
    {KEY_RIGHT, CK_ViewMoveRight, "Right"},

    {KEY_M_CTRL | KEY_LEFT, CK_ViewMoveLeft10, "C-Left"},
    {KEY_M_CTRL | KEY_RIGHT, CK_ViewMoveRight10, "C-Right"},

    {'k', CK_ViewMoveUp, "k"},
    {'y', CK_ViewMoveUp, "y"},
    {XCTRL ('p'), CK_ViewMoveUp, "C-p"},
    {KEY_IC, CK_ViewMoveUp, "Insert"},
    {KEY_UP, CK_ViewMoveUp, "Up"},

    {'j', CK_ViewMoveDown, "j"},
    {'e', CK_ViewMoveDown, "e"},
    {XCTRL ('n'), CK_ViewMoveUp, "C-n"},
    {KEY_DOWN, CK_ViewMoveDown, "Down"},
    {KEY_DC, CK_ViewMoveDown, "Delete"},
    {'\n', CK_ViewMoveDown, "Endter"},

    {' ', CK_ViewMovePgDn, "Space"},
    {'f', CK_ViewMovePgDn, "f"},
    {KEY_NPAGE, CK_ViewMovePgDn, "PgDn"},
    {XCTRL ('v'), CK_ViewMovePgDn, "C-v"},

    {'b', CK_ViewMovePgUp, "b"},
    {KEY_PPAGE, CK_ViewMovePgUp, "PgUp"},
    {ALT ('v'), CK_ViewMovePgUp, "M-v"},
    {KEY_BACKSPACE, CK_ViewMovePgUp, "BackSpace"},

    {'d', CK_ViewMoveHalfPgDn, "d"},
    {'u', CK_ViewMoveHalfPgUp, "u"},

    {KEY_HOME, CK_ViewMoveTop, "Home"},
    {KEY_M_CTRL | KEY_HOME, CK_ViewMoveTop, "C-Home"},
    {KEY_M_CTRL | KEY_PPAGE, CK_ViewMoveTop, "C-PgUp"},
    {KEY_A1, CK_ViewMoveTop, "A1"},
    {ALT ('<'), CK_ViewMoveTop, "M-<"},
    {'g', CK_ViewMoveTop, "g"},

    {KEY_END, CK_ViewMoveBottom, "End"},
    {KEY_M_CTRL | KEY_END, CK_ViewMoveBottom, "C-End"},
    {KEY_M_CTRL | KEY_NPAGE, CK_ViewMoveBottom, "C-PgDn"},
    {KEY_C1, CK_ViewMoveBottom, "C1"},
    {ALT ('>'), CK_ViewMoveBottom, "M->"},
    {'G', CK_ViewMoveBottom, "G"},

    {'m', CK_ViewGotoBookmark, "m"},
    {'r', CK_ViewNewBookmark, "r"},

    {XCTRL ('f'), CK_ViewNextFile, "C-f"},
    {XCTRL ('b'), CK_ViewPrevFile, "C-b"},

    {'q', CK_ViewQuit, "q"},
    {ESC_CHAR, CK_ViewQuit, "Esc"},

    {ALT ('e'), CK_SelectCodepage, "M-e"},
    {XCTRL ('o'), CK_ShowCommandLine, "C-o"},

    {0, CK_Ignore_Key, ""}
};

const global_keymap_t default_viewer_hex_keymap[] = {
    {KEY_F (1), CK_ViewHelp, "F1"},
    {KEY_F (2), CK_ViewToggleHexEditMode, "F2"},
    {KEY_F (3), CK_ViewQuit, "F3"},
    {KEY_F (4), CK_ViewToggleHexMode, "F4"},
    {KEY_F (5), CK_ViewGoto, "F5"},
    {KEY_F (6), CK_ViewHexEditSave, "F6"},
    {KEY_F (7), CK_ViewSearch, "F7"},
    {KEY_F (8), CK_ViewToggleMagicMode, "F8"},
    {KEY_F (9), CK_ViewToggleNroffMode, "F9"},
    {KEY_F (10), CK_ViewQuit, "F10"},

    {'?', CK_ViewSearch, "?"},
    {'/', CK_ViewSearch, "/"},
    {XCTRL ('r'), CK_ViewContinueSearch, "C-r"},
    {XCTRL ('s'), CK_ViewContinueSearch, "C-s"},
    {KEY_F (17), CK_ViewContinueSearch, "S-F7"},
    {'n', CK_ViewContinueSearch, "n"},

    {'\t', CK_ViewToggleHexNavMode, "Tab"},
    {XCTRL ('a'), CK_ViewMoveToBol, "C-a"},
    {XCTRL ('e'), CK_ViewMoveToEol, "C-e"},

    {'b', CK_ViewMoveLeft, "b"},
    {KEY_LEFT, CK_ViewMoveLeft, "Left"},

    {'f', CK_ViewMoveRight, "f"},
    {KEY_RIGHT, CK_ViewMoveRight, "Right"},

    {'k', CK_ViewMoveUp, "k"},
    {'y', CK_ViewMoveUp, "y"},
    {KEY_UP, CK_ViewMoveUp, "Up"},

    {'j', CK_ViewMoveDown, "j"},
    {KEY_DOWN, CK_ViewMoveDown, "Down"},
    {KEY_DC, CK_ViewMoveDown, "Delete"},

    {KEY_NPAGE, CK_ViewMovePgDn, "PgDn"},
    {XCTRL ('v'), CK_ViewMovePgDn, "C-v"},

    {KEY_PPAGE, CK_ViewMovePgUp, "PgUp"},
    {ALT ('v'), CK_ViewMovePgUp, "M-v"},

    {KEY_HOME, CK_ViewMoveTop, "Home"},
    {KEY_M_CTRL | KEY_HOME, CK_ViewMoveTop, "C-Home"},
    {KEY_M_CTRL | KEY_PPAGE, CK_ViewMoveTop, "C-PgUp"},
    {KEY_A1, CK_ViewMoveTop, "A1"},
    {ALT ('<'), CK_ViewMoveTop, "M-<"},
    {'g', CK_ViewMoveTop, "g"},

    {KEY_END, CK_ViewMoveBottom, "End"},
    {KEY_M_CTRL | KEY_END, CK_ViewMoveBottom, "C-End"},
    {KEY_M_CTRL | KEY_NPAGE, CK_ViewMoveBottom, "C-PgDn"},
    {KEY_C1, CK_ViewMoveBottom, "C1"},
    {ALT ('>'), CK_ViewMoveBottom, "M->"},
    {'G', CK_ViewMoveBottom, "G"},

    {'q', CK_ViewQuit, "q"},
    {ESC_CHAR, CK_ViewQuit, "Esc"},

    {ALT ('e'), CK_SelectCodepage, "M-e"},
    {XCTRL ('o'), CK_ShowCommandLine, "C-o"},

    {0, CK_Ignore_Key, ""}
};

#ifdef USE_INTERNAL_EDIT
/* ../edit/editkeys.c */
const global_keymap_t default_editor_keymap[] = {
    {'\n', CK_Enter, "Enter"},
    {'\t', CK_Tab, "Tab"},

    {KEY_F (1), CK_Help, "F1"},
    {KEY_F (2), CK_Save, "F2"},
    {KEY_F (3), CK_Mark, "F3"},
    {KEY_F (4), CK_Replace, "F4"},
    {KEY_F (5), CK_Copy, "F5"},
    {KEY_F (6), CK_Move, "F6"},
    {KEY_F (7), CK_Find, "F7"},
    {KEY_F (8), CK_Remove, "F8"},
    {KEY_F (9), CK_Menu, "F9"},
    {KEY_F (10), CK_Quit, "F10"},
    /* edit user menu */
    {KEY_F (11), CK_User_Menu, "S-F1"},
    {KEY_F (12), CK_Save_As, "S-F2"},
    {KEY_F (13), CK_Column_Mark, "S-F3"},
    {KEY_F (14), CK_Replace_Again, "S-F4"},
    {KEY_F (15), CK_Insert_File, "S-F5"},
    {KEY_F (17), CK_Find_Again, "S-F7"},
    /* C formatter */
    {KEY_F (19), CK_Pipe_Block (0), "S-F9"},

    {ESC_CHAR, CK_Quit, "Esc"},
    {KEY_BACKSPACE, CK_BackSpace, "BackSpace"},
    {KEY_BACKSPACE, CK_BackSpace, "C-h"},
    {KEY_DC, CK_Delete, "Delete"},
    {KEY_DC, CK_Delete, "C-d"},
    {KEY_DOWN, CK_Down, "Down"},
    {KEY_END, CK_End, "End"},
    {KEY_HOME, CK_Home, "Home"},
    {KEY_IC, CK_Toggle_Insert, "Insert"},
    {KEY_LEFT, CK_Left, "Left"},
    {KEY_NPAGE, CK_Page_Down, "PgDn"},
    {KEY_PPAGE, CK_Page_Up, "PgUp"},
    {KEY_RIGHT, CK_Right, "Right"},
    {KEY_UP, CK_Up, "Up"},

    /* Ctrl */
    {KEY_M_CTRL | (KEY_F (2)), CK_Save_As, "C-F2"},
    {KEY_M_CTRL | (KEY_F (4)), CK_Replace_Again, "C-F4"},
    {KEY_M_CTRL | (KEY_F (7)), CK_Find_Again, "C-F7"},
    {KEY_M_CTRL | KEY_BACKSPACE, CK_Undo, "C-BackSpace"},
    {KEY_M_CTRL | KEY_NPAGE, CK_End_Of_Text, "C-PgDn"},
    {KEY_M_CTRL | KEY_PPAGE, CK_Beginning_Of_Text, "C-PgUp"},
    {KEY_M_CTRL | KEY_HOME, CK_Beginning_Of_Text, "C-Home"},
    {KEY_M_CTRL | KEY_END, CK_End_Of_Text, "C-End"},
    {KEY_M_CTRL | KEY_UP, CK_Scroll_Up, "C-Up"},
    {KEY_M_CTRL | KEY_DOWN, CK_Scroll_Down, "C-Down"},
    {KEY_M_CTRL | KEY_LEFT, CK_Word_Left, "C-Left"},
    {XCTRL ('z'), CK_Word_Left, "C-z"},
    {KEY_M_CTRL | KEY_RIGHT, CK_Word_Right, "C-Right"},
    {XCTRL ('x'), CK_Word_Right, "C-x"},
    {KEY_M_CTRL | KEY_IC, CK_XStore, "C-Insert"},
    {KEY_M_CTRL | KEY_DC, CK_Remove, "C-Delete"},

    {XCTRL ('n'), CK_New, "C-n"},
    {XCTRL ('k'), CK_Delete_To_Line_End, "C-k"},
    {XCTRL ('l'), CK_Refresh, "C-l"},
    {XCTRL ('o'), CK_Shell, "C-o"},
    {XCTRL ('s'), CK_Toggle_Syntax, "C-s"},
    {XCTRL ('u'), CK_Undo, "C-u"},
    {ALT ('e'), CK_SelectCodepage, "M-e"},
    {XCTRL ('q'), CK_Insert_Literal, "C-q"},
    {XCTRL ('r'), CK_Begin_End_Macro, "C-r"},
    {XCTRL ('r'), CK_Begin_Record_Macro, "C-r"},
    {XCTRL ('r'), CK_End_Record_Macro, "C-r"},
    {XCTRL ('a'), CK_Execute_Macro, "C-a"},
    {XCTRL ('f'), CK_Save_Block, "C-f"},
    /* Spell check */
    {XCTRL ('p'), CK_Pipe_Block (1), "C-p"},
    {XCTRL ('y'), CK_Delete_Line, "C-y"},

    /* Shift */
    {KEY_M_SHIFT | KEY_NPAGE, CK_Page_Down_Highlight, "S-PgDn"},
    {KEY_M_SHIFT | KEY_PPAGE, CK_Page_Up_Highlight, "S-PgUp"},
    {KEY_M_SHIFT | KEY_LEFT, CK_Left_Highlight, "S-Left"},
    {KEY_M_SHIFT | KEY_RIGHT, CK_Right_Highlight, "S-Right"},
    {KEY_M_SHIFT | KEY_UP, CK_Up_Highlight, "S-Up"},
    {KEY_M_SHIFT | KEY_DOWN, CK_Down_Highlight, "S-Down"},
    {KEY_M_SHIFT | KEY_HOME, CK_Home_Highlight, "S-Home"},
    {KEY_M_SHIFT | KEY_END, CK_End_Highlight, "S-End"},
    {KEY_M_SHIFT | KEY_IC, CK_XPaste, "S-Insert"},
    {KEY_M_SHIFT | KEY_DC, CK_XCut, "S-Delete"},
    /* useful for pasting multiline text */
    {KEY_M_SHIFT | '\n', CK_Return, "S-Enter"},

    /* Ctrl + Shift */
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_NPAGE, CK_End_Of_Text_Highlight, "C-S-PgDn"},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_PPAGE, CK_Beginning_Of_Text_Highlight, "C-S-PgUp"},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT, CK_Word_Left_Highlight, "C-S-Left"},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, CK_Word_Right_Highlight, "C-S-Right"},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_UP, CK_Scroll_Up_Highlight, "C-S-Up"},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN, CK_Scroll_Down_Highlight, "C-S-Down"},

    /* Alt */
    {KEY_M_ALT | KEY_NPAGE, CK_Page_Down_Alt_Highlight, "M-PgDn"},
    {KEY_M_ALT | KEY_PPAGE, CK_Page_Up_Alt_Highlight, "M-PgUp"},
    {KEY_M_ALT | KEY_LEFT, CK_Left_Alt_Highlight, "M-Left"},
    {KEY_M_ALT | KEY_RIGHT, CK_Right_Alt_Highlight, "M-Right"},
    {KEY_M_ALT | KEY_UP, CK_Up_Alt_Highlight, "M-Up"},
    {KEY_M_ALT | KEY_DOWN, CK_Down_Alt_Highlight, "M-Down"},
    {KEY_M_ALT | KEY_HOME, CK_Home_Highlight, "M-Home"},
    {KEY_M_ALT | KEY_END, CK_End_Alt_Highlight, "M-End"},

    {ALT ('\n'), CK_Find_Definition, "M-Enter"},
    {ALT ('\t'), CK_Complete_Word, "M-Tab"},
    {ALT ('l'), CK_Goto, "M-l"},
    {ALT ('L'), CK_Goto, "M-L"},
    {ALT ('p'), CK_Paragraph_Format, "M-p"},
    {ALT ('t'), CK_Sort, "M-t"},
    {ALT ('u'), CK_ExtCmd, "M-u"},
    {ALT ('<'), CK_Beginning_Of_Text, "M-<"},
    {ALT ('>'), CK_End_Of_Text, "M->"},
    {ALT ('-'), CK_Load_Prev_File, "M--"},
    {ALT ('+'), CK_Load_Next_File, "M-+"},
    {ALT ('d'), CK_Delete_Word_Right, "M-d"},
    {ALT (KEY_BACKSPACE), CK_Delete_Word_Left, "M-BackSpace"},
    {ALT ('n'), CK_Toggle_Line_State, "M-n"},
    {ALT ('_'), CK_Toggle_Tab_TWS, "M-_"},
    {ALT ('k'), CK_Toggle_Bookmark, "M-k"},
    {ALT ('i'), CK_Prev_Bookmark, "M-i"},
    {ALT ('j'), CK_Next_Bookmark, "M-j"},
    {ALT ('o'), CK_Flush_Bookmarks, "M-o"},
    {ALT ('b'), CK_Match_Bracket, "M-b"},
    {ALT ('m'), CK_Mail, "M-m"},

    {XCTRL ('x'), CK_Ext_Mode, "C-x"},

    {0, CK_Ignore_Key, ""}
};

/* emacs keyboard layout emulation */
const global_keymap_t default_editor_x_keymap[] = {
    {'k', CK_New, "k"},
    {'e', CK_Execute_Macro, "e"},
    {0, CK_Ignore_Key, ""}
};
#endif /* USE_INTERNAL_EDIT */

/* dialog */
const global_keymap_t default_dialog_keymap[] = {
    {'\n', CK_DialogOK, "Enter"},
    {KEY_ENTER, CK_DialogOK, "Enter"},
    {ESC_CHAR, CK_DialogCancel, "Esc"},
    {XCTRL ('g'), CK_DialogCancel, "C-g"},
    {KEY_F (10), CK_DialogCancel, "F10"},
    {KEY_LEFT, CK_DialogPrevItem, "Left"},
    {KEY_UP, CK_DialogPrevItem, "Up"},
    {KEY_RIGHT, CK_DialogNextItem, "Right"},
    {KEY_DOWN, CK_DialogNextItem, "Down"},
    {KEY_F (1), CK_DialogHelp, "F1"},
    {XCTRL ('z'), CK_DialogSuspend, "C-z"},
    {XCTRL ('l'), CK_DialogRefresh, "C-l"},
    {0, CK_Ignore_Key, ""}
};

/* tree */
const global_keymap_t default_tree_keymap[] = {
    {KEY_F (1), CK_TreeHelp, "F1"},
    {KEY_F (2), CK_TreeRescan, "F2"},
    {KEY_F (3), CK_TreeForget, "F3"},
    {KEY_F (4), CK_TreeToggleNav, "F4"},
    {KEY_F (5), CK_TreeCopy, "F5"},
    {KEY_F (6), CK_TreeMove, "F6"},
#if 0
    {KEY_F (7), CK_TreeMake, "F7"},
#endif
    {KEY_F (8), CK_TreeRemove, "F8"},
    {KEY_UP, CK_TreeMoveUp, "Up"},
    {XCTRL ('p'), CK_TreeMoveUp, "C-p"},
    {KEY_DOWN, CK_TreeMoveDown, "Down"},
    {XCTRL ('n'), CK_TreeMoveDown, "C-n"},
    {KEY_LEFT, CK_TreeMoveLeft, "Left"},
    {KEY_RIGHT, CK_TreeMoveRight, "Right"},
    {KEY_HOME, CK_TreeMoveHome, "Home"},
    {ALT ('<'), CK_TreeMoveHome, "M-<"},
    {KEY_A1, CK_TreeMoveHome, "A1"},
    {KEY_END, CK_TreeMoveEnd, "End"},
    {ALT ('>'), CK_TreeMoveEnd, "M->"},
    {KEY_C1, CK_TreeMoveEnd, "C1"},
    {KEY_PPAGE, CK_TreeMovePgUp, "PgUp"},
    {ALT ('v'), CK_TreeMovePgUp, "M-v"},
    {KEY_NPAGE, CK_TreeMovePgDn, "PnDn"},
    {XCTRL ('v'), CK_TreeMovePgDn, "C-v"},
    {'\n', CK_TreeOpen, "Enter"},
    {KEY_ENTER, CK_TreeOpen, "Enter"},
    {XCTRL ('r'), CK_TreeRescan, "C-r"},
    {XCTRL ('s'), CK_TreeStartSearch, "C-s"},
    {ALT ('s'), CK_TreeStartSearch, "M-s"},
    {KEY_DC, CK_TreeRemove, "Delete"},
    {0, CK_Ignore_Key, ""}
};

/* help */
const global_keymap_t default_help_keymap[] = {
    {KEY_F (1), CK_HelpHelp, "F1"},
    {KEY_F (2), CK_HelpIndex, "F2"},
    {KEY_F (3), CK_HelpBack, "F3"},
    {KEY_F (10), CK_HelpQuit, "F10"},
    {KEY_LEFT, CK_HelpBack, "Left"},
    {'l', CK_HelpBack, "l"},
    {KEY_DOWN, CK_HelpMoveDown, "Down"},
    {XCTRL ('n'), CK_HelpMoveDown, "C-n"},
    {KEY_UP, CK_HelpMoveUp, "Up"},
    {XCTRL ('p'), CK_HelpMoveUp, "C-p"},
    {KEY_NPAGE, CK_HelpMovePgDn, "PgDn"},
    {XCTRL ('v'), CK_HelpMovePgDn, "C-v"},
    {'f', CK_HelpMovePgDn, "f"},
    {' ', CK_HelpMovePgDn, "Space"},
    {KEY_PPAGE, CK_HelpMovePgUp, "PgUp"},
    {ALT ('v'), CK_HelpMovePgUp, "M-v"},
    {'b', CK_HelpMovePgUp, "b"},
    {KEY_BACKSPACE, CK_HelpMovePgUp, "BackSpace"},
    {'d', CK_HelpMoveHalfPgDn, "d"},
    {'u', CK_HelpMoveHalfPgUp, "u"},
    {KEY_HOME, CK_HelpMoveTop, "Home"},
    {KEY_M_CTRL | KEY_HOME, CK_HelpMoveTop, "C-Home"},
    {KEY_M_CTRL | KEY_PPAGE, CK_HelpMoveTop, "C-PgUp"},
    {KEY_A1, CK_HelpMoveTop, "A1"},
    {ALT ('<'), CK_HelpMoveTop, "M-<"},
    {'g', CK_HelpMoveTop, "g"},
    {KEY_END, CK_HelpMoveBottom, "End"},
    {KEY_M_CTRL | KEY_END, CK_HelpMoveBottom, "C-End"},
    {KEY_M_CTRL | KEY_NPAGE, CK_HelpMoveBottom, "C-PgDn"},
    {KEY_C1, CK_HelpMoveBottom, "C1"},
    {ALT ('>'), CK_HelpMoveBottom, "M->"},
    {'G', CK_HelpMoveBottom, "G"},
    {KEY_RIGHT, CK_HelpSelectLink, "Right"},
    {KEY_ENTER, CK_HelpSelectLink, "Enter"},
    {'\n', CK_HelpSelectLink, "Enter"},
    {'\t', CK_HelpNextLink, "Tab"},
    {ALT ('\t'), CK_HelpPrevLink, "M-Tab"},
    {'n', CK_HelpNextNode, "n"},
    {'p', CK_HelpPrevNode, "p"},
    {ESC_CHAR, CK_HelpQuit, "Esc"},
    {0, CK_Ignore_Key, ""}
};

/* panel */
const global_keymap_t default_panel_keymap[] = {
    {ALT ('o'), CK_PanelChdirOtherPanel, "M-o"},
    {ALT ('l'), CK_PanelChdirToReadlink, "M-l"},
    {KEY_F (15), CK_PanelCmdCopyLocal, "S-F5"},
    {KEY_F (18), CK_PanelCmdDeleteLocal, "S-F8"},
    {KEY_ENTER, CK_PanelCmdDoEnter, "Enter"},
    {'\n', CK_PanelCmdDoEnter, "Enter"},
    {KEY_F (14), CK_PanelCmdEditNew, "S-F4"},
    {KEY_F (16), CK_PanelCmdRenameLocal, "S-F6"},
    {ALT ('*'), CK_PanelCmdReverseSelection, "M-*"},
    {KEY_KP_ADD, CK_PanelCmdSelect, "M-+"},
    {KEY_KP_SUBTRACT, CK_PanelCmdUnselect, "M--"},
    {KEY_F (13), CK_PanelCmdViewSimple, "S-F3"},
    {KEY_M_CTRL | KEY_NPAGE, CK_PanelGotoChildDir, "C-PgDn"},
    {KEY_M_CTRL | KEY_PPAGE, CK_PanelGotoParentDir, "C-PgUp"},
    {ALT ('H'), CK_PanelDirectoryHistoryList, "M-H"},
    {ALT ('u'), CK_PanelDirectoryHistoryNext, "M-u"},
    {ALT ('y'), CK_PanelDirectoryHistoryPrev, "M-y"},
    {ALT ('j'), CK_PanelGotoBottomFile, "M-j"},
    {ALT ('r'), CK_PanelGotoMiddleFile, "M-r"},
    {ALT ('g'), CK_PanelGotoTopFile, "M-g"},
    {KEY_IC, CK_PanelMarkFile, "Insert"},
    {KEY_UP, CK_PanelMoveUp, "Up"},
    {KEY_DOWN, CK_PanelMoveDown, "Down"},
    {KEY_LEFT, CK_PanelMoveLeft, "Left"},
    {KEY_RIGHT, CK_PanelMoveRight, "Right"},
    {KEY_END, CK_PanelMoveEnd, "End"},
    {KEY_C1, CK_PanelMoveEnd, "C1"},
    {KEY_HOME, CK_PanelMoveHome, "Home"},
    {KEY_A1, CK_PanelMoveHome, "A1"},
    {KEY_NPAGE, CK_PanelNextPage, "PgDn"},
    {KEY_PPAGE, CK_PanelPrevPage, "PgUp"},
    {ALT ('e'), CK_PanelSetPanelEncoding, "M-e"},
    {XCTRL ('s'), CK_PanelStartSearch, "C-s"},
    {ALT ('s'), CK_PanelStartSearch, "M-s"},
    {ALT ('i'), CK_PanelSyncOtherPanel, "M-i"},
    {0, CK_Ignore_Key, ""}
};

/* main.c */
const global_keymap_t default_main_map[] = {
    {KEY_F (1), CK_HelpCmd, "F1"},
    {KEY_F (2), CK_UserMenuCmd, "F2"},
    {KEY_F (3), CK_ViewCmd, "F3"},
    {KEY_F (4), CK_EditCmd, "F4"},
    {KEY_F (5), CK_CopyCmd, "F5"},
    {KEY_F (6), CK_RenameCmd, "F6"},
    {KEY_F (7), CK_MkdirCmd, "F7"},
    {KEY_F (8), CK_DeleteCmd, "F8"},
    {KEY_F (9), CK_MenuCmd, "F9"},
    {KEY_F (10), CK_QuitCmd, "F10"},
    {KEY_F (13), CK_ViewFileCmd, "S-F3"},
    {KEY_F (19), CK_MenuLastSelectedCmd, "S-F9"},
    {KEY_F (20), CK_QuietQuitCmd, "S-10"},
    {ALT ('h'), CK_HistoryCmd, "M-h"},
    {XCTRL ('@'), CK_SingleDirsizeCmd, "C-Space"},
    /* Copy useful information to the command line */
    {ALT ('a'), CK_CopyCurrentPathname, "M-a"},
    {ALT ('A'), CK_CopyOtherPathname, "M-A"},
    {ALT ('c'), CK_QuickCdCmd, "M-c"},
    /* To access the directory hotlist */
    {XCTRL ('\\'), CK_QuickChdirCmd, "C-\\"},
    /* Suspend */
    {XCTRL ('z'), CK_SuspendCmd, "C-z"},
    /* The filtered view command */
    {ALT ('!'), CK_FilteredViewCmd, "M-!"},
    /* Find file */
    {ALT ('?'), CK_FindCmd, "M-?"},
    /* Panel refresh */
    {XCTRL ('r'), CK_RereadCmd, "C-r"},
    /* Toggle listing between long, user defined and full formats */
    {ALT ('t'), CK_ToggleListingCmd, "M-t"},
    /* Swap panels */
    {XCTRL ('u'), CK_SwapCmd, "C-u"},
    /* View output */
    {XCTRL ('o'), CK_ShowCommandLine, "C-o"},
    {ALT ('.'), CK_ToggleShowHidden, "M-."},
    {ALT (','), CK_TogglePanelsSplit, "M-,"},
    {XCTRL ('x'), CK_StartExtMap1, "C-x"},
    /* Select/unselect group */
    {KEY_KP_ADD, CK_SelectCmd, "+"},
    {KEY_KP_SUBTRACT, CK_UnselectCmd, "-"},
    {ALT ('*'), CK_ReverseSelectionCmd, "*"},

    {ALT ('`'), CK_DialogListCmd, "M-`"},
    {ALT ('}'), CK_DialogNextCmd, "M-}"},
    {ALT ('{'), CK_DialogPrevCmd, "M-{"},

    {0, CK_Ignore_Key, ""}
};

const global_keymap_t default_main_x_map[] = {
    {'d', CK_CompareDirsCmd, "d"},
#ifdef USE_DIFF_VIEW
    {XCTRL ('d'), CK_DiffViewCmd, "C-d"},
#endif /* USE_DIFF_VIEW */
#ifdef ENABLE_VFS
    {'a', CK_ReselectVfs, "a"},
#endif /* ENABLE_VFS */
    {'p', CK_CopyCurrentPathname, "p"},
    {XCTRL ('p'), CK_CopyOtherPathname, "C-p"},
    {'t', CK_CopyCurrentTagged, "t"},
    {XCTRL ('t'), CK_CopyOtherTagged, "C-t"},
    {'c', CK_ChmodCmd, "c"},
    {'o', CK_ChownCmd, "o"},
    {'r', CK_CopyCurrentReadlink, "r"},
    {XCTRL ('r'), CK_CopyOtherReadlink, "C-r"},
    {'l', CK_LinkCmd, "l"},
    {'s', CK_SymlinkCmd, "s"},
    {'v', CK_RelativeSymlinkCmd, "v"},
    {XCTRL ('s'), CK_EditSymlinkCmd, "C-s"},
    {'i', CK_InfoCmd, "i"},
    {'q', CK_QuickViewCmd, "q"},
    {'h', CK_AddHotlist, "h"},
    {'!', CK_ExternalPanelize, "!"},
#ifdef WITH_BACKGROUND
    {'j', CK_JobsCmd, "j"},
#endif /* WITH_BACKGROUND */
    {0, CK_Ignore_Key, ""}
};

const global_keymap_t default_input_keymap[] = {
    /* Motion */
    {XCTRL ('a'), CK_InputBol, "C-a"},
    {KEY_HOME, CK_InputBol, "Home"},
    {KEY_A1, CK_InputBol, "A1"},
    {ALT ('<'), CK_InputBol, "M-<"},
    {XCTRL ('e'), CK_InputEol, "C-e"},
    {KEY_END, CK_InputEol, "End"},
    {ALT ('>'), CK_InputEol, "M->"},
    {KEY_C1, CK_InputEol, "C1"},
    {KEY_LEFT, CK_InputMoveLeft, "Left"},
    {KEY_M_CTRL | KEY_LEFT, CK_InputWordLeft, "C-Left"},
    {KEY_RIGHT, CK_InputMoveRight, "Right"},
    {KEY_M_CTRL | KEY_RIGHT, CK_InputWordRight, "C-Right"},

    {XCTRL ('b'), CK_InputBackwardChar, "C-b"},
    {ALT ('b'), CK_InputBackwardWord, "M-b"},
    {XCTRL ('f'), CK_InputForwardChar, "C-f"},
    {ALT ('f'), CK_InputForwardWord, "M-f"},

    /* Editing */
    {KEY_BACKSPACE, CK_InputBackwardDelete, "BackSpace"},
    {KEY_BACKSPACE, CK_InputBackwardDelete, "C-h"},
    {KEY_DC, CK_InputDeleteChar, "Delete"},
    {KEY_DC, CK_InputDeleteChar, "C-d"},
    {ALT ('d'), CK_InputKillWord, "M-d"},
    {ALT (KEY_BACKSPACE), CK_InputBackwardKillWord, "M-BackSpace"},

    /* Region manipulation */
    {XCTRL ('w'), CK_InputKillRegion, "C-w"},
    {ALT ('w'), CK_InputKillSave, "M-w"},
    {XCTRL ('y'), CK_InputYank, "C-y"},
    {XCTRL ('k'), CK_InputKillLine, "C-k"},

    /* History */
    {ALT ('p'), CK_InputHistoryPrev, "M-p"},
    {ALT ('n'), CK_InputHistoryNext, "M-n"},
    {ALT ('h'), CK_InputHistoryShow, "M-h"},

    /* Completion */
    {ALT ('\t'), CK_InputComplete, "M-tab"},

    {0, CK_Ignore_Key, ""}
};

const global_keymap_t default_listbox_keymap[] = {
    {KEY_UP, CK_ListboxMoveUp, "Up"},
    {XCTRL ('p'), CK_ListboxMoveUp, "C-p"},
    {KEY_DOWN, CK_ListboxMoveDown, "Down"},
    {XCTRL ('n'), CK_ListboxMoveDown, "C-n"},
    {KEY_HOME, CK_ListboxMoveHome, "Home"},
    {ALT ('<'), CK_ListboxMoveHome, "M-<"},
    {KEY_A1, CK_ListboxMoveHome, "A1"},
    {KEY_END, CK_ListboxMoveEnd, "End"},
    {ALT ('>'), CK_ListboxMoveEnd, "M->"},
    {KEY_C1, CK_ListboxMoveEnd, "C1"},
    {KEY_PPAGE, CK_ListboxMovePgUp, "PgUp"},
    {ALT ('v'), CK_ListboxMovePgUp, "M-v"},
    {KEY_NPAGE, CK_ListboxMovePgDn, "PgDn"},
    {XCTRL ('v'), CK_ListboxMovePgDn, "C-v"},
    {KEY_DC, CK_ListboxDeleteItem, "Delete"},
    {'d', CK_ListboxDeleteItem, "d"},
    {KEY_M_SHIFT | KEY_DC, CK_ListboxDeleteAll, "S-Delete"},
    {'D', CK_ListboxDeleteAll, "D"},

    {0, CK_Ignore_Key, ""}
};

#ifdef  USE_DIFF_VIEW
/* diff viewer */
const global_keymap_t default_diff_keymap[] = {

    {'s', CK_DiffDisplaySymbols, "s"},
    {'l', CK_DiffDisplayNumbers, "l"},
    {'f', CK_DiffFull, "f"},
    {'=', CK_DiffEqual, "="},
    {'>', CK_DiffSplitMore, ">"},
    {'<', CK_DiffSplitLess, "<"},
    {'2', CK_DiffSetTab2, "2"},
    {'3', CK_DiffSetTab3, "3"},
    {'4', CK_DiffSetTab4, "4"},
    {'8', CK_DiffSetTab8, "8"},
    {XCTRL ('u'), CK_DiffSwapPanel, "C-u"},
    {XCTRL ('r'), CK_DiffRedo, "C-r"},
    {XCTRL ('o'), CK_ShowCommandLine, "C-o"},
    {'n', CK_DiffNextHunk, "n"},
    {'p', CK_DiffPrevHunk, "p"},
    {'g', CK_DiffGoto, "g"},
    {'G', CK_DiffGoto, "G"},
    {KEY_M_CTRL | KEY_HOME, CK_DiffBOF, "C-Home"},
    {KEY_M_CTRL | KEY_END, CK_DiffEOF, "C-End"},
    {KEY_DOWN, CK_DiffDown, "Down"},
    {KEY_UP, CK_DiffUp, "Up"},
    {KEY_M_CTRL | KEY_LEFT, CK_DiffQuickLeft, "C-Left"},
    {KEY_M_CTRL | KEY_RIGHT, CK_DiffQuickRight, "C-Right"},
    {KEY_LEFT, CK_DiffLeft, "Left"},
    {KEY_RIGHT, CK_DiffRight, "Right"},
    {KEY_NPAGE, CK_DiffPageDown, "Down"},
    {KEY_PPAGE, CK_DiffPageUp, "Up"},
    {KEY_HOME, CK_DiffHome, "Home"},
    {KEY_END, CK_DiffEnd, "End"},
    {'q', CK_DiffQuit, "q"},
    {'Q', CK_DiffQuit, "Q"},
    {ESC_CHAR, CK_DiffQuit, "Esc"},

    {KEY_F (1), CK_DiffHelp, "F1"},
    {KEY_F (4), CK_DiffEditCurrent, "F4"},
    {KEY_F (5), CK_DiffMergeCurrentHunk, "F5"},
    {KEY_F (7), CK_DiffSearch, "F7"},
    {KEY_F (17), CK_DiffContinueSearch, "S-F7"},
    {KEY_F (9), CK_DiffOptions, "F9"},
    {KEY_F (10), CK_DiffQuit, "F10"},
    {KEY_F (14), CK_DiffEditOther, "S-F4"},

    {0, CK_Ignore_Key, ""}
};
#endif

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

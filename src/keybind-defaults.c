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

GArray *main_keymap = NULL;
GArray *main_x_keymap = NULL;
GArray *panel_keymap = NULL;
GArray *dialog_keymap = NULL;
GArray *input_keymap = NULL;
GArray *listbox_keymap = NULL;
GArray *tree_keymap = NULL;
GArray *help_keymap = NULL;
#ifdef USE_INTERNAL_EDIT
GArray *editor_keymap = NULL;
GArray *editor_x_keymap = NULL;
#endif
GArray *viewer_keymap = NULL;
GArray *viewer_hex_keymap = NULL;
#ifdef USE_DIFF_VIEW
GArray *diff_keymap = NULL;
#endif

const global_keymap_t *main_map;
const global_keymap_t *main_x_map;
const global_keymap_t *panel_map;
const global_keymap_t *dialog_map;
const global_keymap_t *input_map;
const global_keymap_t *listbox_map;
const global_keymap_t *tree_map;
const global_keymap_t *help_map;
#ifdef USE_INTERNAL_EDIT
const global_keymap_t *editor_map;
const global_keymap_t *editor_x_map;
#endif
#ifdef USE_DIFF_VIEW
const global_keymap_t *diff_map;
#endif

/*** global variables ****************************************************************************/

/* midnight */
const global_keymap_t default_main_map[] = {
    {KEY_F (1), CK_Help, "F1"},
    {KEY_F (2), CK_UserMenu, "F2"},
    {KEY_F (3), CK_View, "F3"},
    {KEY_F (4), CK_Edit, "F4"},
    {KEY_F (5), CK_Copy, "F5"},
    {KEY_F (6), CK_Move, "F6"},
    {KEY_F (7), CK_MakeDir, "F7"},
    {KEY_F (8), CK_Delete, "F8"},
    {KEY_F (9), CK_Menu, "F9"},
    {KEY_F (10), CK_Quit, "F10"},
    {KEY_F (13), CK_ViewFile, "S-F3"},
    {KEY_F (19), CK_MenuLastSelected, "S-F9"},
    {KEY_F (20), CK_QuitQuiet, "S-10"},
    {ALT ('h'), CK_History, "M-h"},
    {XCTRL ('@'), CK_DirSize, "C-Space"},
    /* Copy useful information to the command line */
    {ALT ('a'), CK_PutCurrentPath, "M-a"},
    {ALT ('A'), CK_PutOtherPath, "M-A"},
    {ALT ('c'), CK_CdQuick, "M-c"},
    /* To access the directory hotlist */
    {XCTRL ('\\'), CK_HotList, "C-\\"},
    /* Suspend */
    {XCTRL ('z'), CK_Suspend, "C-z"},
    /* The filtered view command */
    {ALT ('!'), CK_ViewFiltered, "M-!"},
    /* Find file */
    {ALT ('?'), CK_Find, "M-?"},
    /* Panel refresh */
    {XCTRL ('r'), CK_Reread, "C-r"},
    /* Switch listing between long, user defined and full formats */
    {ALT ('t'), CK_PanelListingSwitch, "M-t"},
    /* Swap panels */
    {XCTRL ('u'), CK_Swap, "C-u"},
    /* View output */
    {XCTRL ('o'), CK_Shell, "C-o"},
    {ALT ('.'), CK_ShowHidden, "M-."},
    {ALT (','), CK_SplitVertHoriz, "M-,"},
    {XCTRL ('x'), CK_ExtendedKeyMap, "C-x"},
    /* Select/unselect group */
    {KEY_KP_ADD, CK_Select, "+"},
    {KEY_KP_SUBTRACT, CK_Unselect, "-"},
    {ALT ('*'), CK_SelectInvert, "*"},

    {ALT ('`'), CK_ScreenList, "M-`"},
    {ALT ('}'), CK_ScreenNext, "M-}"},
    {ALT ('{'), CK_ScreenPrev, "M-{"},

    {0, CK_Ignore_Key, ""}
};

const global_keymap_t default_main_x_map[] = {
    {'d', CK_CompareDirs, "d"},
#ifdef USE_DIFF_VIEW
    {XCTRL ('d'), CK_CompareFiles, "C-d"},
#endif /* USE_DIFF_VIEW */
#ifdef ENABLE_VFS
    {'a', CK_VfsList, "a"},
#endif /* ENABLE_VFS */
    {'p', CK_PutCurrentPath, "p"},
    {XCTRL ('p'), CK_PutOtherPath, "C-p"},
    {'t', CK_PutCurrentTagged, "t"},
    {XCTRL ('t'), CK_PutOtherTagged, "C-t"},
    {'c', CK_ChangeMode, "c"},
    {'o', CK_ChangeOwn, "o"},
    {'r', CK_PutCurrentLink, "r"},
    {XCTRL ('r'), CK_PutOtherLink, "C-r"},
    {'l', CK_Link, "l"},
    {'s', CK_LinkSymbolic, "s"},
    {'v', CK_LinkSymbolicRelative, "v"},
    {XCTRL ('s'), CK_LinkSymbolicEdit, "C-s"},
    {'i', CK_PanelInfo, "i"},
    {'q', CK_PanelQuickView, "q"},
    {'h', CK_HotListAdd, "h"},
    {'!', CK_ExternalPanelize, "!"},
#ifdef WITH_BACKGROUND
    {'j', CK_Jobs, "j"},
#endif /* WITH_BACKGROUND */
    {0, CK_Ignore_Key, ""}
};

/* panel */
const global_keymap_t default_panel_keymap[] = {
    {ALT ('o'), CK_PanelOtherCd, "M-o"},
    {ALT ('l'), CK_PanelOtherCdLink, "M-l"},
    {KEY_F (15), CK_CopySingle, "S-F5"},
    {KEY_F (18), CK_DeleteSingle, "S-F8"},
    {KEY_ENTER, CK_Enter, "Enter"},
    {'\n', CK_Enter, "Enter"},
    {KEY_F (14), CK_EditNew, "S-F4"},
    {KEY_F (16), CK_MoveSingle, "S-F6"},
    {ALT ('*'), CK_SelectInvert, "M-*"},
    {KEY_KP_ADD, CK_Select, "M-+"},
    {KEY_KP_SUBTRACT, CK_Unselect, "M--"},
    {KEY_F (13), CK_ViewRaw, "S-F3"},
    {KEY_M_CTRL | KEY_NPAGE, CK_CdChild, "C-PgDn"},
    {KEY_M_CTRL | KEY_PPAGE, CK_CdParent, "C-PgUp"},
    {ALT ('H'), CK_History, "M-H"},
    {ALT ('u'), CK_HistoryNext, "M-u"},
    {ALT ('y'), CK_HistoryPrev, "M-y"},
    {ALT ('j'), CK_BottomOnScreen, "M-j"},
    {ALT ('r'), CK_MiddleOnScreen, "M-r"},
    {ALT ('g'), CK_TopOnScreen, "M-g"},
    {KEY_IC, CK_Mark, "Insert"},
    {KEY_M_SHIFT | KEY_UP, CK_MarkDown, "S-Down"},
    {KEY_M_SHIFT | KEY_DOWN, CK_MarkUp, "S-Up"},
    {KEY_UP, CK_Up, "Up"},
    {KEY_DOWN, CK_Down, "Down"},
    {KEY_LEFT, CK_Left, "Left"},
    {KEY_RIGHT, CK_Right, "Right"},
    {KEY_END, CK_Bottom, "End"},
    {KEY_C1, CK_Bottom, "C1"},
    {KEY_HOME, CK_Top, "Home"},
    {KEY_A1, CK_Top, "A1"},
    {KEY_NPAGE, CK_PageDown, "PgDn"},
    {KEY_PPAGE, CK_PageUp, "PgUp"},
#ifdef HAVE_CHARSET
    {ALT ('e'), CK_SelectCodepage, "M-e"},
#endif
    {XCTRL ('s'), CK_Search, "C-s"},
    {ALT ('s'), CK_Search, "M-s"},
    {ALT ('i'), CK_PanelOtherSync, "M-i"},
    {0, CK_Ignore_Key, ""}
};

/* dialog */
const global_keymap_t default_dialog_keymap[] = {
    {'\n', CK_Ok, "Enter"},
    {KEY_ENTER, CK_Ok, "Enter"},
    {ESC_CHAR, CK_Cancel, "Esc"},
    {XCTRL ('g'), CK_Cancel, "C-g"},
    {KEY_F (10), CK_Cancel, "F10"},
    {KEY_LEFT, CK_Left, "Left"},
    {KEY_UP, CK_Up, "Up"},
    {KEY_RIGHT, CK_Right, "Right"},
    {KEY_DOWN, CK_Down, "Down"},
    {KEY_F (1), CK_Help, "F1"},
    {XCTRL ('z'), CK_Suspend, "C-z"},
    {XCTRL ('l'), CK_Refresh, "C-l"},
    {0, CK_Ignore_Key, ""}
};

const global_keymap_t default_input_keymap[] = {
    /* Motion */
    {XCTRL ('a'), CK_Home, "C-a"},
    {KEY_HOME, CK_Home, "Home"},
    {KEY_A1, CK_Home, "A1"},
    {ALT ('<'), CK_Home, "M-<"},
    {XCTRL ('e'), CK_End, "C-e"},
    {KEY_END, CK_End, "End"},
    {ALT ('>'), CK_End, "M->"},
    {KEY_C1, CK_End, "C1"},
    {KEY_LEFT, CK_Left, "Left"},
    {XCTRL ('b'), CK_Left, "C-b"},
    {KEY_RIGHT, CK_Right, "Right"},
    {XCTRL ('f'), CK_Right, "C-f"},
    {KEY_M_CTRL | KEY_LEFT, CK_WordLeft, "C-Left"},
    {ALT ('b'), CK_WordLeft, "M-b"},
    {KEY_M_CTRL | KEY_RIGHT, CK_WordRight, "C-Right"},
    {ALT ('f'), CK_WordRight, "M-f"},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT, CK_MarkToWordBegin, "C-S-Left"},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, CK_MarkToWordEnd, "C-S-Right"},
    {KEY_M_SHIFT | KEY_HOME, CK_MarkToHome, "S-Home"},
    {KEY_M_SHIFT | KEY_END, CK_MarkToEnd, "S-End"},

    /* Editing */
    {KEY_BACKSPACE, CK_BackSpace, "BackSpace"},
    {KEY_BACKSPACE, CK_BackSpace, "C-h"},
    {KEY_DC, CK_Delete, "Delete"},
    {KEY_DC, CK_Delete, "C-d"},
    {ALT ('d'), CK_DeleteToWordEnd, "M-d"},
    {ALT (KEY_BACKSPACE), CK_DeleteToWordBegin, "M-BackSpace"},

    /* Region manipulation */
    {XCTRL ('w'), CK_Remove, "C-w"},
    {ALT ('w'), CK_Store, "M-w"},
    {XCTRL ('y'), CK_Yank, "C-y"},
    {XCTRL ('k'), CK_DeleteToEnd, "C-k"},

    /* History */
    {ALT ('p'), CK_HistoryPrev, "M-p"},
    {ALT ('n'), CK_HistoryNext, "M-n"},
    {ALT ('h'), CK_History, "M-h"},

    /* Completion */
    {ALT ('\t'), CK_Complete, "M-tab"},

    {0, CK_Ignore_Key, ""}
};

const global_keymap_t default_listbox_keymap[] = {
    {KEY_UP, CK_Up, "Up"},
    {XCTRL ('p'), CK_Up, "C-p"},
    {KEY_DOWN, CK_Down, "Down"},
    {XCTRL ('n'), CK_Down, "C-n"},
    {KEY_HOME, CK_Top, "Home"},
    {ALT ('<'), CK_Top, "M-<"},
    {KEY_A1, CK_Top, "A1"},
    {KEY_END, CK_Bottom, "End"},
    {ALT ('>'), CK_Bottom, "M->"},
    {KEY_C1, CK_Bottom, "C1"},
    {KEY_PPAGE, CK_PageUp, "PgUp"},
    {ALT ('v'), CK_PageUp, "M-v"},
    {KEY_NPAGE, CK_PageDown, "PgDn"},
    {XCTRL ('v'), CK_PageDown, "C-v"},
    {KEY_DC, CK_Delete, "Delete"},
    {'d', CK_Delete, "d"},
    {KEY_M_SHIFT | KEY_DC, CK_Clear, "S-Delete"},
    {'D', CK_Clear, "D"},
    {0, CK_Ignore_Key, ""}
};

/* tree */
const global_keymap_t default_tree_keymap[] = {
    {KEY_F (1), CK_Help, "F1"},
    {KEY_F (2), CK_Reread, "F2"},
    {KEY_F (3), CK_Forget, "F3"},
    {KEY_F (4), CK_ToggleNavigation, "F4"},
    {KEY_F (5), CK_Copy, "F5"},
    {KEY_F (6), CK_Move, "F6"},
#if 0
    {KEY_F (7), CK_MakeDir, "F7"},
#endif
    {KEY_F (8), CK_Delete, "F8"},
    {KEY_UP, CK_Up, "Up"},
    {XCTRL ('p'), CK_Up, "C-p"},
    {KEY_DOWN, CK_Down, "Down"},
    {XCTRL ('n'), CK_Down, "C-n"},
    {KEY_LEFT, CK_Left, "Left"},
    {KEY_RIGHT, CK_Right, "Right"},
    {KEY_HOME, CK_Top, "Home"},
    {ALT ('<'), CK_Top, "M-<"},
    {KEY_A1, CK_Top, "A1"},
    {KEY_END, CK_Bottom, "End"},
    {ALT ('>'), CK_Bottom, "M->"},
    {KEY_C1, CK_Bottom, "C1"},
    {KEY_PPAGE, CK_PageUp, "PgUp"},
    {ALT ('v'), CK_PageUp, "M-v"},
    {KEY_NPAGE, CK_PageDown, "PgDn"},
    {XCTRL ('v'), CK_PageDown, "C-v"},
    {'\n', CK_Enter, "Enter"},
    {KEY_ENTER, CK_Enter, "Enter"},
    {XCTRL ('r'), CK_Reread, "C-r"},
    {XCTRL ('s'), CK_Search, "C-s"},
    {ALT ('s'), CK_Search, "M-s"},
    {KEY_DC, CK_Delete, "Delete"},
    {0, CK_Ignore_Key, ""}
};

/* help */
const global_keymap_t default_help_keymap[] = {
    {KEY_F (1), CK_Help, "F1"},
    {KEY_F (2), CK_Index, "F2"},
    {KEY_F (3), CK_Back, "F3"},
    {KEY_F (10), CK_Quit, "F10"},
    {KEY_LEFT, CK_Back, "Left"},
    {'l', CK_Back, "l"},
    {KEY_DOWN, CK_Down, "Down"},
    {XCTRL ('n'), CK_Down, "C-n"},
    {KEY_UP, CK_Up, "Up"},
    {XCTRL ('p'), CK_Up, "C-p"},
    {KEY_NPAGE, CK_PageDown, "PgDn"},
    {XCTRL ('v'), CK_PageDown, "C-v"},
    {'f', CK_PageDown, "f"},
    {' ', CK_PageDown, "Space"},
    {KEY_PPAGE, CK_PageUp, "PgUp"},
    {ALT ('v'), CK_PageUp, "M-v"},
    {'b', CK_PageUp, "b"},
    {KEY_BACKSPACE, CK_PageUp, "BackSpace"},
    {'d', CK_HalfPageDown, "d"},
    {'u', CK_HalfPageUp, "u"},
    {KEY_HOME, CK_Top, "Home"},
    {KEY_M_CTRL | KEY_HOME, CK_Top, "C-Home"},
    {KEY_M_CTRL | KEY_PPAGE, CK_Top, "C-PgUp"},
    {KEY_A1, CK_Top, "A1"},
    {ALT ('<'), CK_Top, "M-<"},
    {'g', CK_Top, "g"},
    {KEY_END, CK_Bottom, "End"},
    {KEY_M_CTRL | KEY_END, CK_Bottom, "C-End"},
    {KEY_M_CTRL | KEY_NPAGE, CK_Bottom, "C-PgDn"},
    {KEY_C1, CK_Bottom, "C1"},
    {ALT ('>'), CK_Bottom, "M->"},
    {'G', CK_Bottom, "G"},
    {KEY_RIGHT, CK_Enter, "Right"},
    {KEY_ENTER, CK_Enter, "Enter"},
    {'\n', CK_Enter, "Enter"},
    {'\t', CK_LinkNext, "Tab"},
    {ALT ('\t'), CK_LinkPrev, "M-Tab"},
    {'n', CK_NodeNext, "n"},
    {'p', CK_NodePrev, "p"},
    {ESC_CHAR, CK_Quit, "Esc"},
    {XCTRL ('g'), CK_Quit, "C-g"},
    {0, CK_Ignore_Key, ""}
};

#ifdef USE_INTERNAL_EDIT
/* editor/editkeys.c */
const global_keymap_t default_editor_keymap[] = {
    {'\n', CK_Enter, "Enter"},
    {'\t', CK_Tab, "Tab"},

    {KEY_F (1), CK_Help, "F1"},
    {KEY_F (2), CK_Save, "F2"},
    {KEY_F (3), CK_Mark, "F3"},
    {KEY_F (4), CK_Replace, "F4"},
    {KEY_F (5), CK_Copy, "F5"},
    {KEY_F (6), CK_Move, "F6"},
    {KEY_F (7), CK_Search, "F7"},
    {KEY_F (8), CK_Remove, "F8"},
    {KEY_F (9), CK_Menu, "F9"},
    {KEY_F (10), CK_Quit, "F10"},
    /* edit user menu */
    {KEY_F (11), CK_UserMenu, "S-F1"},
    {KEY_F (12), CK_SaveAs, "S-F2"},
    {KEY_F (13), CK_MarkColumn, "S-F3"},
    {KEY_F (14), CK_ReplaceContinue, "S-F4"},
    {KEY_F (15), CK_InsertFile, "S-F5"},
    {KEY_F (17), CK_SearchContinue, "S-F7"},
    /* C formatter */
    {KEY_F (19), CK_PipeBlock (0), "S-F9"},

    {ESC_CHAR, CK_Quit, "Esc"},
    {KEY_BACKSPACE, CK_BackSpace, "BackSpace"},
    {KEY_BACKSPACE, CK_BackSpace, "C-h"},
    {KEY_DC, CK_Delete, "Delete"},
    {KEY_DC, CK_Delete, "C-d"},
    {KEY_DOWN, CK_Down, "Down"},
    {KEY_END, CK_End, "End"},
    {KEY_HOME, CK_Home, "Home"},
    {KEY_IC, CK_InsertOverwrite, "Insert"},
    {KEY_LEFT, CK_Left, "Left"},
    {KEY_NPAGE, CK_PageDown, "PgDn"},
    {KEY_PPAGE, CK_PageUp, "PgUp"},
    {KEY_RIGHT, CK_Right, "Right"},
    {KEY_UP, CK_Up, "Up"},

    /* Ctrl */
    {KEY_M_CTRL | (KEY_F (2)), CK_SaveAs, "C-F2"},
    {KEY_M_CTRL | (KEY_F (4)), CK_ReplaceContinue, "C-F4"},
    {KEY_M_CTRL | (KEY_F (7)), CK_SearchContinue, "C-F7"},
    {KEY_M_CTRL | KEY_BACKSPACE, CK_Undo, "C-BackSpace"},
    {KEY_M_CTRL | KEY_NPAGE, CK_Bottom, "C-PgDn"},
    {KEY_M_CTRL | KEY_PPAGE, CK_Top, "C-PgUp"},
    {KEY_M_CTRL | KEY_HOME, CK_Top, "C-Home"},
    {KEY_M_CTRL | KEY_END, CK_Bottom, "C-End"},
    {KEY_M_CTRL | KEY_UP, CK_ScrollUp, "C-Up"},
    {KEY_M_CTRL | KEY_DOWN, CK_ScrollDown, "C-Down"},
    {KEY_M_CTRL | KEY_LEFT, CK_WordLeft, "C-Left"},
    {XCTRL ('z'), CK_WordLeft, "C-z"},
    {KEY_M_CTRL | KEY_RIGHT, CK_WordRight, "C-Right"},
    {XCTRL ('x'), CK_WordRight, "C-x"},
    {KEY_M_CTRL | KEY_IC, CK_Store, "C-Insert"},
    {KEY_M_CTRL | KEY_DC, CK_Remove, "C-Delete"},

    {XCTRL ('n'), CK_EditNew, "C-n"},
    {XCTRL ('k'), CK_DeleteToEnd, "C-k"},
    {XCTRL ('l'), CK_Refresh, "C-l"},
    {XCTRL ('o'), CK_Shell, "C-o"},
    {XCTRL ('s'), CK_SyntaxOnOff, "C-s"},
    {XCTRL ('u'), CK_Undo, "C-u"},
    {ALT ('r'), CK_Redo, "M-r"},
#ifdef HAVE_CHARSET
    {ALT ('e'), CK_SelectCodepage, "M-e"},
#endif
    {XCTRL ('f'), CK_BlockSave, "C-f"},
    /* Spell check */
    {XCTRL ('p'), CK_PipeBlock (1), "C-p"},
    {XCTRL ('y'), CK_DeleteLine, "C-y"},

    /* Shift */
    {KEY_M_SHIFT | KEY_NPAGE, CK_MarkPageDown, "S-PgDn"},
    {KEY_M_SHIFT | KEY_PPAGE, CK_MarkPageUp, "S-PgUp"},
    {KEY_M_SHIFT | KEY_LEFT, CK_MarkLeft, "S-Left"},
    {KEY_M_SHIFT | KEY_RIGHT, CK_MarkRight, "S-Right"},
    {KEY_M_SHIFT | KEY_UP, CK_MarkUp, "S-Up"},
    {KEY_M_SHIFT | KEY_DOWN, CK_MarkDown, "S-Down"},
    {KEY_M_SHIFT | KEY_HOME, CK_MarkToHome, "S-Home"},
    {KEY_M_SHIFT | KEY_END, CK_MarkToEnd, "S-End"},
    {KEY_M_SHIFT | KEY_IC, CK_Paste, "S-Insert"},
    {KEY_M_SHIFT | KEY_DC, CK_Cut, "S-Delete"},
    /* useful for pasting multiline text */
    {KEY_M_SHIFT | '\n', CK_Return, "S-Enter"},

    /* Ctrl + Shift */
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_PPAGE, CK_MarkToFileBegin, "C-S-PgUp"},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_NPAGE, CK_MarkToFileEnd, "C-S-PgDn"},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT, CK_MarkToWordBegin, "C-S-Left"},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, CK_MarkToWordEnd, "C-S-Right"},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_UP, CK_MarkScrollUp, "C-S-Up"},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN, CK_MarkScrollDown, "C-S-Down"},

    /* column mark commands */
    {KEY_M_ALT | KEY_PPAGE, CK_MarkColumnPageUp, "M-PgUp"},
    {KEY_M_ALT | KEY_NPAGE, CK_MarkColumnPageDown, "M-PgDn"},
    {KEY_M_ALT | KEY_LEFT, CK_MarkColumnLeft, "M-Left"},
    {KEY_M_ALT | KEY_RIGHT, CK_MarkColumnRight, "M-Right"},
    {KEY_M_ALT | KEY_UP, CK_MarkColumnUp, "M-Up"},
    {KEY_M_ALT | KEY_DOWN, CK_MarkColumnDown, "M-Down"},
    {KEY_M_ALT | KEY_HOME, CK_MarkToHome, "M-Home"},

    {ALT ('\n'), CK_Find, "M-Enter"},
    {ALT ('\t'), CK_Complete, "M-Tab"},
    {ALT ('l'), CK_Goto, "M-l"},
    {ALT ('L'), CK_Goto, "M-L"},
    {ALT ('p'), CK_ParagraphFormat, "M-p"},
    {ALT ('t'), CK_Sort, "M-t"},
    {ALT ('u'), CK_ExternalCommand, "M-u"},
    {ALT ('<'), CK_Top, "M-<"},
    {ALT ('>'), CK_Bottom, "M->"},
    {ALT ('-'), CK_FilePrev, "M--"},
    {ALT ('+'), CK_FileNext, "M-+"},
    {ALT ('d'), CK_DeleteToWordEnd, "M-d"},
    {ALT (KEY_BACKSPACE), CK_DeleteToWordBegin, "M-BackSpace"},
    {ALT ('n'), CK_ShowNumbers, "M-n"},
    {ALT ('_'), CK_ShowTabTws, "M-_"},
    {ALT ('k'), CK_Bookmark, "M-k"},
    {ALT ('i'), CK_BookmarkPrev, "M-i"},
    {ALT ('j'), CK_BookmarkNext, "M-j"},
    {ALT ('o'), CK_BookmarkFlush, "M-o"},
    {ALT ('b'), CK_MatchBracket, "M-b"},
    {ALT ('m'), CK_Mail, "M-m"},

    {XCTRL ('x'), CK_ExtendedKeyMap, "C-x"},

    {0, CK_Ignore_Key, ""}
};

/* emacs keyboard layout emulation */
const global_keymap_t default_editor_x_keymap[] = {
    {'k', CK_EditNew, "k"},
    {0, CK_Ignore_Key, ""}
};
#endif /* USE_INTERNAL_EDIT */

/* viewer/actions_cmd.c */
const global_keymap_t default_viewer_keymap[] = {
    {KEY_F (1), CK_Help, "F1"},
    {KEY_F (2), CK_WrapMode, "F2"},
    {KEY_F (3), CK_Quit, "F3"},
    {KEY_F (4), CK_HexMode, "F4"},
    {KEY_F (5), CK_Goto, "F5"},
    {KEY_F (7), CK_Search, "F7"},
    {KEY_F (8), CK_MagicMode, "F8"},
    {KEY_F (9), CK_NroffMode, "F9"},
    {KEY_F (10), CK_Quit, "F10"},

    {'?', CK_Search, "?"},
    {'/', CK_Search, "/"},
    {XCTRL ('r'), CK_SearchContinue, "C-r"},
    {XCTRL ('s'), CK_SearchContinue, "C-s"},
    {KEY_F (17), CK_SearchContinue, "S-F7"},
    {'n', CK_SearchContinue, "n"},
    {ALT ('r'), CK_Ruler, "M-r"},

    {XCTRL ('a'), CK_Home, "C-a"},
    {XCTRL ('e'), CK_End, "C-e"},

    {'h', CK_Left, "h"},
    {KEY_LEFT, CK_Left, "Left"},

    {'l', CK_Right, "l"},
    {KEY_RIGHT, CK_Right, "Right"},

    {KEY_M_CTRL | KEY_LEFT, CK_LeftQuick, "C-Left"},
    {KEY_M_CTRL | KEY_RIGHT, CK_RightQuick, "C-Right"},

    {'k', CK_Up, "k"},
    {'y', CK_Up, "y"},
    {XCTRL ('p'), CK_Up, "C-p"},
    {KEY_IC, CK_Up, "Insert"},
    {KEY_UP, CK_Up, "Up"},
    {XCTRL ('n'), CK_Up, "C-n"},

    {'j', CK_Down, "j"},
    {'e', CK_Down, "e"},
    {KEY_DOWN, CK_Down, "Down"},
    {KEY_DC, CK_Down, "Delete"},
    {'\n', CK_Down, "Endter"},

    {' ', CK_PageDown, "Space"},
    {'f', CK_PageDown, "f"},
    {KEY_NPAGE, CK_PageDown, "PgDn"},
    {XCTRL ('v'), CK_PageDown, "C-v"},

    {'b', CK_PageUp, "b"},
    {KEY_PPAGE, CK_PageUp, "PgUp"},
    {ALT ('v'), CK_PageUp, "M-v"},
    {KEY_BACKSPACE, CK_PageUp, "BackSpace"},

    {'d', CK_HalfPageDown, "d"},
    {'u', CK_HalfPageUp, "u"},

    {KEY_HOME, CK_Top, "Home"},
    {KEY_M_CTRL | KEY_HOME, CK_Top, "C-Home"},
    {KEY_M_CTRL | KEY_PPAGE, CK_Top, "C-PgUp"},
    {KEY_A1, CK_Top, "A1"},
    {ALT ('<'), CK_Top, "M-<"},
    {'g', CK_Top, "g"},

    {KEY_END, CK_Bottom, "End"},
    {KEY_M_CTRL | KEY_END, CK_Bottom, "C-End"},
    {KEY_M_CTRL | KEY_NPAGE, CK_Bottom, "C-PgDn"},
    {KEY_C1, CK_Bottom, "C1"},
    {ALT ('>'), CK_Bottom, "M->"},
    {'G', CK_Bottom, "G"},

    {'m', CK_BookmarkGoto, "m"},
    {'r', CK_Bookmark, "r"},

    {XCTRL ('f'), CK_FileNext, "C-f"},
    {XCTRL ('b'), CK_FilePrev, "C-b"},

    {'q', CK_Quit, "q"},
    {XCTRL ('g'), CK_Quit, "C-g"},
    {ESC_CHAR, CK_Quit, "Esc"},

#ifdef HAVE_CHARSET
    {ALT ('e'), CK_SelectCodepage, "M-e"},
#endif
    {XCTRL ('o'), CK_Shell, "C-o"},

    {0, CK_Ignore_Key, ""}
};

const global_keymap_t default_viewer_hex_keymap[] = {
    {KEY_F (1), CK_Help, "F1"},
    {KEY_F (2), CK_HexEditMode, "F2"},
    {KEY_F (3), CK_Quit, "F3"},
    {KEY_F (4), CK_HexMode, "F4"},
    {KEY_F (5), CK_Goto, "F5"},
    {KEY_F (6), CK_Save, "F6"},
    {KEY_F (7), CK_Search, "F7"},
    {KEY_F (8), CK_MagicMode, "F8"},
    {KEY_F (9), CK_NroffMode, "F9"},
    {KEY_F (10), CK_Quit, "F10"},

    {'?', CK_Search, "?"},
    {'/', CK_Search, "/"},
    {XCTRL ('r'), CK_SearchContinue, "C-r"},
    {XCTRL ('s'), CK_SearchContinue, "C-s"},
    {KEY_F (17), CK_SearchContinue, "S-F7"},
    {'n', CK_SearchContinue, "n"},

    {'\t', CK_ToggleNavigation, "Tab"},
    {KEY_HOME, CK_Home, "Home"},
    {KEY_END, CK_End, "End"},
    {XCTRL ('a'), CK_Home, "C-a"},
    {XCTRL ('e'), CK_End, "C-e"},

    {'b', CK_Left, "b"},
    {KEY_LEFT, CK_Left, "Left"},

    {'f', CK_Right, "f"},
    {KEY_RIGHT, CK_Right, "Right"},

    {'k', CK_Up, "k"},
    {'y', CK_Up, "y"},
    {KEY_UP, CK_Up, "Up"},

    {'j', CK_Down, "j"},
    {KEY_DOWN, CK_Down, "Down"},
    {KEY_DC, CK_Down, "Delete"},

    {KEY_NPAGE, CK_PageDown, "PgDn"},
    {XCTRL ('v'), CK_PageDown, "C-v"},

    {KEY_PPAGE, CK_PageUp, "PgUp"},
    {ALT ('v'), CK_PageUp, "M-v"},

    {KEY_M_CTRL | KEY_HOME, CK_Top, "C-Home"},
    {KEY_M_CTRL | KEY_PPAGE, CK_Top, "C-PgUp"},
    {KEY_A1, CK_Top, "A1"},
    {ALT ('<'), CK_Top, "M-<"},
    {'g', CK_Top, "g"},

    {KEY_M_CTRL | KEY_END, CK_Bottom, "C-End"},
    {KEY_M_CTRL | KEY_NPAGE, CK_Bottom, "C-PgDn"},
    {KEY_C1, CK_Bottom, "C1"},
    {ALT ('>'), CK_Bottom, "M->"},
    {'G', CK_Bottom, "G"},

    {'q', CK_Quit, "q"},
    {XCTRL ('g'), CK_Quit, "C-g"},
    {ESC_CHAR, CK_Quit, "Esc"},

#ifdef HAVE_CHARSET
    {ALT ('e'), CK_SelectCodepage, "M-e"},
#endif
    {XCTRL ('o'), CK_Shell, "C-o"},

    {0, CK_Ignore_Key, ""}
};


#ifdef  USE_DIFF_VIEW
/* diff viewer */
const global_keymap_t default_diff_keymap[] = {
    {'s', CK_ShowSymbols, "s"},
    {'l', CK_ShowNumbers, "l"},
    {'f', CK_SplitFull, "f"},
    {'=', CK_SplitEqual, "="},
    {'>', CK_SplitMore, ">"},
    {'<', CK_SplitLess, "<"},
    {'2', CK_Tab2, "2"},
    {'3', CK_Tab3, "3"},
    {'4', CK_Tab4, "4"},
    {'8', CK_Tab8, "8"},
    {XCTRL ('u'), CK_Swap, "C-u"},
    {XCTRL ('r'), CK_Redo, "C-r"},
    {XCTRL ('o'), CK_Shell, "C-o"},
    {'n', CK_HunkNext, "n"},
    {'p', CK_HunkPrev, "p"},
    {'g', CK_Goto, "g"},
    {'G', CK_Goto, "G"},
    {KEY_M_CTRL | KEY_HOME, CK_Top, "C-Home"},
    {KEY_M_CTRL | KEY_END, CK_Bottom, "C-End"},
    {KEY_DOWN, CK_Down, "Down"},
    {KEY_UP, CK_Up, "Up"},
    {KEY_M_CTRL | KEY_LEFT, CK_LeftQuick, "C-Left"},
    {KEY_M_CTRL | KEY_RIGHT, CK_RightQuick, "C-Right"},
    {KEY_LEFT, CK_Left, "Left"},
    {KEY_RIGHT, CK_Right, "Right"},
    {KEY_NPAGE, CK_PageDown, "PgDn"},
    {KEY_PPAGE, CK_PageUp, "PgUp"},
    {KEY_HOME, CK_Home, "Home"},
    {KEY_END, CK_End, "End"},
    {'q', CK_Quit, "q"},
    {'Q', CK_Quit, "Q"},
    {XCTRL ('g'), CK_Quit, "C-g"},
    {ESC_CHAR, CK_Quit, "Esc"},

    {KEY_F (1), CK_Help, "F1"},
    {KEY_F (4), CK_Edit, "F4"},
    {KEY_F (5), CK_Merge, "F5"},
    {KEY_F (7), CK_Search, "F7"},
    {KEY_F (17), CK_SearchContinue, "S-F7"},
    {KEY_F (9), CK_Options, "F9"},
    {KEY_F (10), CK_Quit, "F10"},
    {KEY_F (14), CK_EditOther, "S-F4"},

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
